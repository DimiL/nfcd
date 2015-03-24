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

#include "SecureElement.h"
#include "NfcDebug.h"
#include "PowerSwitch.h"
#include "config.h"
#include "NfcNciUtil.h"
#include "DeviceHost.h"
#include "NfcManager.h"

SecureElement SecureElement::sSecElem;
const char* SecureElement::APP_NAME = "nfc";

SecureElement::SecureElement()
 : mActiveEeHandle(NFA_HANDLE_INVALID)
 , mNfaHciHandle(NFA_HANDLE_INVALID)
 , mIsInit(false)
 , mActualNumEe(0)
 , mNumEePresent(0)
 , mbNewEE(true)   // by default we start w/thinking there are new EE
 , mActiveSeOverride(0)
 , mIsPiping(false)
 , mCurrentRouteSelection(NoRoute)
 , mActivatedInListenMode(false)
 , mRfFieldIsOn(false)
{
  memset(&mEeInfo, 0, sizeof(mEeInfo));
  memset(&mUiccInfo, 0, sizeof(mUiccInfo));
  memset(&mLastRfFieldToggle, 0, sizeof(mLastRfFieldToggle));
}

SecureElement::~SecureElement()
{
}

SecureElement& SecureElement::GetInstance()
{
  return sSecElem;
}

void SecureElement::SetActiveSeOverride(uint8_t aActiveSeOverride)
{
  NCI_DEBUG("seid=0x%X", aActiveSeOverride);
  mActiveSeOverride = aActiveSeOverride;
}

bool SecureElement::Initialize(NfcManager* aNfcManager)
{
  tNFA_STATUS nfaStat;
  unsigned long num = 0;

  // active SE, if not set active all SEs.
  if (GetNumValue("ACTIVE_SE", &num, sizeof(num))) {
    mActiveSeOverride = num;
  }
  NCI_DEBUG("Active SE override: 0x%X", mActiveSeOverride);

  mNfcManager = aNfcManager;

  mActiveEeHandle = NFA_HANDLE_INVALID;
  mNfaHciHandle   = NFA_HANDLE_INVALID;
  mActualNumEe    = MAX_NUM_EE;
  mbNewEE         = true;
  mRfFieldIsOn    = false;
  mActivatedInListenMode = false;
  mCurrentRouteSelection = NoRoute;
  mNumEePresent = 0;
  mIsPiping = false;
  memset(mEeInfo, 0, sizeof(mEeInfo));
  memset(&mUiccInfo, 0, sizeof(mUiccInfo));

  // Get Fresh EE info.
  if (!GetEeInfo()) {
    return false;
  }

  {
    SyncEventGuard guard(mEeRegisterEvent);
    NCI_DEBUG("try ee register");
    nfaStat = NFA_EeRegister(NfaEeCallback);
    if (nfaStat != NFA_STATUS_OK) {
      NCI_ERROR("fail ee register; error=0x%X", nfaStat);
      return false;
    }
    mEeRegisterEvent.Wait();
  }

  // If the controller has an HCI Network, register for that.
  for (size_t i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface <= 0) ||
        (mEeInfo[i].ee_interface[0] != NCI_NFCEE_INTERFACE_HCI_ACCESS)) {
      continue;
    }

    NCI_DEBUG("Found HCI network, try hci register");

    SyncEventGuard guard(mHciRegisterEvent);

    nfaStat = NFA_HciRegister(const_cast<char*>(APP_NAME), NfaHciCallback, true);
    if (nfaStat != NFA_STATUS_OK) {
      NCI_ERROR("fail hci register; error=0x%X", nfaStat);
      return false;
    }
    mHciRegisterEvent.Wait();
    break;
  }

  mRouteDataSet.Initialize();
  mRouteDataSet.Import();  //read XML file.

  mIsInit = true;

  return true;
}

void SecureElement::Finalize()
{
  NCI_DEBUG("enter");

  NFA_EeDeregister(NfaEeCallback);

  if (mNfaHciHandle != NFA_HANDLE_INVALID) {
    NFA_HciDeregister(const_cast<char*>(APP_NAME));
  }

  mIsInit       = false;
  mActualNumEe  = 0;
}

bool SecureElement::GetEeInfo()
{
  NCI_DEBUG("enter; mbNewEE=%d, mActualNumEe=%d", mbNewEE, mActualNumEe);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;

  if (!mbNewEE) {
    return (mActualNumEe != 0);
  }

  // If mbNewEE is true then there is new EE info.
  mActualNumEe = MAX_NUM_EE;

  if ((nfaStat = NFA_EeGetInfo(&mActualNumEe, mEeInfo)) != NFA_STATUS_OK) {
    NCI_ERROR("fail get info; error=0x%X", nfaStat);
    mActualNumEe = 0;
    return false;
  }

  mbNewEE = false;

  NCI_DEBUG("num EEs discovered: %u", mActualNumEe);
  for (uint8_t i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NCI_NFCEE_INTERFACE_HCI_ACCESS)) {
      mNumEePresent++;
    }

    NCI_DEBUG("EE[%u] Handle: 0x%04x  Status: %s  Num I/f: %u: (0x%02x, 0x%02x)  Num TLVs: %u",
              i,
              mEeInfo[i].ee_handle,
              EeStatusToString(mEeInfo[i].ee_status),
              mEeInfo[i].num_interface,
              mEeInfo[i].ee_interface[0],
              mEeInfo[i].ee_interface[1],
              mEeInfo[i].num_tlvs);

    for (size_t j = 0; j < mEeInfo[i].num_tlvs; j++) {
      NCI_DEBUG("EE[%u] TLV[%u]  Tag: 0x%02x  Len: %u  Values[]: 0x%02x  0x%02x  0x%02x ...",
                i, j,
                mEeInfo[i].ee_tlv[j].tag,
                mEeInfo[i].ee_tlv[j].len,
                mEeInfo[i].ee_tlv[j].info[0],
                mEeInfo[i].ee_tlv[j].info[1],
                mEeInfo[i].ee_tlv[j].info[2]);
    }
  }

  NCI_DEBUG("exit; mActualNumEe=%d, mNumEePresent=%d", mActualNumEe, mNumEePresent);
  return (mActualNumEe != 0);
}

/**
 * Computes time difference in milliseconds.
 */
static uint32_t TimeDiff(timespec aStart, timespec aEnd)
{
  aEnd.tv_sec -= aStart.tv_sec;
  aEnd.tv_nsec -= aStart.tv_nsec;

  if (aEnd.tv_nsec < 0) {
    aEnd.tv_nsec += 10e8;
    aEnd.tv_sec -=1;
  }

  return (aEnd.tv_sec * 1000) + (aEnd.tv_nsec / 10e5);
}

bool SecureElement::IsRfFieldOn()
{
  AutoMutex mutex(mMutex);
  if (mRfFieldIsOn) {
    return true;
  }

  struct timespec now;
  int ret = clock_gettime(CLOCK_MONOTONIC, &now);
  if (ret == -1) {
    NCI_ERROR("isRfFieldOn(): clock_gettime failed");
    return false;
  }

  // If it was less than 50ms ago that RF field
  // was turned off, still return ON.
  return (TimeDiff(mLastRfFieldToggle, now) < 50);
}

bool SecureElement::IsActivatedInListenMode()
{
  return mActivatedInListenMode;
}

void SecureElement::GetListOfEeHandles(std::vector<uint32_t>& aListSe)
{
  NCI_DEBUG("enter");
  if (mNumEePresent == 0 || !mIsInit || !GetEeInfo()) {
    return;
  }

  int cnt = 0;
  for (int i = 0; i < mActualNumEe && cnt < mNumEePresent; i++) {
    NCI_DEBUG("%u = 0x%X", i, mEeInfo[i].ee_handle);
    if ((mEeInfo[i].num_interface == 0) ||
        (mEeInfo[i].ee_interface[0] == NCI_NFCEE_INTERFACE_HCI_ACCESS)) {
      continue;
    }
    uint32_t handle = mEeInfo[i].ee_handle & ~NFA_HANDLE_GROUP_EE;
    aListSe.push_back(handle);
    cnt++;
  }
}

bool SecureElement::Activate()
{
  int numActivatedEe = 0;

  NCI_DEBUG("enter;");

  if (!mIsInit) {
    NCI_ERROR("not init");
    return false;
  }

  if (mActiveEeHandle != NFA_HANDLE_INVALID) {
    NCI_DEBUG("already active");
    return true;
  }

  // Get Fresh EE info if needed.
  if (!GetEeInfo()) {
    NCI_ERROR("no EE info");
    return false;
  }

  uint16_t overrideEeHandle =
    mActiveSeOverride ? NFA_HANDLE_GROUP_EE | mActiveSeOverride : 0;

  if (mRfFieldIsOn) {
    NCI_ERROR("RF field indication still on, resetting");
    mRfFieldIsOn = false;
  }

  NCI_DEBUG("override ee h=0x%X", overrideEeHandle);
  //activate every discovered secure element
  for (int index = 0; index < mActualNumEe; index++) {
    tNFA_EE_INFO& eeItem = mEeInfo[index];
    if ((eeItem.ee_handle != EE_HANDLE_0xF3) &&
        (eeItem.ee_handle != EE_HANDLE_0xF4) &&
        (eeItem.ee_handle != EE_HANDLE_0x01) &&
        (eeItem.ee_handle != EE_HANDLE_0x02)) {
      continue;
    }

    if (overrideEeHandle != eeItem.ee_handle) {
      continue;   // do not enable all SEs; only the override one
    }

    if (eeItem.ee_status != NFC_NFCEE_STATUS_INACTIVE) {
      NCI_DEBUG("h=0x%X already activated", eeItem.ee_handle);
      numActivatedEe++;
      continue;
    }

    {
      tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
      SyncEventGuard guard(mEeSetModeEvent);
      NCI_DEBUG("set EE mode activate; h=0x%X", eeItem.ee_handle);
      if ((nfaStat = NFA_EeModeSet(eeItem.ee_handle, NFA_EE_MD_ACTIVATE)) == NFA_STATUS_OK) {
        mEeSetModeEvent.Wait(); //wait for NFA_EE_MODE_SET_EVT
        if (eeItem.ee_status == NFC_NFCEE_STATUS_ACTIVE) {
          numActivatedEe++;
        }
      }else {
        NCI_ERROR("NFA_EeModeSet failed; error=0x%X", nfaStat);
      }
    } //for
  }

  mActiveEeHandle = GetDefaultEeHandle();

  NCI_DEBUG("exit; active ee h=0x%X", mActiveEeHandle);

  return mActiveEeHandle != NFA_HANDLE_INVALID;
}

bool SecureElement::Deactivate()
{
  bool retval = false;

  NCI_DEBUG("enter; mActiveEeHandle=0x%X", mActiveEeHandle);

  if (!mIsInit) {
    NCI_ERROR ("not init");
    return retval;
  }

  // if the controller is routing to sec elems or piping,
  // then the secure element cannot be deactivated
  if (IsBusy()) {
    NCI_ERROR ("still busy");
    return retval;
  } else if (mActiveEeHandle == NFA_HANDLE_INVALID) {
    NCI_ERROR("invalid EE handle");
    return retval;
  }

  mActiveEeHandle = NFA_HANDLE_INVALID;
  retval = true;

  NCI_DEBUG("exit; ok=%u", retval);
  return retval;
}

void SecureElement::NotifyTransactionEvent(const uint8_t* aAid,
                                           uint32_t aAidLen,
                                           const uint8_t* aPayload,
                                           uint32_t aPayloadLen)
{
  if (aAidLen == 0) {
    return;
  }

  TransactionEvent* pTransaction = new TransactionEvent();

  // TODO: For now, we dodn't have a solution to get aid origin from nfcd.
  //       So use SIM1 as dfault value.
  pTransaction->originType = TransactionEvent::SIM;
  pTransaction->originIndex = 1;

  pTransaction->aidLen = aAidLen;
  pTransaction->aid = new uint8_t[aAidLen];
  memcpy(pTransaction->aid, aAid, aAidLen);

  pTransaction->payloadLen = aPayloadLen;
  pTransaction->payload = new uint8_t[aPayloadLen];
  memcpy(pTransaction->payload, aPayload, aPayloadLen);

  mNfcManager->NotifyTransactionEvent(pTransaction);
}

void SecureElement::NotifyListenModeState(bool aIsActivated)
{
  NCI_DEBUG("enter; listen mode active=%u", aIsActivated);

  // TODO Implement notify.
  mActivatedInListenMode = aIsActivated;
}

void SecureElement::NotifyRfFieldEvent(bool aIsActive)
{
  // NCI_DEBUG("enter; is active=%u", aIsActive);

  // TODO Implement
  mMutex.Lock();
  int ret = clock_gettime(CLOCK_MONOTONIC, &mLastRfFieldToggle);
  if (ret == -1) {
    NCI_ERROR("clock_gettime failed");
    // There is no good choice here...
  }

  mRfFieldIsOn = aIsActive;
  mMutex.Unlock();
}

void SecureElement::ResetRfFieldStatus()
{
  NCI_DEBUG("enter;");

  mMutex.Lock();
  mRfFieldIsOn = false;
  int ret = clock_gettime(CLOCK_MONOTONIC, &mLastRfFieldToggle);
  if (ret == -1) {
    NCI_ERROR("clock_gettime failed");
    // There is no good choice here...
  }
  mMutex.Unlock();
}

void SecureElement::StoreUiccInfo(tNFA_EE_DISCOVER_REQ& aInfo)
{
  NCI_DEBUG("Status: %u   Num EE: %u", aInfo.status, aInfo.num_ee);

  SyncEventGuard guard(mUiccInfoEvent);
  memcpy(&mUiccInfo, &aInfo, sizeof(mUiccInfo));
  for (uint8_t i = 0; i < aInfo.num_ee; i++) {
    //for each technology (A, B, F, B'), print the bit field that shows
    //what protocol(s) is support by that technology
    NCI_DEBUG("EE[%u] Handle: 0x%04x  techA: 0x%02x  techB: 0x%02x  techF: 0x%02x  techBprime: 0x%02x",
              i,
              aInfo.ee_disc_info[i].ee_handle,
              aInfo.ee_disc_info[i].la_protocol,
              aInfo.ee_disc_info[i].lb_protocol,
              aInfo.ee_disc_info[i].lf_protocol,
              aInfo.ee_disc_info[i].lbp_protocol);
  }
  mUiccInfoEvent.NotifyOne();
}

void SecureElement::AdjustRoutes(RouteSelection aSelection)
{
  NCI_DEBUG("enter; selection=%u", aSelection);

  mCurrentRouteSelection = aSelection;
  AdjustProtocolRoutes(aSelection);
  AdjustTechnologyRoutes(aSelection);

  NFA_EeUpdateNow(); //apply new routes now.
}

void SecureElement::AdjustProtocolRoutes(RouteSelection aRouteSelection)
{
  NCI_DEBUG("enter");

  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  const tNFA_PROTOCOL_MASK protoMask = NFA_PROTOCOL_MASK_ISO_DEP;

  /**
   * delete route to host
   */
  {
    NCI_DEBUG("delete route to host");
    SyncEventGuard guard(mRoutingEvent);
    if ((nfaStat = NFA_EeSetDefaultProtoRouting(NFA_EE_HANDLE_DH, 0, 0, 0)) == NFA_STATUS_OK) {
      mRoutingEvent.Wait();
    } else {
      NCI_ERROR("fail delete route to host; error=0x%X", nfaStat);
    }
  }

  /**
   * delete route to every sec elem
   */
  for (int i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NFC_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status == NFA_EE_STATUS_ACTIVE)) {
      NCI_DEBUG("delete route to EE h=0x%X", mEeInfo[i].ee_handle);
      SyncEventGuard guard(mRoutingEvent);
      if ((nfaStat = NFA_EeSetDefaultProtoRouting(mEeInfo[i].ee_handle, 0, 0, 0)) == NFA_STATUS_OK) {
        mRoutingEvent.Wait();
      } else {
        NCI_ERROR("fail delete route to EE; error=0x%X", nfaStat);
      }
    }
  }

  /**
   * if route database is empty, setup a default route.
   */
  if (true) {
    tNFA_HANDLE eeHandle =
      (aRouteSelection == SecElemRoute) ? mActiveEeHandle : NFA_EE_HANDLE_DH;

    NCI_DEBUG("route to default EE h=0x%X", eeHandle);
    SyncEventGuard guard(mRoutingEvent);
    nfaStat = NFA_EeSetDefaultProtoRouting(eeHandle, protoMask, 0, 0);
    if (nfaStat == NFA_STATUS_OK) {
      mRoutingEvent.Wait();
    } else {
      NCI_ERROR("fail route to EE; error=0x%X", nfaStat);
    }
  }
  NCI_DEBUG("exit");
}

void SecureElement::AdjustTechnologyRoutes(RouteSelection aRouteSelection)
{
  NCI_DEBUG("enter");

  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  const tNFA_TECHNOLOGY_MASK techMask = NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B;

  /**
   * delete route to host.
   */
  {
    NCI_DEBUG("delete route to host");
    SyncEventGuard guard(mRoutingEvent);
    if ((nfaStat = NFA_EeSetDefaultTechRouting(NFA_EE_HANDLE_DH, 0, 0, 0)) == NFA_STATUS_OK) {
      mRoutingEvent.Wait();
    } else {
      NCI_ERROR("fail delete route to host; error=0x%X", nfaStat);
    }
  }

  /**
   * delete route to every sec elem.
   */
  for (int i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NFC_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status == NFA_EE_STATUS_ACTIVE)) {
      NCI_DEBUG("delete route to EE h=0x%X", mEeInfo[i].ee_handle);
      SyncEventGuard guard(mRoutingEvent);
      if ((nfaStat = NFA_EeSetDefaultTechRouting (mEeInfo[i].ee_handle, 0, 0, 0)) == NFA_STATUS_OK) {
        mRoutingEvent.Wait();
      } else {
        NCI_ERROR("fail delete route to EE; error=0x%X", nfaStat);
      }
    }
  }

  /**
   * if route database is empty, setup a default route.
   */
  if (true) {
    tNFA_HANDLE eeHandle =
      (aRouteSelection == SecElemRoute) ? mActiveEeHandle : NFA_EE_HANDLE_DH;

    NCI_DEBUG("route to default EE h=0x%X", eeHandle);
    SyncEventGuard guard(mRoutingEvent);
    nfaStat = NFA_EeSetDefaultTechRouting(eeHandle, techMask, 0, 0);
    if (nfaStat == NFA_STATUS_OK) {
      mRoutingEvent.Wait();
    } else {
      NCI_ERROR("fail route to EE; error=0x%X", nfaStat);
    }
  }
}

void SecureElement::NfaEeCallback(tNFA_EE_EVT aEvent,
                                  tNFA_EE_CBACK_DATA* aEventData)
{
  NCI_DEBUG("event=0x%X", aEvent);
  switch (aEvent) {
    case NFA_EE_REGISTER_EVT: {
      SyncEventGuard guard (sSecElem.mEeRegisterEvent);
      NCI_DEBUG("NFA_EE_REGISTER_EVT; status=%u", aEventData->ee_register);
      sSecElem.mEeRegisterEvent.NotifyOne();
      break;
    }
    case NFA_EE_MODE_SET_EVT: {
      NCI_DEBUG("NFA_EE_MODE_SET_EVT; status: 0x%04X  handle: 0x%04X  mActiveEeHandle: 0x%04X",
                aEventData->mode_set.status,
                aEventData->mode_set.ee_handle,
                sSecElem.mActiveEeHandle);

      if (aEventData->mode_set.status == NFA_STATUS_OK) {
        tNFA_EE_INFO *pEE = sSecElem.FindEeByHandle (aEventData->mode_set.ee_handle);
        if (pEE) {
          pEE->ee_status ^= 1;
          NCI_DEBUG("NFA_EE_MODE_SET_EVT; pEE->ee_status: %s (0x%04x)",
                    SecureElement::EeStatusToString(pEE->ee_status), pEE->ee_status);
        } else {
          NCI_ERROR("NFA_EE_MODE_SET_EVT; EE: 0x%04x not found.  mActiveEeHandle: 0x%04x",
                    aEventData->mode_set.ee_handle, sSecElem.mActiveEeHandle);
        }
      }
      SyncEventGuard guard(sSecElem.mEeSetModeEvent);
      sSecElem.mEeSetModeEvent.NotifyOne();
      break;
    }
    case NFA_EE_SET_TECH_CFG_EVT: {
      NCI_DEBUG("NFA_EE_SET_TECH_CFG_EVT; status=0x%X", aEventData->status);
      SyncEventGuard guard(sSecElem.mRoutingEvent);
      sSecElem.mRoutingEvent.NotifyOne();
      break;
    }
    case NFA_EE_SET_PROTO_CFG_EVT: {
      NCI_DEBUG("NFA_EE_SET_PROTO_CFG_EVT; status=0x%X", aEventData->status);
      SyncEventGuard guard(sSecElem.mRoutingEvent);
      sSecElem.mRoutingEvent.NotifyOne();
      break;
    }
    case NFA_EE_DISCOVER_REQ_EVT: {
      NCI_DEBUG("NFA_EE_DISCOVER_REQ_EVT; status=0x%X; num ee=%u",
                aEventData->discover_req.status,
                aEventData->discover_req.num_ee);
      sSecElem.StoreUiccInfo(aEventData->discover_req);
      break;
    }
    case NFA_EE_ADD_AID_EVT: {
      NCI_DEBUG("NFA_EE_ADD_AID_EVT  status=%u", aEventData->status);
      SyncEventGuard guard(sSecElem.mAidAddRemoveEvent);
      sSecElem.mAidAddRemoveEvent.NotifyOne();
      break;
    }
    case NFA_EE_REMOVE_AID_EVT: {
      NCI_DEBUG("NFA_EE_REMOVE_AID_EVT  status=%u", aEventData->status);
      SyncEventGuard guard(sSecElem.mAidAddRemoveEvent);
      sSecElem.mAidAddRemoveEvent.NotifyOne();
      break;
    }
    case NFA_EE_NEW_EE_EVT: {
      NCI_DEBUG("NFA_EE_NEW_EE_EVT  h=0x%X; status=%u",
                aEventData->new_ee.ee_handle, aEventData->new_ee.ee_status);
      // Indicate there are new EE
      sSecElem.mbNewEE = true;
      break;
    }
    default:
      NCI_ERROR("unknown event=%u ????", aEvent);
      break;
  }
}

tNFA_EE_INFO *SecureElement::FindEeByHandle(tNFA_HANDLE aEeHandle)
{
  for (uint8_t i = 0; i < mActualNumEe; i++) {
    if (mEeInfo[i].ee_handle == aEeHandle) {
      return &mEeInfo[i];
    }
  }
  return (NULL);
}

void SecureElement::NfaHciCallback(tNFA_HCI_EVT aEvent,
                                   tNFA_HCI_EVT_DATA* aEventData)
{
  NCI_DEBUG("event=0x%X", aEvent);

  switch (aEvent) {
    case NFA_HCI_REGISTER_EVT: {
      NCI_DEBUG("NFA_HCI_REGISTER_EVT; status=0x%X; handle=0x%X",
                aEventData->hci_register.status,
                aEventData->hci_register.hci_handle);
      SyncEventGuard guard(sSecElem.mHciRegisterEvent);
      sSecElem.mNfaHciHandle = aEventData->hci_register.hci_handle;
      sSecElem.mHciRegisterEvent.NotifyOne();
      break;
    }
    case NFA_HCI_EVENT_RCVD_EVT: {
      NCI_DEBUG("NFA_HCI_EVENT_RCVD_EVT; code: 0x%X; pipe: 0x%X; data len: %u",
                aEventData->rcvd_evt.evt_code, aEventData->rcvd_evt.pipe,
                aEventData->rcvd_evt.evt_len);
      if (aEventData->rcvd_evt.evt_code == NFA_HCI_EVT_TRANSACTION) {
        uint8_t aidLen = 0;
        uint8_t payloadLen = 0;
        NCI_DEBUG("NFA_HCI_EVENT_RCVD_EVT; NFA_HCI_EVT_TRANSACTION");
        // If we got an AID, notify any listeners.
        if ((aEventData->rcvd_evt.evt_len > 3) &&
            (aEventData->rcvd_evt.p_evt_buf[0] == 0x81)) {
          aidLen = aEventData->rcvd_evt.p_evt_buf[1];
        }
        if ((aEventData->rcvd_evt.evt_len > (3 + aidLen)) &&
            (aEventData->rcvd_evt.p_evt_buf[2 + aidLen] == 0x82)) {
          payloadLen = aEventData->rcvd_evt.p_evt_buf[3 + aidLen];
        }
        if (aidLen) {
          sSecElem.NotifyTransactionEvent(
            aEventData->rcvd_evt.p_evt_buf + 2,
            aidLen,
            aEventData->rcvd_evt.p_evt_buf + 4 + aidLen,
            payloadLen
          );
        }
      }
      break;
    }
    default:
      NCI_ERROR("unknown event code=0x%X ????", aEvent);
      break;
  }
}


tNFA_HANDLE SecureElement::GetDefaultEeHandle()
{
  uint16_t overrideEeHandle = NFA_HANDLE_GROUP_EE | mActiveSeOverride;
  // Find the first EE that is not the HCI Access i/f.
  for (uint8_t i = 0; i < mActualNumEe; i++) {
    if (mActiveSeOverride && (overrideEeHandle != mEeInfo[i].ee_handle)) {
      continue; //skip all the EE's that are ignored
    }
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NCI_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status != NFC_NFCEE_STATUS_INACTIVE)) {
      return mEeInfo[i].ee_handle;
    }
  }

  NCI_ERROR("ee handle not found");
  return NFA_HANDLE_INVALID;
}

const char* SecureElement::EeStatusToString(uint8_t aStatus)
{
  switch (aStatus) {
    case NFC_NFCEE_STATUS_ACTIVE:
      return "Connected/Active";
    case NFC_NFCEE_STATUS_INACTIVE:
      return "Connected/Inactive";
    case NFC_NFCEE_STATUS_REMOVED:
      return "Removed";
    default:
      return "?? Unknown ??";
  }
}

void SecureElement::ConnectionEventHandler(uint8_t aEvent,
                                           tNFA_CONN_EVT_DATA* /*aEventData*/)
{
  switch (aEvent) {
    case NFA_CE_UICC_LISTEN_CONFIGURED_EVT: {
      SyncEventGuard guard(mUiccListenEvent);
      mUiccListenEvent.NotifyOne();
      break;
    }
  }
}

bool SecureElement::RouteToSecureElement()
{
  NCI_DEBUG("enter");
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  tNFA_TECHNOLOGY_MASK tech_mask = NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B;
  bool retval = false;

  if (!mIsInit) {
    NCI_ERROR("not init");
    return false;
  }

  if (mCurrentRouteSelection == SecElemRoute) {
    NCI_ERROR("already sec elem route");
    return true;
  }

  if (mActiveEeHandle == NFA_HANDLE_INVALID) {
    NCI_ERROR("invalid EE handle");
    return false;
  }

  AdjustRoutes(SecElemRoute);

  {
    unsigned long num = 0;
    if (GetNumValue("UICC_LISTEN_TECH_MASK", &num, sizeof(num))) {
      tech_mask = num;
    }

    NCI_DEBUG("start UICC listen; h=0x%X; tech mask=0x%X",
            mActiveEeHandle, tech_mask);
    SyncEventGuard guard(mUiccListenEvent);
    nfaStat = NFA_CeConfigureUiccListenTech(mActiveEeHandle, tech_mask);
    if (nfaStat == NFA_STATUS_OK) {
      mUiccListenEvent.Wait();
      retval = true;
    } else {
      NCI_ERROR("fail to start UICC listen");
    }
  }

  return retval;
}

bool SecureElement::RouteToDefault()
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  bool retval = false;

  NCI_DEBUG("enter");
  if (!mIsInit) {
    NCI_ERROR("not init");
    return false;
  }

  if (mCurrentRouteSelection == DefaultRoute) {
    NCI_DEBUG("already default route");
    return true;
  }

  if (mActiveEeHandle != NFA_HANDLE_INVALID) {
    NCI_DEBUG("stop UICC listen; EE h=0x%X", mActiveEeHandle);
    SyncEventGuard guard(mUiccListenEvent);
    nfaStat = NFA_CeConfigureUiccListenTech(mActiveEeHandle, 0);
    if (nfaStat == NFA_STATUS_OK) {
      mUiccListenEvent.Wait();
      retval = true;
    } else {
      NCI_ERROR("fail to stop UICC listen");
    }
  } else {
    retval = true;
  }

  AdjustRoutes(DefaultRoute);

  NCI_DEBUG("exit; ok=%u", retval);
  return retval;
}

bool SecureElement::IsBusy()
{
  bool retval = (mCurrentRouteSelection == SecElemRoute) || mIsPiping;
  NCI_DEBUG("%u", retval);
  return retval;
}
