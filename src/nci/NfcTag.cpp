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

#include "NfcDebug.h"
#include "NfcManager.h"
#include "INfcTag.h"

extern "C"
{
  #include "rw_int.h"
#ifdef NFCC_PN547
  #include "phNxpExtns.h"
#endif
}

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

NfcTag& NfcTag::GetInstance()
{
  static NfcTag tag;
  return tag;
}

void NfcTag::Initialize(NfcManager* aNfcManager)
{
  mNfcManager = aNfcManager;

  mActivationState = Idle;
  mProtocol = NFC_PROTOCOL_UNKNOWN;
  mNumTechList = 0;
  mtT1tMaxMessageSize = 0;
  mReadCompletedStatus = NFA_STATUS_OK;
  ResetTechnologies();
}

void NfcTag::Abort()
{
  SyncEventGuard g(mReadCompleteEvent);
  mReadCompleteEvent.NotifyOne();
}

NfcTag::ActivationState NfcTag::GetActivationState()
{
  return mActivationState;
}

void NfcTag::SetDeactivationState(tNFA_DEACTIVATED& aDeactivated)
{
  mActivationState = Idle;
  mNdefDetectionTimedOut = false;
  if (aDeactivated.type == NFA_DEACTIVATE_TYPE_SLEEP) {
    mActivationState = Sleep;
  }
  NCI_DEBUG("state=%u", mActivationState);
}

void NfcTag::SetActivationState()
{
  mNdefDetectionTimedOut = false;
  mActivationState = Active;
  NCI_DEBUG("state=%u", mActivationState);
}

tNFC_PROTOCOL NfcTag::GetProtocol()
{
  return mProtocol;
}

uint32_t TimeDiff(timespec aStart, timespec aEnd)
{
  timespec temp;
  if ((aEnd.tv_nsec-aStart.tv_nsec)<0) {
    temp.tv_sec = aEnd.tv_sec - aStart.tv_sec - 1;
    temp.tv_nsec = 1000000000 + aEnd.tv_nsec - aStart.tv_nsec;
  } else {
    temp.tv_sec = aEnd.tv_sec - aStart.tv_sec;
    temp.tv_nsec = aEnd.tv_nsec - aStart.tv_nsec;
  }

  return (temp.tv_sec * 1000) + (temp.tv_nsec / 1000000);
}

bool NfcTag::IsSameKovio(tNFA_ACTIVATED& aActivationData)
{
  NCI_DEBUG("enter");
  tNFC_ACTIVATE_DEVT& rfDetail = aActivationData.activate_ntf;

  if (rfDetail.protocol != NFC_PROTOCOL_KOVIO)
    return false;

  memcpy(&(mTechParams[0]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
  if (mTechParams[0].mode != NFC_DISCOVERY_TYPE_POLL_KOVIO) {
    return false;
  }

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  bool rVal = false;
  if (mTechParams[0].param.pk.uid_len == mLastKovioUidLen) {
    if (memcmp(mLastKovioUid,
               &mTechParams[0].param.pk.uid,
               mTechParams[0].param.pk.uid_len) == 0) {
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
  NCI_DEBUG("exit, is same Kovio=%d", rVal);
  return rVal;
}

void NfcTag::DiscoverTechnologies(tNFA_ACTIVATED& aActivationData)
{
  NCI_DEBUG("enter");
  tNFC_ACTIVATE_DEVT& rfDetail = aActivationData.activate_ntf;

  mNumTechList = 0;
  mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
  mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;

  size_t techLen = sizeof(rfDetail.rf_tech_param);
  // Save the stack's data structure for interpretation later
  memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), techLen);

  switch (rfDetail.protocol) {
    case NFC_PROTOCOL_T1T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;
      break;
    case NFC_PROTOCOL_T2T: {
      tNFC_RF_TECH_PARAMS tech_params;
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;

      // Could be MifFare UL or Classic or Kovio.
      // Need to look at first byte of uid to find manuf.
      memcpy(&tech_params, &(rfDetail.rf_tech_param), techLen);
      if ((tech_params.param.pa.nfcid1[0] == 0x04 && rfDetail.rf_tech_param.param.pa.sel_rsp == 0) ||
          rfDetail.rf_tech_param.param.pa.sel_rsp == 0x18 ||
          rfDetail.rf_tech_param.param.pa.sel_rsp == 0x08) {
        if (rfDetail.rf_tech_param.param.pa.sel_rsp == 0) {
          mNumTechList++;
          mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
          mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;
          // Save the stack's data structure for interpretation later
          memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), techLen);
          mTechList[mNumTechList] = TECHNOLOGY_TYPE_MIFARE_UL;
        }
      }
      break;
    }
    case NFC_PROTOCOL_T3T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_FELICA;
      break;
    case NFC_PROTOCOL_ISO_DEP: // Type-4 tag uses technology ISO-DEP and technology A or B.
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_4;
      if ( (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
             (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
             (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
             (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) ) {
        mNumTechList++;
        mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;
        // Save the stack's data structure for interpretation later
        memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), techLen);
      } else if ((rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                 (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                 (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                 (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME)) {
        mNumTechList++;
        mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3B;
        // Save the stack's data structure for interpretation later.
        memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), techLen);
      }
      break;
    case NFC_PROTOCOL_15693:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO15693;
      break;
    case NFC_PROTOCOL_KOVIO:
      NCI_DEBUG("Kovio");
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_KOVIO_BARCODE;
      break;
#ifdef NFCC_PN547
    case NFC_PROTOCOL_MIFARE:
      NCI_DEBUG("Mifare Classic detected");
      EXTNS_MfcInit(aActivationData);
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;
      mNumTechList++;
      mTechHandles[mNumTechList] = rfDetail.rf_disc_id;
      mTechLibNfcTypes[mNumTechList] = rfDetail.protocol;
      // Save the stack's data structure for interpretation later.
      memcpy(&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), techLen);
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_MIFARE_CLASSIC;
      break;
#endif
    default:
      NCI_ERROR("unknown protocol ????");
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_UNKNOWN;
      break;
  }

  mNumTechList++;
  for (int i = 0; i < mNumTechList; i++) {
    NCI_DEBUG("index=%d; tech=%d; handle=%d; nfc type=%d",
              i, mTechList[i], mTechHandles[i], mTechLibNfcTypes[i]);
  }
  NCI_DEBUG("exit");
}

void NfcTag::DiscoverTechnologies(tNFA_DISC_RESULT& aDiscoveryData)
{
  tNFC_RESULT_DEVT& discovery_ntf = aDiscoveryData.discovery_ntf;

  NCI_DEBUG("enter: rf disc. id=%u; protocol=%u, mNumTechList=%u",
            discovery_ntf.rf_disc_id, discovery_ntf.protocol, mNumTechList);
  if (mNumTechList >= MAX_NUM_TECHNOLOGY) {
    NCI_ERROR("exceed max=%d, exit", MAX_NUM_TECHNOLOGY);
    return;
  }
  mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
  mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;

  // Save the stack's data structure for interpretation later.
  size_t techLen = sizeof(discovery_ntf.rf_tech_param);
  memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), techLen);

  switch (discovery_ntf.protocol) {
    case NFC_PROTOCOL_T1T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;
      break;
    // Type-2 tags are identitical to Mifare Ultralight, so Ultralight is also discovered.
    case NFC_PROTOCOL_T2T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;
      if (discovery_ntf.rf_tech_param.param.pa.sel_rsp == 0) {
        // Mifare Ultralight.
        mNumTechList++;
        mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_MIFARE_UL;
      }

      // Save the stack's data structure for interpretation later.
      memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), techLen);
      break;
    case NFC_PROTOCOL_T3T:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_FELICA;
      break;
    // Type-4 tag uses technology ISO-DEP and technology A or B.
    case NFC_PROTOCOL_ISO_DEP:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_4;
      if ((discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
          (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
          (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
          (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE)) {
        mNumTechList++;
        mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;
        // Save the stack's data structure for interpretation later.
        memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), techLen);
      } else if ((discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                 (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                 (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                 (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME)) {
        mNumTechList++;
        mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
        mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;
        mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3B;
        // Save the stack's data structure for interpretation later
        memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), techLen);
      }
      break;
    case NFC_PROTOCOL_15693:
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO15693;
      break;
#ifdef NFCC_PN547
    case NFC_PROTOCOL_MIFARE:
      mTechHandles[mNumTechList] = discovery_ntf.rf_disc_id;
      mTechLibNfcTypes[mNumTechList] = discovery_ntf.protocol;
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_MIFARE_CLASSIC;
      // Save the stack's data structure for interpretation later
      memcpy(&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), techLen);
      mNumTechList++;
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_ISO14443_3A;
      break;
#endif
    default:
      NCI_ERROR("unknown protocol ????");
      mTechList[mNumTechList] = TECHNOLOGY_TYPE_UNKNOWN;
      break;
  }

  mNumTechList++;
  if (discovery_ntf.more == FALSE) {
    for (int i = 0; i < mNumTechList; i++) {
      NCI_DEBUG("index=%d; tech=%d; handle=%d; nfc type=%d",
                i, mTechList[i], mTechHandles[i], mTechLibNfcTypes[i]);
    }
  }
}

void NfcTag::CreateNfcTag(tNFA_ACTIVATED& aActivationData)
{
  NCI_DEBUG("enter");

  INfcTag* pINfcTag =
    reinterpret_cast<INfcTag*>(mNfcManager->QueryInterface(INTERFACE_TAG_MANAGER));

  if (pINfcTag == NULL) {
    NCI_ERROR("cannot get nfc tag class");
    return;
  }

  // Fill NfcTag's mProtocols, mTechList, mTechHandles, mTechLibNfcTypes.
  FillNfcTagMembers1(pINfcTag);

  // Fill NfcTag's members: mHandle, mConnectedTechnology.
  FillNfcTagMembers2(pINfcTag);

  // Fill NfcTag's members: mTechPollBytes.
  FillNfcTagMembers3(pINfcTag, aActivationData);

  // Fill NfcTag's members: mTechActBytes.
  FillNfcTagMembers4(pINfcTag, aActivationData);

  // Fill NfcTag's members: mUid.
  FillNfcTagMembers5(pINfcTag, aActivationData);

  // Notify NFC service about this new tag.
  NCI_DEBUG("try notify nfc service");
  mNfcManager->NotifyTagDiscovered(pINfcTag);

  NCI_DEBUG("exit");
}

void NfcTag::FillNfcTagMembers1(INfcTag* aINfcTag)
{
  std::vector<TagTechnology>& techList = aINfcTag->GetTechList();
  std::vector<int>& techHandles = aINfcTag->GetTechHandles();
  std::vector<int>& techLibNfcTypes = aINfcTag->GetTechLibNfcTypes();

  for (int i = 0; i < mNumTechList; i++) {
    gNat.tProtocols[i] = mTechLibNfcTypes[i];
    gNat.handles[i] = mTechHandles[i];

    // Convert from vendor specific technology definition to common tag technology definition.
    techList.push_back(NfcNciUtil::ToTagTechnology(mTechList[i]));
    techHandles.push_back(mTechHandles[i]);
    techLibNfcTypes.push_back(mTechLibNfcTypes[i]);
  }
}

// Fill NfcTag's members: mHandle, mConnectedTechnology.
void NfcTag::FillNfcTagMembers2(INfcTag* aINfcTag)
{
  int& connectedTechIndex = aINfcTag->GetConnectedHandle();
  connectedTechIndex = 0;
}

void NfcTag::FillNfcTagMembers3(INfcTag* aINfcTag,
                                tNFA_ACTIVATED& aActivationData)
{
  int len = 0;
  std::vector<uint8_t> pollBytes;
  std::vector<std::vector<uint8_t> >& techPollBytes = aINfcTag->GetTechPollBytes();

  for (int i = 0; i < mNumTechList; i++) {
    NCI_DEBUG("index=%d; rf tech params mode=%u", i, mTechParams[i].mode);
    switch (mTechParams[i].mode) {
      case NFC_DISCOVERY_TYPE_POLL_A:
      case NFC_DISCOVERY_TYPE_POLL_A_ACTIVE:
      case NFC_DISCOVERY_TYPE_LISTEN_A:
      case NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE:
        NCI_DEBUG("tech A");
        pollBytes.clear();
        for (int j = 0; j < 2; j++) {
          pollBytes.push_back(mTechParams[i].param.pa.sens_res[j]);
        }
        break;
      case NFC_DISCOVERY_TYPE_POLL_B:
      case NFC_DISCOVERY_TYPE_POLL_B_PRIME:
      case NFC_DISCOVERY_TYPE_LISTEN_B:
      case NFC_DISCOVERY_TYPE_LISTEN_B_PRIME:
        if (mTechList[i] == TECHNOLOGY_TYPE_ISO14443_3B) {
          // See NFC Forum Digital Protocol specification; section 5.6.2.
          // In SENSB_RES response, byte 6 through 9 is Application Data,
          // byte 10-12 or 13 is Protocol Info.
          NCI_DEBUG("tech B; TECHNOLOGY_TYPE_ISO14443_3B");
          len = mTechParams[i].param.pb.sensb_res_len;
          len = len - 4; // Subtract 4 bytes for NFCID0 at byte 2 through 5.
          pollBytes.clear();
          for (int j = 0; j < len; j++) {
            pollBytes.push_back(mTechParams[i].param.pb.sensb_res[j + 4]);
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
        NCI_DEBUG("tech F");
        uint8_t result[10]; // Return result to NFC service.
        memset(result, 0, sizeof(result));
        len = 10;

        memcpy(result, mTechParams[i].param.pf.sensf_res + 8, 8); // Copy PMm.
        // Copy the first System Code.
        if (aActivationData.params.t3t.num_system_codes > 0) {
          uint16_t systemCode = *(aActivationData.params.t3t.p_system_codes);
          result[8] = (uint8_t)(systemCode >> 8);
          result[9] = (uint8_t)systemCode;
          NCI_DEBUG("tech F; sys code=0x%X 0x%X", result[8], result[9]);
        }
        pollBytes.clear();
        for (int j = 0; j < len; j++) {
          pollBytes.push_back(result[j]);
        }
        break;
      }
      case NFC_DISCOVERY_TYPE_POLL_ISO15693:
      case NFC_DISCOVERY_TYPE_LISTEN_ISO15693: {
        NCI_DEBUG("tech iso 15693");
        // iso 15693 response flags: 1 octet.
        // iso 15693 Data Structure Format Identifier (DSF ID): 1 octet.
        uint8_t data[2] =
          {aActivationData.params.i93.afi, aActivationData.params.i93.dsfid};
        pollBytes.clear();
        for (int i = 0; i < 2; i++) {
          pollBytes.push_back(data[i]);
        }
        break;
      }
      default:
        NCI_ERROR("tech unknown ????");
        pollBytes.clear();
        break;
    }
    techPollBytes.push_back(pollBytes);
  }
}

void NfcTag::FillNfcTagMembers4(INfcTag* aINfcTag,
                                tNFA_ACTIVATED& aActivationData)
{
  std::vector<unsigned char> actBytes;
  std::vector<std::vector<uint8_t> >& techActBytes = aINfcTag->GetTechActBytes();

  for (int i = 0; i < mNumTechList; i++) {
    NCI_DEBUG("index=%d", i);
    switch (mTechLibNfcTypes[i]) {
      case NFC_PROTOCOL_T1T:
        NCI_DEBUG("T1T; tech A");
        actBytes.clear();
        actBytes.push_back(mTechParams[i].param.pa.sel_rsp);
        break;
      // TODO: why is this code a duplicate of NFC_PROTOCOL_T1T?
      case NFC_PROTOCOL_T2T:
        NCI_DEBUG("T2T; tech A");
        actBytes.clear();
        actBytes.push_back(mTechParams[i].param.pa.sel_rsp);
        break;
      // Felica.
      case NFC_PROTOCOL_T3T:
        NCI_DEBUG("T3T; felica; tech F");
        // Really, there is no data.
        actBytes.clear();
        break;
      // T4T
      case NFC_PROTOCOL_ISO_DEP:
        if (mTechList[i] == TECHNOLOGY_TYPE_ISO14443_4) {
          if ((mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
              (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
              (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
              (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) ) {
            // See NFC Forum Digital Protocol specification, section 11.6.2,
            // "RATS Response"; search for "historical bytes".
            // Copy historical bytes into Java object.
            // The public API, IsoDep.getHistoricalBytes(), returns this data.
            if (aActivationData.activate_ntf.intf_param.type == NFC_INTERFACE_ISO_DEP) {
              tNFC_INTF_PA_ISO_DEP& pa_iso =
                aActivationData.activate_ntf.intf_param.intf_param.pa_iso;
              NCI_DEBUG("T4T; ISO_DEP for tech A; copy historical bytes; len=%u", pa_iso.his_byte_len);
              actBytes.clear();
              if (pa_iso.his_byte_len > 0) {
                for (int j = 0; j < pa_iso.his_byte_len; j++) {
                  actBytes.push_back(pa_iso.his_byte[j]);
                }
              }
            } else {
              NCI_ERROR("T4T; ISO_DEP for tech A; wrong interface=%u",
                        aActivationData.activate_ntf.intf_param.type);
              actBytes.clear();
            }
          } else if ((mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                     (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                     (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                     (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME)) {
            // See NFC Forum Digital Protocol specification, section 12.6.2, "ATTRIB Response".
            // Copy higher-layer response bytes into Java object.
            // The public API, IsoDep.getHiLayerResponse(), returns this data.
            if (aActivationData.activate_ntf.intf_param.type == NFC_INTERFACE_ISO_DEP) {
              tNFC_INTF_PB_ISO_DEP& pb_iso =
                aActivationData.activate_ntf.intf_param.intf_param.pb_iso;
              NCI_DEBUG("T4T; ISO_DEP for tech B; copy response bytes; len=%u", pb_iso.hi_info_len);
              actBytes.clear();
              if (pb_iso.hi_info_len > 0) {
                for (int j = 0; j < pb_iso.hi_info_len; j++) {
                  actBytes.push_back(pb_iso.hi_info[j]);
                }
              }
            } else {
              NCI_ERROR("T4T; ISO_DEP for tech B; wrong interface=%u",
                        aActivationData.activate_ntf.intf_param.type);
              actBytes.clear();
            }
          }
        } else if (mTechList[i] == TECHNOLOGY_TYPE_ISO14443_3A) {
          NCI_DEBUG("T4T; tech A");
          actBytes.clear();
          actBytes.push_back(mTechParams[i].param.pa.sel_rsp);
        } else {
          actBytes.clear();
        }
        break;
#ifdef NFCC_PN547
      case NFC_PROTOCOL_MIFARE:
        NCI_DEBUG("Mifare Classic; tech A");
        actBytes.clear();
        actBytes.push_back(mTechParams[i].param.pa.sel_rsp);
        break;
#endif
      case NFC_PROTOCOL_15693: {
        NCI_DEBUG("tech iso 15693");
        // iso 15693 response flags: 1 octet.
        // iso 15693 Data Structure Format Identifier (DSF ID): 1 octet.
        uint8_t data[2] =
          {aActivationData.params.i93.afi, aActivationData.params.i93.dsfid};
        actBytes.clear();
        for (int j = 0; j < 2; j++) {
          actBytes.push_back(data[j]);
        }
        break;
      }
      default:
        NCI_DEBUG("tech unknown ????");
        actBytes.clear();
        break;
    }
    techActBytes.push_back(actBytes);
  }
}

void NfcTag::FillNfcTagMembers5(INfcTag* aINfcTag,
                                tNFA_ACTIVATED& aActivationData)
{
  int len = 0;
  std::vector<unsigned char>& uid = aINfcTag->GetUid();

  switch (mTechParams[0].mode) {
    case NFC_DISCOVERY_TYPE_POLL_KOVIO:
      NCI_DEBUG("Kovio");
      len = mTechParams[0].param.pk.uid_len;
      uid.clear();
      for (int i = 0; i < len; i++) {
        uid.push_back(mTechParams[0].param.pk.uid[i]);
      }
      break;
    case NFC_DISCOVERY_TYPE_POLL_A:
    case NFC_DISCOVERY_TYPE_POLL_A_ACTIVE:
    case NFC_DISCOVERY_TYPE_LISTEN_A:
    case NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE:
      NCI_DEBUG("tech A");
      len = mTechParams[0].param.pa.nfcid1_len;
      uid.clear();
      for (int i = 0; i < len; i++) {
        uid.push_back(mTechParams[0].param.pa.nfcid1[i]);
      }
      break;
    case NFC_DISCOVERY_TYPE_POLL_B:
    case NFC_DISCOVERY_TYPE_POLL_B_PRIME:
    case NFC_DISCOVERY_TYPE_LISTEN_B:
    case NFC_DISCOVERY_TYPE_LISTEN_B_PRIME:
      NCI_DEBUG("tech B");
      uid.clear();
      for (int i = 0; i < NFC_NFCID0_MAX_LEN; i++) {
        uid.push_back(mTechParams[0].param.pb.nfcid0[i]);
      }
      break;
    case NFC_DISCOVERY_TYPE_POLL_F:
    case NFC_DISCOVERY_TYPE_POLL_F_ACTIVE:
    case NFC_DISCOVERY_TYPE_LISTEN_F:
    case NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE:
      NCI_DEBUG("tech F");
      uid.clear();
      for (int i = 0; i < NFC_NFCID2_LEN; i++) {
        uid.push_back(mTechParams[0].param.pf.nfcid2[i]);
      }
      break;
    case NFC_DISCOVERY_TYPE_POLL_ISO15693:
    case NFC_DISCOVERY_TYPE_LISTEN_ISO15693: {
      NCI_DEBUG("tech iso 15693");
      unsigned char data[I93_UID_BYTE_LEN];  // 8 bytes.
      for (int i = 0; i < I93_UID_BYTE_LEN; i++) {  // Reverse the ID.
        data[i] = aActivationData.params.i93.uid[I93_UID_BYTE_LEN - i - 1];
      }
      uid.clear();
      for (int i = 0; i < I93_UID_BYTE_LEN; i++) {
        uid.push_back(data[i]);
      }
    }
    break;
    default:
      NCI_ERROR("tech unknown ????");
      uid.clear();
      break;
  }
}

bool NfcTag::IsP2pDiscovered()
{
  bool retval = false;

  for (int i = 0; i < mNumTechList; i++) {
    if (mTechLibNfcTypes[i] == NFA_PROTOCOL_NFC_DEP) {
      // If remote device supports P2P.
      NCI_DEBUG("discovered P2P");
      retval = true;
      break;
    }
  }
  NCI_DEBUG("return=%u", retval);
  return retval;
}

void NfcTag::SelectP2p()
{
  uint8_t rfDiscoveryId = 0;

  for (int i = 0; i < mNumTechList; i++) {
    // If remote device does not support P2P, just skip it.
    if (mTechLibNfcTypes[i] != NFA_PROTOCOL_NFC_DEP)
      continue;

    // If remote device supports tech F.
    // Tech F is preferred because it is faster than tech A.
    if ((mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F) ||
        (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F_ACTIVE)) {
      rfDiscoveryId = mTechHandles[i];
      break; // No need to search further.
    } else if ((mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
               (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE)) {
      // Only choose tech A if tech F is unavailable.
      if (rfDiscoveryId == 0)
        rfDiscoveryId = mTechHandles[i];
    }
  }

  if (rfDiscoveryId > 0) {
    NCI_DEBUG("select P2P; target rf discov id=0x%X", rfDiscoveryId);
    tNFA_STATUS stat =
      NFA_Select(rfDiscoveryId, NFA_PROTOCOL_NFC_DEP, NFA_INTERFACE_NFC_DEP);
    if (stat != NFA_STATUS_OK) {
      NCI_ERROR("fail select P2P; error=0x%X", stat);
    }
  } else {
    NCI_ERROR("cannot find P2P");
  }
  ResetTechnologies();
}

void NfcTag::ResetTechnologies()
{
  NCI_DEBUG("enter");
  mNumTechList = 0;
  memset(mTechList, 0, sizeof(mTechList));
  memset(mTechHandles, 0, sizeof(mTechHandles));
  memset(mTechLibNfcTypes, 0, sizeof(mTechLibNfcTypes));
  memset(mTechParams, 0, sizeof(mTechParams));

  ResetAllTransceiveTimeouts();
}

void NfcTag::SelectFirstTag()
{
  NCI_DEBUG("nfa target h=0x%X; protocol=0x%X",
            mTechHandles[0], mTechLibNfcTypes[0]);
  tNFA_INTF_TYPE rf_intf = NFA_INTERFACE_FRAME;

  if (mTechLibNfcTypes[0] == NFA_PROTOCOL_ISO_DEP) {
    rf_intf = NFA_INTERFACE_ISO_DEP;
  } else if (mTechLibNfcTypes[0] == NFA_PROTOCOL_NFC_DEP) {
    rf_intf = NFA_INTERFACE_NFC_DEP;
  } else {
    rf_intf = NFA_INTERFACE_FRAME;
  }

  tNFA_STATUS stat = NFA_Select(mTechHandles[0], mTechLibNfcTypes[0], rf_intf);
  if (stat != NFA_STATUS_OK) {
    NCI_ERROR("fail select; error=0x%X", stat);
  }
}

int NfcTag::GetT1tMaxMessageSize()
{
  if (mProtocol != NFC_PROTOCOL_T1T) {
    NCI_ERROR("wrong protocol %u", mProtocol);
    return 0;
  }
  return mtT1tMaxMessageSize;
}

void NfcTag::CalculateT1tMaxMessageSize(tNFA_ACTIVATED& aActivate)
{
  // Make sure the tag is type-1.
  if (aActivate.activate_ntf.protocol != NFC_PROTOCOL_T1T) {
    mtT1tMaxMessageSize = 0;
    return;
  }

  // Examine the first byte of header ROM bytes.
  switch (aActivate.params.t1t.hr[0]) {
    case RW_T1T_IS_TOPAZ96:
      mtT1tMaxMessageSize = 90;
      break;
    case RW_T1T_IS_TOPAZ512:
      mtT1tMaxMessageSize = 462;
      break;
    default:
      NCI_ERROR("unknown T1T HR0=%u", aActivate.params.t1t.hr[0]);
      mtT1tMaxMessageSize = 0;
      break;
  }
}

bool NfcTag::IsMifareUltralight()
{
  bool retval = false;

  for (int i = 0; i < mNumTechList; i++) {
    if ((mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
        (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
        (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE)) {
      // See NFC Digital Protocol, section 4.6.3 (SENS_RES); section 4.8.2 (SEL_RES).
      // See Mifare Type Identification Procedure, section 5.1 (ATQA), 5.2 (SAK).
      if ((mTechParams[i].param.pa.sens_res[0] == 0x44) &&
          (mTechParams[i].param.pa.sens_res[1] == 0)) {
        retval = true;
      }
      break;
    }
  }
  NCI_DEBUG("return=%u", retval);
  return retval;
}

bool NfcTag::IsT2tNackResponse(const uint8_t* aResponse,
                               uint32_t aResponseLen)
{
  bool isNack = false;

  if (aResponseLen == 1) {
    if (aResponse[0] == 0xA) {
      isNack = false; // An ACK response, so definitely not a NACK.
    } else {
      isNack = true;  // Assume every value is a NACK.
    }
  }
  NCI_DEBUG("return %u", isNack);
  return isNack;
}

bool NfcTag::IsNdefDetectionTimedOut()
{
  return mNdefDetectionTimedOut;
}

void NfcTag::ConnectionEventHandler(uint8_t aEvent,
                                    tNFA_CONN_EVT_DATA* aData)
{
  switch (aEvent) {
    case NFA_DISC_RESULT_EVT: {
      tNFA_DISC_RESULT& disc_result = aData->disc_result;
      if (disc_result.status == NFA_STATUS_OK) {
        DiscoverTechnologies(disc_result);
      }
      break;
    }
    case NFA_ACTIVATED_EVT:
      // Only do tag detection if we are polling and it is not 'EE Direct RF' activation.
      // (which may happen when we are activated as a tag).
      if (aData->activated.activate_ntf.rf_tech_param.mode < NCI_DISCOVERY_TYPE_LISTEN_A &&
          aData->activated.activate_ntf.intf_param.type != NFC_INTERFACE_EE_DIRECT_RF) {
        tNFA_ACTIVATED& activated = aData->activated;
        if (IsSameKovio(activated))
          break;
        mProtocol = activated.activate_ntf.protocol;
        CalculateT1tMaxMessageSize(activated);
        DiscoverTechnologies(activated);
        CreateNfcTag(activated);
      }
      break;
    case NFA_DEACTIVATED_EVT:
      mProtocol = NFC_PROTOCOL_UNKNOWN;
      ResetTechnologies();
      break;
    case NFA_READ_CPLT_EVT: {
      SyncEventGuard g(mReadCompleteEvent);
      mReadCompletedStatus = aData->status;
      mReadCompleteEvent.NotifyOne();
      break;
    }
    case NFA_NDEF_DETECT_EVT: {
      tNFA_NDEF_DETECT& ndef_detect = aData->ndef_detect;
      mNdefDetectionTimedOut = ndef_detect.status == NFA_STATUS_TIMEOUT;
      if (mNdefDetectionTimedOut) {
        NCI_ERROR("NDEF detection timed out");
      }
    }
  }
}

void NfcTag::ResetAllTransceiveTimeouts()
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

int NfcTag::GetTransceiveTimeout(int aTechId)
{
  int retval = 1000;
  if ((aTechId > 0) && (aTechId < (int)mTimeoutTable.size())) {
    retval = mTimeoutTable[aTechId];
  } else {
    NCI_ERROR("invalid tech=%d", aTechId);
  }

  return retval;
}
