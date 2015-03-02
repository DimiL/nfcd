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
#include "PowerSwitch.h"
#include "config.h"
#include "NfcNciUtil.h"
#include "DeviceHost.h"
#include "NfcManager.h"

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

SecureElement SecureElement::sSecElem;
const char* SecureElement::APP_NAME = "nfc";

SecureElement::SecureElement()
 : mActiveEeHandle(NFA_HANDLE_INVALID)
 , mNfaHciHandle(NFA_HANDLE_INVALID)
 , mIsInit(false)
 , mActualNumEe(0)
 , mNumEePresent(0)
 , mActiveSeOverride(0)
 , mIsPiping(false)
 , mActivatedInListenMode(false)
 , mRfFieldIsOn(false)
{
  memset(&mEeInfo, 0, sizeof(mEeInfo));
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
  ALOGD("%s, seid=0x%X", __FUNCTION__, aActiveSeOverride);
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
  ALOGD("%s: Active SE override: 0x%X", __FUNCTION__, mActiveSeOverride);

  mNfcManager = aNfcManager;

  mActiveEeHandle = NFA_HANDLE_INVALID;
  mNfaHciHandle   = NFA_HANDLE_INVALID;
  mActualNumEe    = MAX_NUM_EE;
  mRfFieldIsOn    = false;
  mActivatedInListenMode = false;
  mNumEePresent = 0;
  mIsPiping = false;
  memset(mEeInfo, 0, sizeof(mEeInfo));

  // Get Fresh EE info.
  if (!GetEeInfo()) {
    return false;
  }

  // If the controller has an HCI Network, register for that.
  for (size_t i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface <= 0) ||
        (mEeInfo[i].ee_interface[0] != NCI_NFCEE_INTERFACE_HCI_ACCESS)) {
      continue;
    }

    ALOGD("%s: Found HCI network, try hci register", __FUNCTION__);

    SyncEventGuard guard(mHciRegisterEvent);

    nfaStat = NFA_HciRegister(const_cast<char*>(APP_NAME), NfaHciCallback, true);
    if (nfaStat != NFA_STATUS_OK) {
      ALOGE("%s: fail hci register; error=0x%X", __FUNCTION__, nfaStat);
      return false;
    }
    mHciRegisterEvent.Wait();
    break;
  }

  mIsInit = true;

  return true;
}

void SecureElement::Finalize()
{
  ALOGD("%s: enter", __FUNCTION__);

  if (mNfaHciHandle != NFA_HANDLE_INVALID) {
    NFA_HciDeregister(const_cast<char*>(APP_NAME));
  }

  mIsInit       = false;
  mActualNumEe  = 0;
}

bool SecureElement::GetEeInfo()
{
  ALOGD("%s: enter; mActualNumEe=%d", __FUNCTION__,  mActualNumEe);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;

  mActualNumEe = MAX_NUM_EE;

  if ((nfaStat = NFA_EeGetInfo(&mActualNumEe, mEeInfo)) != NFA_STATUS_OK) {
    ALOGE("%s: fail get info; error=0x%X", __FUNCTION__, nfaStat);
    mActualNumEe = 0;
    return false;
  }

  ALOGD("%s: num EEs discovered: %u", __FUNCTION__, mActualNumEe);
  for (uint8_t i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NCI_NFCEE_INTERFACE_HCI_ACCESS)) {
      mNumEePresent++;
    }

    ALOGD("%s: EE[%u] Handle: 0x%04x  Status: %s  Num I/f: %u: (0x%02x, 0x%02x)  Num TLVs: %u",
          __FUNCTION__, i,
          mEeInfo[i].ee_handle,
          EeStatusToString(mEeInfo[i].ee_status),
          mEeInfo[i].num_interface,
          mEeInfo[i].ee_interface[0],
          mEeInfo[i].ee_interface[1],
          mEeInfo[i].num_tlvs);

    for (size_t j = 0; j < mEeInfo[i].num_tlvs; j++) {
      ALOGD("%s: EE[%u] TLV[%u]  Tag: 0x%02x  Len: %u  Values[]: 0x%02x  0x%02x  0x%02x ...",
            __FUNCTION__, i, j,
            mEeInfo[i].ee_tlv[j].tag,
            mEeInfo[i].ee_tlv[j].len,
            mEeInfo[i].ee_tlv[j].info[0],
            mEeInfo[i].ee_tlv[j].info[1],
            mEeInfo[i].ee_tlv[j].info[2]);
    }
  }

  ALOGD("%s: exit; mActualNumEe=%d, mNumEePresent=%d", __FUNCTION__, mActualNumEe, mNumEePresent);
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
    ALOGE("isRfFieldOn(): clock_gettime failed");
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
  ALOGD("%s: enter", __FUNCTION__);
  if (mNumEePresent == 0 || !mIsInit || !GetEeInfo()) {
    return;
  }

  int cnt = 0;
  for (int i = 0; i < mActualNumEe && cnt < mNumEePresent; i++) {
    ALOGD("%s: %u = 0x%X", __FUNCTION__, i, mEeInfo[i].ee_handle);
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

  ALOGD("%s: enter;", __FUNCTION__);

  if (!mIsInit) {
    ALOGE("%s: not init", __FUNCTION__);
    return false;
  }

  if (mActiveEeHandle != NFA_HANDLE_INVALID) {
    ALOGD("%s: already active", __FUNCTION__);
    return true;
  }

  // Get Fresh EE info if needed.
  if (!GetEeInfo()) {
    ALOGE("%s: no EE info", __FUNCTION__);
    return false;
  }

  uint16_t overrideEeHandle =
    mActiveSeOverride ? NFA_HANDLE_GROUP_EE | mActiveSeOverride : 0;

  if (mRfFieldIsOn) {
    ALOGE("%s: RF field indication still on, resetting", __FUNCTION__);
    mRfFieldIsOn = false;
  }

  ALOGD("%s: override ee h=0x%X", __FUNCTION__, overrideEeHandle);
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
      ALOGD("%s: h=0x%X already activated", __FUNCTION__, eeItem.ee_handle);
      numActivatedEe++;
      continue;
    }

    {
      tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
      SyncEventGuard guard(mEeSetModeEvent);
      ALOGD("%s: set EE mode activate; h=0x%X", __FUNCTION__, eeItem.ee_handle);
      if ((nfaStat = NFA_EeModeSet(eeItem.ee_handle, NFA_EE_MD_ACTIVATE)) == NFA_STATUS_OK) {
        mEeSetModeEvent.Wait(); //wait for NFA_EE_MODE_SET_EVT
        if (eeItem.ee_status == NFC_NFCEE_STATUS_ACTIVE) {
          numActivatedEe++;
        }
      }else {
        ALOGE("%s: NFA_EeModeSet failed; error=0x%X", __FUNCTION__, nfaStat);
      }
    } //for
  }

  mActiveEeHandle = GetDefaultEeHandle();

  ALOGD("%s: exit; active ee h=0x%X", __FUNCTION__, mActiveEeHandle);

  return mActiveEeHandle != NFA_HANDLE_INVALID;
}

bool SecureElement::Deactivate()
{
  bool retval = false;

  ALOGD("%s: enter; mActiveEeHandle=0x%X", __FUNCTION__, mActiveEeHandle);

  if (!mIsInit) {
    ALOGE ("%s: not init", __FUNCTION__);
    return retval;
  }

  if (mActiveEeHandle == NFA_HANDLE_INVALID) {
    ALOGE("%s: invalid EE handle", __FUNCTION__);
    return retval;
  }

  mActiveEeHandle = NFA_HANDLE_INVALID;
  retval = true;

  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
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
  ALOGD("%s: enter; listen mode active=%u", __FUNCTION__, aIsActivated);

  // TODO Implement notify.
  mActivatedInListenMode = aIsActivated;
}

void SecureElement::NotifyRfFieldEvent(bool aIsActive)
{
  ALOGD("%s: enter; is active=%u", __FUNCTION__, aIsActive);

  // TODO Implement
  mMutex.Lock();
  int ret = clock_gettime(CLOCK_MONOTONIC, &mLastRfFieldToggle);
  if (ret == -1) {
    ALOGE("%s: clock_gettime failed", __FUNCTION__);
    // There is no good choice here...
  }

  mRfFieldIsOn = aIsActive;
  mMutex.Unlock();
}

void SecureElement::ResetRfFieldStatus()
{
  ALOGD ("%s: enter;", __FUNCTION__);

  mMutex.Lock();
  mRfFieldIsOn = false;
  int ret = clock_gettime(CLOCK_MONOTONIC, &mLastRfFieldToggle);
  if (ret == -1) {
    ALOGE("%s: clock_gettime failed", __FUNCTION__);
    // There is no good choice here...
  }
  mMutex.Unlock();
}

void SecureElement::NotifyModeSet(tNFA_HANDLE aEeHandle, bool aSuccess)
{
  if (aSuccess) {
    tNFA_EE_INFO *pEE = FindEeByHandle(aEeHandle);
    if (pEE) {
      pEE->ee_status ^= 1;
    }
  }

  SyncEventGuard guard(mEeSetModeEvent);
  mEeSetModeEvent.NotifyOne();
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
  ALOGD("%s: event=0x%X", __FUNCTION__, aEvent);

  switch (aEvent) {
    case NFA_HCI_REGISTER_EVT: {
      ALOGD("%s: NFA_HCI_REGISTER_EVT; status=0x%X; handle=0x%X",
            __FUNCTION__, aEventData->hci_register.status, aEventData->hci_register.hci_handle);
      SyncEventGuard guard(sSecElem.mHciRegisterEvent);
      sSecElem.mNfaHciHandle = aEventData->hci_register.hci_handle;
      sSecElem.mHciRegisterEvent.NotifyOne();
      break;
    }
    case NFA_HCI_EVENT_RCVD_EVT: {
      ALOGD("%s: NFA_HCI_EVENT_RCVD_EVT; code: 0x%X; pipe: 0x%X; data len: %u",
            __FUNCTION__, aEventData->rcvd_evt.evt_code, aEventData->rcvd_evt.pipe,
            aEventData->rcvd_evt.evt_len);
      if (aEventData->rcvd_evt.evt_code == NFA_HCI_EVT_TRANSACTION) {
        uint8_t aidLen = 0;
        uint8_t payloadLen = 0;
        ALOGD ("%s: NFA_HCI_EVENT_RCVD_EVT; NFA_HCI_EVT_TRANSACTION", __FUNCTION__);
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
      ALOGE("%s: unknown event code=0x%X ????", __FUNCTION__, aEvent);
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

  ALOGE("%s: ee handle not found", __FUNCTION__);
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
//      SyncEventGuard guard(mUiccListenEvent);
//      mUiccListenEvent.notifyOne();
      break;
    }
  }
}

bool SecureElement::RouteToSecureElement()
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  tNFA_TECHNOLOGY_MASK tech_mask = NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B;
  bool retval = false;

  if (!mIsInit) {
    ALOGE("%s: not init", __FUNCTION__);
    return false;
  }

  if (mActiveEeHandle == NFA_HANDLE_INVALID) {
    ALOGE("%s: invalid EE handle", __FUNCTION__);
    return false;
  }

  return retval;
}

bool SecureElement::RouteToDefault()
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  bool retval = false;

  ALOGD("%s: enter", __FUNCTION__);
  if (!mIsInit) {
    ALOGE("%s: not init", __FUNCTION__);
    return false;
  }

  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
  return retval;
}

