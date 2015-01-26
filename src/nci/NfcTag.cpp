/*
 * Copyright (C) 2014  Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Tag-reading, tag-writing operations.
 */
#include "NfcTag.h"

#include "NfcManager.h"
#include "INfcTag.h"

extern "C"
{
  #include "rw_int.h"
}

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

extern nfc_data gNat;

NfcTag::NfcTag()
 : mNumTechList(0)
 , mTimeoutTable(MAX_NUM_TECHNOLOGY)
 , mActivationState(Idle)
 , mProtocol(NFC_PROTOCOL_UNKNOWN)
 , mtT1tMaxMessageSize(0)
 , mReadCompletedStatus(NFA_STATUS_OK)
 , mLastKovioUidLen(0)
 , mNdefDetectionTimedOut(false)
{
  memset(mTechList, 0, sizeof(mTechList));
  memset(mTechHandles, 0, sizeof(mTechHandles));
  memset(mTechLibNfcTypes, 0, sizeof(mTechLibNfcTypes));
  memset(mTechParams, 0, sizeof(mTechParams));
  memset(mLastKovioUid, 0, NFC_KOVIO_MAX_LEN);
}

NfcTag& NfcTag::getInstance()
{
  static NfcTag tag;
  return tag;
}

void NfcTag::initialize(NfcManager* pNfcManager)
{
  mNfcManager = pNfcManager;

  mActivationState = Idle;
  mProtocol = NFC_PROTOCOL_UNKNOWN;
  mNumTechList = 0;
  mtT1tMaxMessageSize = 0;
  mReadCompletedStatus = NFA_STATUS_OK;
  resetTechnologies();
}

void NfcTag::abort()
{
  SyncEventGuard g(mReadCompleteEvent);
  mReadCompleteEvent.notifyOne();
}

NfcTag::ActivationState NfcTag::getActivationState()
{
  return mActivationState;
}

void NfcTag::setDeactivationState(tNFA_DEACTIVATED& deactivated)
{
  static const char fn [] = "NfcTag::setDeactivationState";
  mActivationState = Idle;
  mNdefDetectionTimedOut = false;
  if (deactivated.type == NFA_DEACTIVATE_TYPE_SLEEP)
    mActivationState = Sleep;
  ALOGD("%s: state=%u", fn, mActivationState);
}

void NfcTag::setActivationState()
{
  static const char fn [] = "NfcTag::setActivationState";
  mNdefDetectionTimedOut = false;
  mActivationState = Active;
  ALOGD("%s: state=%u", fn, mActivationState);
}

tNFC_PROTOCOL NfcTag::getProtocol()
{
  return mProtocol;
}

UINT32 TimeDiff(timespec start, timespec end)
{
  timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }

  return (temp.tv_sec * 1000) + (temp.tv_nsec / 1000000);
}

bool NfcTag::IsSameKovio(tNFA_ACTIVATED& activationData)
{
  static const char fn [] = "NfcTag::IsSameKovio";
  ALOGD("%s: enter", fn);
  tNFC_ACTIVATE_DEVT& rfDetail = activationData.activate_ntf;

  if (rfDetail.protocol != NFC_PROTOCOL_KOVIO)
    return false;

  memcpy(&(mTechParams[0]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
  if (mTechParams[0].mode != NFC_DISCOVERY_TYPE_POLL_KOVIO)
    return false;

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  bool rVal = false;
  if (mTechParams[0].param.pk.uid_len == mLastKovioUidLen) {
    if (memcmp(mLastKovioUid, &mTechParams [0].param.pk.uid, mTechParams[0].param.pk.uid_len) == 0) {
      // Same tag.
      if (TimeDiff(mLastKovioTime, now) < 500) {
        // Same tag within 500 ms, ignore activation.
        rVal = true;
      }
    }
  }

  // Save Kovio tag info.
  if (!rVal) {
    if ((mLastKovioUidLen = mTechParams[0].param.pk.uid_len) > NFC_KOVIO_MAX_LEN)
      mLastKovioUidLen = NFC_KOVIO_MAX_LEN;
    memcpy(mLastKovioUid, mTechParams[0].param.pk.uid, mLastKovioUidLen);
  }
  mLastKovioTime = now;
  ALOGD ("%s: exit, is same Kovio=%d", fn, rVal);
  return rVal;
}

void NfcTag::discoverTechnologies(tNFA_ACTIVATED& activationData)
{
  static const char fn [] = "NfcTag::discoverTechnologies (activation)";
  ALOGD("%s: enter", fn);
  tNFC_ACTIVATE_DEVT& rfDetail = activationData.activate_ntf;

  mNumTechList = 0;
  mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
  mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;

  // Save the stack's data structure for interpretation later
  memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));

  switch (rfDetail.protocol) {
    case NFC_PROTOCOL_T1T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;  // Is TagTechnology.NFC_A by Java API.
      break;

    case NFC_PROTOCOL_T2T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;  // Is TagTechnology.NFC_A by Java API.
      // Could be MifFare UL or Classic or Kovio.
      {
        // Need to look at first byte of uid to find manuf.
        tNFC_RF_TECH_PARAMS tech_params;
        memcpy(&tech_params, &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));

        if ((tech_params.param.pa.nfcid1[0] == 0x04 && rfDetail.rf_tech_param.param.pa.sel_rsp == 0) ||
               rfDetail.rf_tech_param.param.pa.sel_rsp == 0x18 ||
               rfDetail.rf_tech_param.param.pa.sel_rsp == 0x08) {
          if (rfDetail.rf_tech_param.param.pa.sel_rsp == 0) {
            mNumTechList++;
            mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
            mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;
            // Save the stack's data structure for interpretation later
            memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
            mTechList[mNumTechList] = TECHNOLOGY_TYPE_MIFARE_UL; // Is TagTechnology.MIFARE_ULTRALIGHT by Java API.
          }
        }
      }
      break;

    case NFC_PROTOCOL_T3T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_FELICA;
      break;

    case NFC_PROTOCOL_ISO_DEP: // Type-4 tag uses technology ISO-DEP and technology A or B.
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_4; // Is TagTechnology.ISO_DEP by Java API.
      if ( (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
             (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
             (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
             (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) ) {
        mNumTechList++;
        mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API.
        // Save the stack's data structure for interpretation later
        memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
      } else if ( (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                  (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                  (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                  (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME) ) {
        mNumTechList++;
        mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3B; // Is TagTechnology.NFC_B by Java API.
        // Save the stack's data structure for interpretation later.
        memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
      }
      break;

    case NFC_PROTOCOL_15693: // Is TagTechnology.NFC_V by Java API.
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO15693;
      break;

    case NFC_PROTOCOL_KOVIO:
      ALOGD("%s: Kovio", fn);
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_KOVIO_BARCODE;
      break;

    default:
      ALOGE("%s: unknown protocol ????", fn);
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_UNKNOWN;
      break;
  }

  mNumTechList++;
  for (int i=0; i < mNumTechList; i++) {
    ALOGD("%s: index=%d; tech=%d; handle=%d; nfc type=%d", fn,
            i, mTechList[i], mTechHandles[i], mTechLibNfcTypes[i]);
  }
  ALOGD("%s: exit", fn);
}

void NfcTag::discoverTechnologies(tNFA_DISC_RESULT& discoveryData)
{
  static const char fn [] = "NfcTag::discoverTechnologies (discovery)";
  tNFC_RESULT_DEVT& discovery_ntf = discoveryData.discovery_ntf;

  ALOGD("%s: enter: rf disc. id=%u; protocol=%u, mNumTechList=%u", fn, discovery_ntf.rf_disc_id, discovery_ntf.protocol, mNumTechList);
  if (mNumTechList >= MAX_NUM_TECHNOLOGY) {
    ALOGE("%s: exceed max=%d", fn, MAX_NUM_TECHNOLOGY);
    goto TheEnd;
  }
  mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
  mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;

  // Save the stack's data structure for interpretation later.
  memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));

  switch (discovery_ntf.protocol) {
    case NFC_PROTOCOL_T1T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A; // Is TagTechnology.NFC_A by Java API.
      break;

    case NFC_PROTOCOL_T2T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;  // Is TagTechnology.NFC_A by Java API.
      // Type-2 tags are identitical to Mifare Ultralight, so Ultralight is also discovered.
      if (discovery_ntf.rf_tech_param.param.pa.sel_rsp == 0) {
        // Mifare Ultralight.
        mNumTechList++;
        mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_MIFARE_UL; // Is TagTechnology.MIFARE_ULTRALIGHT by Java API.
      }

      // Save the stack's data structure for interpretation later.
      memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
      break;

    case NFC_PROTOCOL_T3T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_FELICA;
      break;

    case NFC_PROTOCOL_ISO_DEP: // Type-4 tag uses technology ISO-DEP and technology A or B.
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_4; // Is TagTechnology.ISO_DEP by Java API.
      if ( (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
           (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
           (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
           (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) ) {
        mNumTechList++;
        mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A; // Is TagTechnology.NFC_A by Java API.
        // Save the stack's data structure for interpretation later.
        memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
      } else if ( (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                  (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                  (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                  (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME) ) {
        mNumTechList++;
        mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3B; // Is TagTechnology.NFC_B by Java API
        // Save the stack's data structure for interpretation later
        memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
      }
      break;

    case NFC_PROTOCOL_15693: // Is TagTechnology.NFC_V by Java API.
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO15693;
      break;

    default:
      ALOGE("%s: unknown protocol ????", fn);
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_UNKNOWN;
      break;
  }

  mNumTechList++;
  if (discovery_ntf.more == FALSE) {
    for (int i=0; i < mNumTechList; i++) {
      ALOGD("%s: index=%d; tech=%d; handle=%d; nfc type=%d", fn,
        i, mTechList[i], mTechHandles[i], mTechLibNfcTypes[i]);
    }
  }

TheEnd:
  ALOGD("%s: exit", fn);
}

void NfcTag::createNfcTag(tNFA_ACTIVATED& activationData)
{
  static const char fn [] = "NfcTag::createNfcTag";
  ALOGD ("%s: enter", fn);

  INfcTag* pINfcTag = reinterpret_cast<INfcTag*>(mNfcManager->queryInterface(INTERFACE_TAG_MANAGER));

  if (pINfcTag == NULL) {
    ALOGE("%s : cannot get nfc tag class", fn);
    return;
  }

  // Fill NfcTag's mProtocols, mTechList, mTechHandles, mTechLibNfcTypes.
  fillNfcTagMembers1(pINfcTag);

  // Fill NfcTag's members: mHandle, mConnectedTechnology.
  fillNfcTagMembers2(pINfcTag);

  // Fill NfcTag's members: mTechPollBytes.
  fillNfcTagMembers3(pINfcTag, activationData);

  // Fill NfcTag's members: mTechActBytes.
  fillNfcTagMembers4(pINfcTag, activationData);

  // Fill NfcTag's members: mUid.
  fillNfcTagMembers5(pINfcTag, activationData);

  // Notify NFC service about this new tag.
  ALOGD("%s: try notify nfc service", fn);
  mNfcManager->notifyTagDiscovered(pINfcTag);

  ALOGD("%s: exit", fn);
}

void NfcTag::fillNfcTagMembers1(INfcTag* pINfcTag)
{
  static const char fn [] = "NfcTag::fillNfcTagMembers1";
  ALOGD("%s", fn);

  std::vector<TagTechnology>& techList = pINfcTag->getTechList();
  std::vector<int>& techHandles = pINfcTag->getTechHandles();
  std::vector<int>& techLibNfcTypes = pINfcTag->getTechLibNfcTypes();

  for (int i = 0; i < mNumTechList; i++) {
    gNat.tProtocols[i] = mTechLibNfcTypes[i];
    gNat.handles[i] = mTechHandles[i];

    // Convert from vendor specific technology definition to common tag technology definition.
    techList.push_back(NfcNciUtil::toTagTechnology(mTechList[i]));
    techHandles.push_back(mTechHandles[i]);
    techLibNfcTypes.push_back(mTechLibNfcTypes[i]);
  }
}

// Fill NfcTag's members: mHandle, mConnectedTechnology.
void NfcTag::fillNfcTagMembers2(INfcTag* pINfcTag)
{
  static const char fn [] = "NfcTag::fillNfcTagMembers2";
  ALOGD("%s", fn);

  int& connectedTechIndex = pINfcTag->getConnectedHandle();
  connectedTechIndex = 0;
}

void NfcTag::fillNfcTagMembers3(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData)
{
  static const char fn [] = "NfcTag::fillNfcTagMembers3";
  int len = 0;
  std::vector<uint8_t> pollBytes;
  std::vector<std::vector<uint8_t> >& techPollBytes = pINfcTag->getTechPollBytes();

  for (int i = 0; i < mNumTechList; i++) {
    ALOGD("%s: index=%d; rf tech params mode=%u", fn, i, mTechParams [i].mode);
    switch (mTechParams [i].mode) {
      case NFC_DISCOVERY_TYPE_POLL_A:
      case NFC_DISCOVERY_TYPE_POLL_A_ACTIVE:
      case NFC_DISCOVERY_TYPE_LISTEN_A:
      case NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE:
        ALOGD("%s: tech A", fn);
        pollBytes.clear();
        for (int idx = 0; idx < 2; idx++) {
          pollBytes.push_back(mTechParams[i].param.pa.sens_res[idx]);
        }
        break;

      case NFC_DISCOVERY_TYPE_POLL_B:
      case NFC_DISCOVERY_TYPE_POLL_B_PRIME:
      case NFC_DISCOVERY_TYPE_LISTEN_B:
      case NFC_DISCOVERY_TYPE_LISTEN_B_PRIME:
        if (mTechList [i] == TECHNOLOGY_TYPE_ISO14443_3B) { // Is TagTechnology.NFC_B by Java API.
           // See NFC Forum Digital Protocol specification; section 5.6.2.
           // In SENSB_RES response, byte 6 through 9 is Application Data, byte 10-12 or 13 is Protocol Info.
           // Used by public API: NfcB.getApplicationData(), NfcB.getProtocolInfo().
          ALOGD("%s: tech B; TECHNOLOGY_TYPE_ISO14443_3B", fn);
          len = mTechParams[i].param.pb.sensb_res_len;
          len = len - 4; // Subtract 4 bytes for NFCID0 at byte 2 through 5.
          pollBytes.clear();
          for (int idx = 0; idx < len; idx++) {
            pollBytes.push_back(mTechParams[i].param.pb.sensb_res[idx + 4]);
          }
        } else {
          pollBytes.clear();
        }
        break;

      case NFC_DISCOVERY_TYPE_POLL_F:
      case NFC_DISCOVERY_TYPE_POLL_F_ACTIVE:
      case NFC_DISCOVERY_TYPE_LISTEN_F:
      case NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE: {
        // See NFC Forum Type 3 Tag Operation Specification; sections 2.3.2, 2.3.1.2.
        // See NFC Forum Digital Protocol Specification; sections 6.6.2.
        // PMm: manufacture parameter; 8 bytes.
        // System Code: 2 bytes/
        ALOGD("%s: tech F", fn);
        UINT8 result [10]; // Return result to NFC service.
        memset(result, 0, sizeof(result));
        len = 10;

        /****
        for(int ii = 0; ii < mTechParams [i].param.pf.sensf_res_len; ii++) {
          ALOGD("%s: tech F, sendf_res[%d]=%d (0x%x)",
            fn, ii, mTechParams [i].param.pf.sensf_res[ii],mTechParams [i].param.pf.sensf_res[ii]);
        }
        ***/
        memcpy(result, mTechParams [i].param.pf.sensf_res + 8, 8); // Copy PMm.
        if (activationData.params.t3t.num_system_codes > 0) {      // Copy the first System Code.
          UINT16 systemCode = *(activationData.params.t3t.p_system_codes);
          result [8] = (UINT8) (systemCode >> 8);
          result [9] = (UINT8) systemCode;
          ALOGD("%s: tech F; sys code=0x%X 0x%X", fn, result [8], result [9]);
        }
        pollBytes.clear();
        for (int idx = 0; idx < len; idx++) {
          pollBytes.push_back(result[idx]);
        }
        break;
      }

      case NFC_DISCOVERY_TYPE_POLL_ISO15693:
      case NFC_DISCOVERY_TYPE_LISTEN_ISO15693: {
        ALOGD("%s: tech iso 15693", fn);
        // iso 15693 response flags: 1 octet.
        // iso 15693 Data Structure Format Identifier (DSF ID): 1 octet.
        // used by public API: NfcV.getDsfId(), NfcV.getResponseFlags().
        uint8_t data [2]= {activationData.params.i93.afi, activationData.params.i93.dsfid};
        pollBytes.clear();
        for (int idx = 0; idx < 2; idx++) {
          pollBytes.push_back(data[idx]);
        }
        break;
      }

      default:
        ALOGE("%s: tech unknown ????", fn);
        pollBytes.clear();
        break;
    } // switch: every type of technology.
    techPollBytes.push_back(pollBytes);
  } // for: every technology in the array.
}

void NfcTag::fillNfcTagMembers4(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData)
{
  static const char fn [] = "NfcTag::fillNfcTagMembers4";
  std::vector<unsigned char> actBytes;
  std::vector<std::vector<uint8_t> >& techActBytes = pINfcTag->getTechActBytes();

  for (int i = 0; i < mNumTechList; i++) {
    ALOGD("%s: index=%d", fn, i);
    switch (mTechLibNfcTypes[i]) {
      case NFC_PROTOCOL_T1T: {
        ALOGD("%s: T1T; tech A", fn);
        actBytes.clear();
        actBytes.push_back(mTechParams[i].param.pa.sel_rsp);
        break;
      }

      case NFC_PROTOCOL_T2T: { // TODO: why is this code a duplicate of NFC_PROTOCOL_T1T?
        ALOGD("%s: T2T; tech A", fn);
        actBytes.clear();
        actBytes.push_back(mTechParams[i].param.pa.sel_rsp);
        break;
      }

      case NFC_PROTOCOL_T3T: { // Felica.
        ALOGD("%s: T3T; felica; tech F", fn);
        // Really, there is no data.
        actBytes.clear();
        break;
      }

      case NFC_PROTOCOL_ISO_DEP: { // t4t.
        if (mTechList [i] == TECHNOLOGY_TYPE_ISO14443_4) { // Is TagTechnology.ISO_DEP by Java API.
          if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
               (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
               (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
               (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) ) {
            // See NFC Forum Digital Protocol specification, section 11.6.2, "RATS Response"; search for "historical bytes".
            // Copy historical bytes into Java object.
            // The public API, IsoDep.getHistoricalBytes(), returns this data.
            if (activationData.activate_ntf.intf_param.type == NFC_INTERFACE_ISO_DEP) {
              tNFC_INTF_PA_ISO_DEP& pa_iso = activationData.activate_ntf.intf_param.intf_param.pa_iso;
              ALOGD("%s: T4T; ISO_DEP for tech A; copy historical bytes; len=%u", fn, pa_iso.his_byte_len);
              actBytes.clear();
              if (pa_iso.his_byte_len > 0) {
                for (int idx = 0;idx < pa_iso.his_byte_len; idx++) {
                  actBytes.push_back(pa_iso.his_byte[idx]);
                }
              }
            } else {
              ALOGE ("%s: T4T; ISO_DEP for tech A; wrong interface=%u", fn, activationData.activate_ntf.intf_param.type);
              actBytes.clear();
            }
          } else if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                      (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                      (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                      (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME) ) {
            // See NFC Forum Digital Protocol specification, section 12.6.2, "ATTRIB Response".
            // Copy higher-layer response bytes into Java object.
            // The public API, IsoDep.getHiLayerResponse(), returns this data.
            if (activationData.activate_ntf.intf_param.type == NFC_INTERFACE_ISO_DEP) {
              tNFC_INTF_PB_ISO_DEP& pb_iso = activationData.activate_ntf.intf_param.intf_param.pb_iso;
              ALOGD("%s: T4T; ISO_DEP for tech B; copy response bytes; len=%u", fn, pb_iso.hi_info_len);
              actBytes.clear();
              if (pb_iso.hi_info_len > 0) {
                for (int idx = 0;idx < pb_iso.hi_info_len;idx++) {
                  actBytes.push_back(pb_iso.hi_info[idx]);
                }
              }
            } else {
              ALOGE ("%s: T4T; ISO_DEP for tech B; wrong interface=%u", fn, activationData.activate_ntf.intf_param.type);
              actBytes.clear();
            }
          }
        } else if (mTechList [i] == TECHNOLOGY_TYPE_ISO14443_3A) { // Is TagTechnology.NFC_A by Java API.
          ALOGD("%s: T4T; tech A", fn);
          actBytes.clear();
          actBytes.push_back(mTechParams [i].param.pa.sel_rsp);
        } else {
          actBytes.clear();
        }
        break;
      } // Case NFC_PROTOCOL_ISO_DEP: //t4t.

      case NFC_PROTOCOL_15693: {
        ALOGD("%s: tech iso 15693", fn);
        // iso 15693 response flags: 1 octet.
        // iso 15693 Data Structure Format Identifier (DSF ID): 1 octet.
        // used by public API: NfcV.getDsfId(), NfcV.getResponseFlags().
        uint8_t data [2]= {activationData.params.i93.afi, activationData.params.i93.dsfid};
        actBytes.clear();
        for (int idx = 0;idx < 2;idx++) {
          actBytes.push_back(data[idx]);
        }
        break;
      }

      default:
        ALOGD("%s: tech unknown ????", fn);
        actBytes.clear();
        break;
    }
    techActBytes.push_back(actBytes);
  } // For: every technology in the array.
}

void NfcTag::fillNfcTagMembers5(INfcTag* pINfcTag, tNFA_ACTIVATED& activationData)
{
  static const char fn [] = "NfcTag::fillNfcTagMembers5";
  int len = 0;
  std::vector<unsigned char>& uid = pINfcTag->getUid();

  switch (mTechParams [0].mode) {
    case NFC_DISCOVERY_TYPE_POLL_KOVIO:
      ALOGD("%s: Kovio", fn);
      len = mTechParams [0].param.pk.uid_len;
      uid.clear();
      for (int idx = 0;idx < len;idx++) {
        uid.push_back(mTechParams [0].param.pk.uid[idx]);
      }
      break;

    case NFC_DISCOVERY_TYPE_POLL_A:
    case NFC_DISCOVERY_TYPE_POLL_A_ACTIVE:
    case NFC_DISCOVERY_TYPE_LISTEN_A:
    case NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE:
      ALOGD("%s: tech A", fn);
      len = mTechParams [0].param.pa.nfcid1_len;
      uid.clear();
      for (int idx = 0;idx < len;idx++) {
        uid.push_back(mTechParams [0].param.pa.nfcid1[idx]);
      }
      break;

    case NFC_DISCOVERY_TYPE_POLL_B:
    case NFC_DISCOVERY_TYPE_POLL_B_PRIME:
    case NFC_DISCOVERY_TYPE_LISTEN_B:
    case NFC_DISCOVERY_TYPE_LISTEN_B_PRIME:
      ALOGD("%s: tech B", fn);
      uid.clear();
      for (int idx = 0;idx < NFC_NFCID0_MAX_LEN;idx++) {
        uid.push_back(mTechParams [0].param.pb.nfcid0[idx]);
      }
      break;

    case NFC_DISCOVERY_TYPE_POLL_F:
    case NFC_DISCOVERY_TYPE_POLL_F_ACTIVE:
    case NFC_DISCOVERY_TYPE_LISTEN_F:
    case NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE:
      ALOGD("%s: tech F", fn);
      uid.clear();
      for (int idx = 0;idx < NFC_NFCID2_LEN;idx++) {
        uid.push_back(mTechParams [0].param.pf.nfcid2[idx]);
      }
      break;

    case NFC_DISCOVERY_TYPE_POLL_ISO15693:
    case NFC_DISCOVERY_TYPE_LISTEN_ISO15693: {
      ALOGD("%s: tech iso 15693", fn);
      unsigned char data [I93_UID_BYTE_LEN];  // 8 bytes.
      for (int i=0; i<I93_UID_BYTE_LEN; ++i)  // Reverse the ID.
        data[i] = activationData.params.i93.uid [I93_UID_BYTE_LEN - i - 1];
      uid.clear();
      for (int idx = 0;idx < I93_UID_BYTE_LEN;idx++) {
        uid.push_back(data[idx]);
      }
    }
    break;

    default:
      ALOGE("%s: tech unknown ????", fn);
      uid.clear();
      break;
  }
}

bool NfcTag::isP2pDiscovered()
{
  static const char fn [] = "NfcTag::isP2pDiscovered";
  bool retval = false;

  for (int i = 0; i < mNumTechList; i++) {
    if (mTechLibNfcTypes[i] == NFA_PROTOCOL_NFC_DEP) {
      // If remote device supports P2P.
      ALOGD("%s: discovered P2P", fn);
      retval = true;
      break;
    }
  }
  ALOGD("%s: return=%u", fn, retval);
  return retval;
}

void NfcTag::selectP2p()
{
  static const char fn [] = "NfcTag::selectP2p";
  UINT8 rfDiscoveryId = 0;

  for (int i = 0; i < mNumTechList; i++) {
    // If remote device does not support P2P, just skip it.
    if (mTechLibNfcTypes[i] != NFA_PROTOCOL_NFC_DEP)
      continue;

    // If remote device supports tech F.
    // Tech F is preferred because it is faster than tech A.
    if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F) ||
         (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F_ACTIVE) ) {
      rfDiscoveryId = mTechHandles[i];
      break; // No need to search further.
    } else if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ) {
      // Only choose tech A if tech F is unavailable.
      if (rfDiscoveryId == 0)
        rfDiscoveryId = mTechHandles[i];
    }
  }

  if (rfDiscoveryId > 0) {
    ALOGD("%s: select P2P; target rf discov id=0x%X", fn, rfDiscoveryId);
    tNFA_STATUS stat = NFA_Select(rfDiscoveryId, NFA_PROTOCOL_NFC_DEP, NFA_INTERFACE_NFC_DEP);
    if (stat != NFA_STATUS_OK)
      ALOGE("%s: fail select P2P; error=0x%X", fn, stat);
  } else
    ALOGE("%s: cannot find P2P", fn);
  resetTechnologies();
}

void NfcTag::resetTechnologies()
{
  static const char fn [] = "NfcTag::resetTechnologies";
  ALOGD("%s", fn);
  mNumTechList = 0;
  memset(mTechList, 0, sizeof(mTechList));
  memset(mTechHandles, 0, sizeof(mTechHandles));
  memset(mTechLibNfcTypes, 0, sizeof(mTechLibNfcTypes));
  memset(mTechParams, 0, sizeof(mTechParams));

  resetAllTransceiveTimeouts();
}

void NfcTag::selectFirstTag()
{
  static const char fn [] = "NfcTag::selectFirstTag";
  ALOGD("%s: nfa target h=0x%X; protocol=0x%X",
          fn, mTechHandles [0], mTechLibNfcTypes [0]);
  tNFA_INTF_TYPE rf_intf = NFA_INTERFACE_FRAME;

  if (mTechLibNfcTypes [0] == NFA_PROTOCOL_ISO_DEP) {
    rf_intf = NFA_INTERFACE_ISO_DEP;
  } else if (mTechLibNfcTypes [0] == NFA_PROTOCOL_NFC_DEP) {
    rf_intf = NFA_INTERFACE_NFC_DEP;
  }
  else {
    rf_intf = NFA_INTERFACE_FRAME;
  }

  tNFA_STATUS stat = NFA_Select(mTechHandles [0], mTechLibNfcTypes [0], rf_intf);
  if (stat != NFA_STATUS_OK)
    ALOGE("%s: fail select; error=0x%X", fn, stat);
}

int NfcTag::getT1tMaxMessageSize()
{
  static const char fn [] = "NfcTag::getT1tMaxMessageSize";

  if (mProtocol != NFC_PROTOCOL_T1T) {
    ALOGE("%s: wrong protocol %u", fn, mProtocol);
    return 0;
  }
  return mtT1tMaxMessageSize;
}

void NfcTag::calculateT1tMaxMessageSize(tNFA_ACTIVATED& activate)
{
  static const char fn [] = "NfcTag::calculateT1tMaxMessageSize";

  // Make sure the tag is type-1.
  if (activate.activate_ntf.protocol != NFC_PROTOCOL_T1T) {
    mtT1tMaxMessageSize = 0;
    return;
  }

  // Examine the first byte of header ROM bytes.
  switch (activate.params.t1t.hr[0]) {
    case RW_T1T_IS_TOPAZ96:
      mtT1tMaxMessageSize = 90;
      break;
    case RW_T1T_IS_TOPAZ512:
      mtT1tMaxMessageSize = 462;
      break;
    default:
      ALOGE("%s: unknown T1T HR0=%u", fn, activate.params.t1t.hr[0]);
      mtT1tMaxMessageSize = 0;
      break;
  }
}

bool NfcTag::isMifareUltralight()
{
  static const char fn [] = "NfcTag::isMifareUltralight";
  bool retval = false;

  for (int i =0; i < mNumTechList; i++) {
    if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
         (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
         (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) ) {
      // See NFC Digital Protocol, section 4.6.3 (SENS_RES); section 4.8.2 (SEL_RES).
      // See Mifare Type Identification Procedure, section 5.1 (ATQA), 5.2 (SAK).
      if ( (mTechParams[i].param.pa.sens_res[0] == 0x44) &&
           (mTechParams[i].param.pa.sens_res[1] == 0) ) {
        // SyncEventGuard g (mReadCompleteEvent);
        // mReadCompletedStatus = NFA_STATUS_BUSY;
        // ALOGD ("%s: read block 0x10", fn);
        // tNFA_STATUS stat = NFA_RwT2tRead (0x10);
        // if (stat == NFA_STATUS_OK)
        // mReadCompleteEvent.wait ();
        //
        // //if read-completion status is failure, then the tag is
        // //definitely Mifare Ultralight;
        // //if read-completion status is OK, then the tag is
        // //definitely Mifare Ultralight C;
        // retval = (mReadCompletedStatus == NFA_STATUS_FAILED);
        retval = true;
      }
      break;
    }
  }
  ALOGD("%s: return=%u", fn, retval);
  return retval;
}

bool NfcTag::isT2tNackResponse(const UINT8* response, UINT32 responseLen)
{
  static const char fn [] = "NfcTag::isT2tNackResponse";
  bool isNack = false;

  if (responseLen == 1) {
    if (response[0] == 0xA)
      isNack = false; // An ACK response, so definitely not a NACK.
    else
      isNack = true;  // Assume every value is a NACK.
  }
  ALOGD("%s: return %u", fn, isNack);
  return isNack;
}

bool NfcTag::isNdefDetectionTimedOut()
{
  return mNdefDetectionTimedOut;
}

void NfcTag::connectionEventHandler(UINT8 event, tNFA_CONN_EVT_DATA* data)
{
  static const char fn [] = "NfcTag::connectionEventHandler";

  switch (event) {
    case NFA_DISC_RESULT_EVT: {
      tNFA_DISC_RESULT& disc_result = data->disc_result;
      if (disc_result.status == NFA_STATUS_OK) {
        discoverTechnologies(disc_result);
      }
      break;
    }

    case NFA_ACTIVATED_EVT:
      // Only do tag detection if we are polling and it is not 'EE Direct RF' activation.
      // (which may happen when we are activated as a tag).
      if (data->activated.activate_ntf.rf_tech_param.mode < NCI_DISCOVERY_TYPE_LISTEN_A
          && data->activated.activate_ntf.intf_param.type != NFC_INTERFACE_EE_DIRECT_RF) {
        tNFA_ACTIVATED& activated = data->activated;
        if (IsSameKovio(activated))
          break;
        mProtocol = activated.activate_ntf.protocol;
        calculateT1tMaxMessageSize(activated);
        discoverTechnologies(activated);
        createNfcTag(activated);
      }
      break;

    case NFA_DEACTIVATED_EVT:
      mProtocol = NFC_PROTOCOL_UNKNOWN;
      resetTechnologies();
      break;

    case NFA_READ_CPLT_EVT: {
      SyncEventGuard g (mReadCompleteEvent);
      mReadCompletedStatus = data->status;
      mReadCompleteEvent.notifyOne();
      break;
    }

    case NFA_NDEF_DETECT_EVT: {
      tNFA_NDEF_DETECT& ndef_detect = data->ndef_detect;
      mNdefDetectionTimedOut = ndef_detect.status == NFA_STATUS_TIMEOUT;
      if (mNdefDetectionTimedOut)
        ALOGE("%s: NDEF detection timed out", fn);
    }
  }
}

void NfcTag::resetAllTransceiveTimeouts()
{
  mTimeoutTable[TECHNOLOGY_TYPE_ISO14443_3A] = 618;     // NfcA
  mTimeoutTable[TECHNOLOGY_TYPE_ISO14443_3B] = 1000;    // NfcB
  mTimeoutTable[TECHNOLOGY_TYPE_ISO14443_4] = 309;      // ISO-DEP
  mTimeoutTable[TECHNOLOGY_TYPE_FELICA] = 255;          // Felica
  mTimeoutTable[TECHNOLOGY_TYPE_ISO15693] = 1000;       // NfcV
  mTimeoutTable[TECHNOLOGY_TYPE_MIFARE_CLASSIC] = 618;  // MifareClassic
  mTimeoutTable[TECHNOLOGY_TYPE_MIFARE_UL] = 618;       // MifareUltralight
  mTimeoutTable[TECHNOLOGY_TYPE_KOVIO_BARCODE] = 1000;  // NfcBarcode
}

int NfcTag::getTransceiveTimeout(int techId)
{
  int retval = 1000;
  if ((techId > 0) && (techId < (int)mTimeoutTable.size())) {
    retval = mTimeoutTable[techId];
  } else {
    ALOGE("%s: invalid tech=%d", __FUNCTION__, techId);
  }

  return retval;
}
