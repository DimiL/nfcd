/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HostAidRouter.h"
#include "config.h"
#include "SecureElement.h"

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

HostAidRouter HostAidRouter::sHostAidRouter; //singleton HostAidRouter object.

HostAidRouter::HostAidRouter()
 : mTempHandle(NFA_HANDLE_INVALID)
 , mIsFeatureEnabled(true)
{
}

HostAidRouter::~HostAidRouter()
{
}

HostAidRouter& HostAidRouter::getInstance()
{
  return sHostAidRouter;
}

bool HostAidRouter::initialize()
{
  unsigned long num = 0;
  mTempHandle = NFA_HANDLE_INVALID;
  mHandleDatabase.clear();

  if (GetNumValue(NAME_REGISTER_VIRTUAL_SE, &num, sizeof(num))) {
    mIsFeatureEnabled = num != 0;
  }
  return true;
}

bool HostAidRouter::addPpseRoute()
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  bool retval = false;

  if (!mIsFeatureEnabled) {
    ALOGD("%s: feature disabled", __FUNCTION__);
    goto TheEnd;
  }

  {
    ALOGD("%s: register PPSE AID", __FUNCTION__);
    SyncEventGuard guard(mRegisterEvent);
    mTempHandle = NFA_HANDLE_INVALID;
    nfaStat = NFA_CeRegisterAidOnDH((uint8_t*) "2PAY.SYS.DDF01", 14, stackCallback);
    if (nfaStat == NFA_STATUS_OK) {
      mRegisterEvent.wait(); //wait for NFA_CE_REGISTERED_EVT
      if (mTempHandle == NFA_HANDLE_INVALID) {
        ALOGE("%s: received invalid handle", __FUNCTION__);
        goto TheEnd;
      } else {
        mHandleDatabase.push_back(mTempHandle);
      }
    } else {
      ALOGE("%s: fail register", __FUNCTION__);
      goto TheEnd;
    }
  }
  retval = true;

TheEnd:
  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
  return retval;
}

bool HostAidRouter::deleteAllRoutes()
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  bool retval = false;

  if (!mIsFeatureEnabled) {
    ALOGD("%s: feature disabled", __FUNCTION__);
    goto TheEnd;
  }

  //deregister each registered AID from the stack
  for (AidHandleDatabase::iterator iter1 = mHandleDatabase.begin();
       iter1 != mHandleDatabase.end(); iter1++) {
    tNFA_HANDLE aidHandle = *iter1;
    ALOGD("%s: deregister h=0x%X", __FUNCTION__, aidHandle);
    SyncEventGuard guard(mDeregisterEvent);
    nfaStat = NFA_CeDeregisterAidOnDH(aidHandle);
    if (nfaStat == NFA_STATUS_OK) {
      mDeregisterEvent.wait(); //wait for NFA_CE_DEREGISTERED_EVT
    } else {
      ALOGE("%s: fail deregister", __FUNCTION__);
    }
  }
  mHandleDatabase.clear();
  retval = true;

TheEnd:
  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
  return retval;
}

bool HostAidRouter::startRoute(const uint8_t* aid, uint8_t aidLen)
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  bool retval = false;

  if (!mIsFeatureEnabled) {
    ALOGD("%s: feature disabled", __FUNCTION__);
    goto TheEnd;
  }

  {
    ALOGD("%s: register AID; len=%u", __FUNCTION__, aidLen);
    SyncEventGuard guard(mRegisterEvent);
    mTempHandle = NFA_HANDLE_INVALID;
    nfaStat = NFA_CeRegisterAidOnDH((UINT8*) aid, aidLen, stackCallback);
    if (nfaStat == NFA_STATUS_OK) {
      mRegisterEvent.wait(); //wait for NFA_CE_REGISTERED_EVT.
      if (mTempHandle == NFA_HANDLE_INVALID) {
        ALOGE("%s: received invalid handle", __FUNCTION__);
        goto TheEnd;
      } else {
        mHandleDatabase.push_back(mTempHandle);
      }
    } else {
      ALOGE("%s: fail register", __FUNCTION__);
      goto TheEnd;
    }
  }

TheEnd:
  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
  return retval;
}

void HostAidRouter::stackCallback(uint8_t event, tNFA_CONN_EVT_DATA* eventData)
{
  ALOGD("%s: event=0x%X", __FUNCTION__, event);

  switch (event) {
  case NFA_CE_REGISTERED_EVT:
    {
      tNFA_CE_REGISTERED& ce_registered = eventData->ce_registered;
      ALOGD("%s: NFA_CE_REGISTERED_EVT; status=0x%X; h=0x%X",
            __FUNCTION__, ce_registered.status, ce_registered.handle);
      SyncEventGuard guard(sHostAidRouter.mRegisterEvent);
      if (ce_registered.status == NFA_STATUS_OK) {
        sHostAidRouter.mTempHandle = ce_registered.handle;
      } else {
        sHostAidRouter.mTempHandle = NFA_HANDLE_INVALID;
      }
      sHostAidRouter.mRegisterEvent.notifyOne();
    }
    break;

  case NFA_CE_DEREGISTERED_EVT:
    {
      tNFA_CE_DEREGISTERED& ce_deregistered = eventData->ce_deregistered;
      ALOGD("%s: NFA_CE_DEREGISTERED_EVT; h=0x%X",
            __FUNCTION__, ce_deregistered.handle);
      SyncEventGuard guard(sHostAidRouter.mDeregisterEvent);
      sHostAidRouter.mDeregisterEvent.notifyOne();
    }
    break;

  case NFA_CE_DATA_EVT:
    {
      tNFA_CE_DATA& ce_data = eventData->ce_data;
      ALOGD("%s: NFA_CE_DATA_EVT; h=0x%X; data len=%u",
            __FUNCTION__, ce_data.handle, ce_data.len);
      SecureElement::getInstance().
        notifyTransactionListenersOfAid((uint8_t *)"2PAY.SYS.DDF01", 14);
    }
    break;
  }
}
