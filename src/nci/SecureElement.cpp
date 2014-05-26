/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SecureElement.h"
#include "PowerSwitch.h"
#include "config.h"
#include "NfcUtil.h"

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

SecureElement SecureElement::sSecElem;

SecureElement::SecureElement()
 : mActiveEeHandle(NFA_HANDLE_INVALID)
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

SecureElement& SecureElement::getInstance()
{
  return sSecElem;
}

void SecureElement::setActiveSeOverride(uint8_t activeSeOverride)
{
  ALOGD("%s, seid=0x%X", __FUNCTION__, activeSeOverride);
  mActiveSeOverride = activeSeOverride;
}

bool SecureElement::initialize()
{  
  unsigned long num = 0;

  // active SE, if not set active all SEs.
  if (GetNumValue("ACTIVE_SE", &num, sizeof(num))) {
    mActiveSeOverride = num;
  }
  ALOGD("%s: Active SE override: 0x%X", __FUNCTION__, mActiveSeOverride);

  mActiveEeHandle = NFA_HANDLE_INVALID;
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
  if (!getEeInfo()) {
    return false;
  }

  {
    tNFA_STATUS nfaStat;
    SyncEventGuard guard(mEeRegisterEvent);
    ALOGD("%s: try ee register", __FUNCTION__);
    nfaStat = NFA_EeRegister(nfaEeCallback);
    if (nfaStat != NFA_STATUS_OK) {
      ALOGE("%s: fail ee register; error=0x%X", __FUNCTION__, nfaStat);
      return false;
    }
    mEeRegisterEvent.wait();
  }

  mIsInit = true;

  return true;
}

void SecureElement::finalize()
{
  ALOGD("%s: enter", __FUNCTION__);

  NFA_EeDeregister(nfaEeCallback);

  mIsInit       = false;
  mActualNumEe  = 0;
}

bool SecureElement::getEeInfo()
{
  ALOGD("%s: enter; mbNewEE=%d, mActualNumEe=%d", __FUNCTION__, mbNewEE, mActualNumEe);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;

  if (!mbNewEE) {
    return (mActualNumEe != 0);
  }

  // If mbNewEE is true then there is new EE info.
  mActualNumEe = MAX_NUM_EE;

  if ((nfaStat = NFA_EeGetInfo(&mActualNumEe, mEeInfo)) != NFA_STATUS_OK) {
    ALOGE("%s: fail get info; error=0x%X", __FUNCTION__, nfaStat);
    mActualNumEe = 0;
    return false;
  }

  mbNewEE = false;

  ALOGD("%s: num EEs discovered: %u", __FUNCTION__, mActualNumEe);
  for (uint8_t i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NCI_NFCEE_INTERFACE_HCI_ACCESS)) {
      mNumEePresent++;
    }

    ALOGD("%s: EE[%u] Handle: 0x%04x  Status: %s  Num I/f: %u: (0x%02x, 0x%02x)  Num TLVs: %u",
          __FUNCTION__, i,
          mEeInfo[i].ee_handle,
          eeStatusToString(mEeInfo[i].ee_status),
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
static uint32_t TimeDiff(timespec start, timespec end)
{
  end.tv_sec -= start.tv_sec;
  end.tv_nsec -= start.tv_nsec;

  if (end.tv_nsec < 0) {
    end.tv_nsec += 10e8;
    end.tv_sec -=1;
  }

  return (end.tv_sec * 1000) + (end.tv_nsec / 10e5);
}

bool SecureElement::isRfFieldOn()
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

bool SecureElement::isActivatedInListenMode()
{
  return mActivatedInListenMode;
}

void SecureElement::getListOfEeHandles(std::vector<uint32_t>& listSe)
{
  ALOGD("%s: enter", __FUNCTION__);
  if (mNumEePresent == 0 || !mIsInit || !getEeInfo()) {
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
    listSe.push_back(handle);
    cnt++;
  }
}

bool SecureElement::activate()
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
  if (!getEeInfo()) {
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
        mEeSetModeEvent.wait(); //wait for NFA_EE_MODE_SET_EVT
        if (eeItem.ee_status == NFC_NFCEE_STATUS_ACTIVE) {
          numActivatedEe++;
        } 
      }else {
        ALOGE("%s: NFA_EeModeSet failed; error=0x%X", __FUNCTION__, nfaStat);
      }
    } //for
  }

  mActiveEeHandle = getDefaultEeHandle();

  ALOGD("%s: exit; active ee h=0x%X", __FUNCTION__, mActiveEeHandle);

  return mActiveEeHandle != NFA_HANDLE_INVALID;
}

bool SecureElement::deactivate()
{
  bool retval = false;

  ALOGD("%s: enter; mActiveEeHandle=0x%X", __FUNCTION__, mActiveEeHandle);

  if (!mIsInit) {
    ALOGE ("%s: not init", __FUNCTION__);
    return retval;
  }

  // if the controller is routing to sec elems or piping,
  // then the secure element cannot be deactivated
  if (isBusy()) {
    ALOGE ("%s: still busy", __FUNCTION__);
    return retval;
  } else if (mActiveEeHandle == NFA_HANDLE_INVALID) {
    ALOGE("%s: invalid EE handle", __FUNCTION__);
    return retval;
  }

  mActiveEeHandle = NFA_HANDLE_INVALID;
  retval = true;

  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
  return retval;
}

void SecureElement::notifyTransactionListenersOfAid(const uint8_t* aidBuffer, uint8_t aidBufferLen)
{
  ALOGD("%s: enter; aid len=%u", __FUNCTION__, aidBufferLen);

  if (aidBufferLen == 0) {
    return;
  }

  const uint16_t TLV_LEN_OFFSET = 10;
  const uint16_t tlvMaxLen = aidBufferLen + TLV_LEN_OFFSET;
  uint8_t* tlv = new uint8_t[tlvMaxLen];
  if (tlv == NULL) {
    ALOGE("%s: fail allocate tlv", __FUNCTION__);
    return;
  }

  memcpy(tlv, aidBuffer, aidBufferLen);
  uint16_t tlvActualLen = aidBufferLen;

  // TODO: Implement notify.
TheEnd:
  delete []tlv;
}

void SecureElement::notifyListenModeState(bool isActivated)
{
  ALOGD("%s: enter; listen mode active=%u", __FUNCTION__, isActivated);

  // TODO Implement notify.
  mActivatedInListenMode = isActivated;
}

void SecureElement::notifyRfFieldEvent(bool isActive)
{
  ALOGD("%s: enter; is active=%u", __FUNCTION__, isActive);

  // TODO Implement
  mMutex.lock();
  int ret = clock_gettime(CLOCK_MONOTONIC, &mLastRfFieldToggle);
  if (ret == -1) {
    ALOGE("%s: clock_gettime failed", __FUNCTION__);
    // There is no good choice here...
  }

  mRfFieldIsOn = isActive;
  mMutex.unlock();
}

void SecureElement::resetRfFieldStatus ()
{
  ALOGD ("%s: enter;", __FUNCTION__);

  mMutex.lock();
  mRfFieldIsOn = false;
  int ret = clock_gettime(CLOCK_MONOTONIC, &mLastRfFieldToggle);
  if (ret == -1) {
    ALOGE("%s: clock_gettime failed", __FUNCTION__);
    // There is no good choice here...
  }
  mMutex.unlock();
}

void SecureElement::storeUiccInfo (tNFA_EE_DISCOVER_REQ& info)
{
  ALOGD("%s:  Status: %u   Num EE: %u", __FUNCTION__, info.status, info.num_ee);

  SyncEventGuard guard(mUiccInfoEvent);
  memcpy(&mUiccInfo, &info, sizeof(mUiccInfo));
  for (uint8_t i = 0; i < info.num_ee; i++) {
    //for each technology (A, B, F, B'), print the bit field that shows
    //what protocol(s) is support by that technology
    ALOGD("%s   EE[%u] Handle: 0x%04x  techA: 0x%02x  techB: 0x%02x  techF: 0x%02x  techBprime: 0x%02x",
          __FUNCTION__, i, info.ee_disc_info[i].ee_handle,
          info.ee_disc_info[i].la_protocol,
          info.ee_disc_info[i].lb_protocol,
          info.ee_disc_info[i].lf_protocol,
          info.ee_disc_info[i].lbp_protocol);
  }
  mUiccInfoEvent.notifyOne();
}

void SecureElement::adjustRoutes(RouteSelection selection)
{
  ALOGD("%s: enter; selection=%u", __FUNCTION__, selection);

  mCurrentRouteSelection = selection;
  adjustProtocolRoutes(selection);
  adjustTechnologyRoutes(selection);

  NFA_EeUpdateNow(); //apply new routes now.
}

void SecureElement::adjustProtocolRoutes(RouteSelection routeSelection)
{
  ALOGD("%s: enter", __FUNCTION__);

  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  const tNFA_PROTOCOL_MASK protoMask = NFA_PROTOCOL_MASK_ISO_DEP;

  /**
   * delete route to host
   */
  {
    ALOGD("%s: delete route to host", __FUNCTION__);
    SyncEventGuard guard(mRoutingEvent);
    if ((nfaStat = NFA_EeSetDefaultProtoRouting(NFA_EE_HANDLE_DH, 0, 0, 0)) == NFA_STATUS_OK) {
      mRoutingEvent.wait();
    } else {
      ALOGE("%s: fail delete route to host; error=0x%X", __FUNCTION__, nfaStat);
    }
  }

  /**
   * delete route to every sec elem
   */
  for (int i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NFC_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status == NFA_EE_STATUS_ACTIVE)) {
      ALOGD("%s: delete route to EE h=0x%X", __FUNCTION__, mEeInfo[i].ee_handle);
      SyncEventGuard guard(mRoutingEvent);
      if ((nfaStat = NFA_EeSetDefaultProtoRouting(mEeInfo[i].ee_handle, 0, 0, 0)) == NFA_STATUS_OK) {
        mRoutingEvent.wait();
      } else {
        ALOGE("%s: fail delete route to EE; error=0x%X", __FUNCTION__, nfaStat);
      }
    }
  }

  /**
   * if route database is empty, setup a default route.
   */
  if (true) {
    tNFA_HANDLE eeHandle =
      (routeSelection == SecElemRoute) ? mActiveEeHandle : NFA_EE_HANDLE_DH;

    ALOGD ("%s: route to default EE h=0x%X", __FUNCTION__, eeHandle);
    SyncEventGuard guard(mRoutingEvent);
    nfaStat = NFA_EeSetDefaultProtoRouting(eeHandle, protoMask, 0, 0);
    if (nfaStat == NFA_STATUS_OK) {
      mRoutingEvent.wait();
    } else {
      ALOGE ("%s: fail route to EE; error=0x%X", __FUNCTION__, nfaStat);
    }
  }
  ALOGD("%s: exit", __FUNCTION__);
}

void SecureElement::adjustTechnologyRoutes(RouteSelection routeSelection)
{
  ALOGD("%s: enter", __FUNCTION__);

  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  const tNFA_TECHNOLOGY_MASK techMask = NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B;

  /**
   * delete route to host.
   */
  {
    ALOGD("%s: delete route to host", __FUNCTION__);
    SyncEventGuard guard(mRoutingEvent);
    if ((nfaStat = NFA_EeSetDefaultTechRouting(NFA_EE_HANDLE_DH, 0, 0, 0)) == NFA_STATUS_OK) {
      mRoutingEvent.wait();
    } else {
      ALOGE("%s: fail delete route to host; error=0x%X", __FUNCTION__, nfaStat);
    }
  }

  /**
   * delete route to every sec elem.
   */
  for (int i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NFC_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status == NFA_EE_STATUS_ACTIVE)) {
      ALOGD("%s: delete route to EE h=0x%X", __FUNCTION__, mEeInfo[i].ee_handle);
      SyncEventGuard guard(mRoutingEvent);
      if ((nfaStat = NFA_EeSetDefaultTechRouting (mEeInfo[i].ee_handle, 0, 0, 0)) == NFA_STATUS_OK) {
        mRoutingEvent.wait();
      } else {
        ALOGE("%s: fail delete route to EE; error=0x%X", __FUNCTION__, nfaStat);
      }
    }
  }

  /**
   * if route database is empty, setup a default route.
   */
  if (true) {
    tNFA_HANDLE eeHandle =
      (routeSelection == SecElemRoute) ? mActiveEeHandle : NFA_EE_HANDLE_DH;

    ALOGD("%s: route to default EE h=0x%X", __FUNCTION__, eeHandle);
    SyncEventGuard guard(mRoutingEvent);
    nfaStat = NFA_EeSetDefaultTechRouting(eeHandle, techMask, 0, 0);
    if (nfaStat == NFA_STATUS_OK) {
      mRoutingEvent.wait();
    } else {
      ALOGE("%s: fail route to EE; error=0x%X", __FUNCTION__, nfaStat);
    }
  }
}

void SecureElement::nfaEeCallback(tNFA_EE_EVT event, tNFA_EE_CBACK_DATA* eventData)
{
  ALOGD("%s: event=0x%X", __FUNCTION__, event);
  switch (event) {
    case NFA_EE_REGISTER_EVT:
      {
        SyncEventGuard guard (sSecElem.mEeRegisterEvent);
        ALOGD("%s: NFA_EE_REGISTER_EVT; status=%u", __FUNCTION__, eventData->ee_register);
        sSecElem.mEeRegisterEvent.notifyOne();
      }
      break;
    case NFA_EE_MODE_SET_EVT:
      {
        ALOGD ("%s: NFA_EE_MODE_SET_EVT; status: 0x%04X  handle: 0x%04X  mActiveEeHandle: 0x%04X",
               __FUNCTION__, eventData->mode_set.status, eventData->mode_set.ee_handle,
               sSecElem.mActiveEeHandle);

        if (eventData->mode_set.status == NFA_STATUS_OK) {
          tNFA_EE_INFO *pEE = sSecElem.findEeByHandle (eventData->mode_set.ee_handle);
          if (pEE) {
            pEE->ee_status ^= 1;
            ALOGD("%s: NFA_EE_MODE_SET_EVT; pEE->ee_status: %s (0x%04x)",
                  __FUNCTION__, SecureElement::eeStatusToString(pEE->ee_status), pEE->ee_status);
          } else {
            ALOGE("%s: NFA_EE_MODE_SET_EVT; EE: 0x%04x not found.  mActiveEeHandle: 0x%04x",
                  __FUNCTION__, eventData->mode_set.ee_handle, sSecElem.mActiveEeHandle);
          }
        }
        SyncEventGuard guard(sSecElem.mEeSetModeEvent);
        sSecElem.mEeSetModeEvent.notifyOne();
      }
      break;
    case NFA_EE_SET_TECH_CFG_EVT:
      {
        ALOGD("%s: NFA_EE_SET_TECH_CFG_EVT; status=0x%X", __FUNCTION__, eventData->status);
        SyncEventGuard guard(sSecElem.mRoutingEvent);
        sSecElem.mRoutingEvent.notifyOne();
      }
      break;
    case NFA_EE_SET_PROTO_CFG_EVT:
      {
        ALOGD("%s: NFA_EE_SET_PROTO_CFG_EVT; status=0x%X", __FUNCTION__, eventData->status);
        SyncEventGuard guard(sSecElem.mRoutingEvent);
        sSecElem.mRoutingEvent.notifyOne();
      }
      break;
    case NFA_EE_ACTION_EVT:
      {
        tNFA_EE_ACTION& action = eventData->action;
        if (action.trigger == NFC_EE_TRIG_SELECT) {
          ALOGD("%s: NFA_EE_ACTION_EVT; h=0x%X; trigger=select (0x%X)",
                __FUNCTION__, action.ee_handle, action.trigger);
        } else if (action.trigger == NFC_EE_TRIG_APP_INIT) {
          tNFC_APP_INIT& app_init = action.param.app_init;
          ALOGD("%s: NFA_EE_ACTION_EVT; h=0x%X; trigger=app-init (0x%X); aid len=%u; data len=%u",
                __FUNCTION__, action.ee_handle, action.trigger, app_init.len_aid, app_init.len_data);
          // if app-init operation is successful;
          // app_init.data[] contains two bytes, which are the status codes of the event;
          // app_init.data[] does not contain an APDU response;
          // see EMV Contactless Specification for Payment Systems; Book B; Entry Point Specification;
          // version 2.1; March 2011; section 3.3.3.5;
          if ((app_init.len_data > 1) &&
              (app_init.data[0] == 0x90) &&
              (app_init.data[1] == 0x00)) {
            sSecElem.notifyTransactionListenersOfAid(app_init.aid, app_init.len_aid);
          }
        }
      }
      break;
    case NFA_EE_DISCOVER_REQ_EVT:
      {
        ALOGD("%s: NFA_EE_DISCOVER_REQ_EVT; status=0x%X; num ee=%u",
              __FUNCTION__, eventData->discover_req.status, eventData->discover_req.num_ee);
        sSecElem.storeUiccInfo(eventData->discover_req);
      }
      break;
    case NFA_EE_NO_CB_ERR_EVT:
      {
        ALOGD("%s: NFA_EE_NO_CB_ERR_EVT  status=%u", __FUNCTION__, eventData->status);
      }
      break;
      case NFA_EE_ADD_AID_EVT:
      {
        ALOGD("%s: NFA_EE_ADD_AID_EVT  status=%u", __FUNCTION__, eventData->status);
        SyncEventGuard guard(sSecElem.mAidAddRemoveEvent);
        sSecElem.mAidAddRemoveEvent.notifyOne();
      }
      break;
    case NFA_EE_REMOVE_AID_EVT:
      {
        ALOGD("%s: NFA_EE_REMOVE_AID_EVT  status=%u", __FUNCTION__, eventData->status);
        SyncEventGuard guard(sSecElem.mAidAddRemoveEvent);
        sSecElem.mAidAddRemoveEvent.notifyOne();
      }
      break;
    case NFA_EE_NEW_EE_EVT:
      {
        ALOGD ("%s: NFA_EE_NEW_EE_EVT  h=0x%X; status=%u",
               __FUNCTION__, eventData->new_ee.ee_handle, eventData->new_ee.ee_status);
        // Indicate there are new EE
        sSecElem.mbNewEE = true;
      }
      break;
    default:
      ALOGE("%s: unknown event=%u ????", __FUNCTION__, event);
      break;
  }
}

tNFA_EE_INFO *SecureElement::findEeByHandle(tNFA_HANDLE eeHandle)
{
  for (uint8_t i = 0; i < mActualNumEe; i++) {
    if (mEeInfo[i].ee_handle == eeHandle) {
      return &mEeInfo[i];
    }
  }
  return (NULL);
}

tNFA_HANDLE SecureElement::getDefaultEeHandle()
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

const char* SecureElement::eeStatusToString(uint8_t status)
{
  switch (status)
  {
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

void SecureElement::connectionEventHandler(uint8_t event, tNFA_CONN_EVT_DATA* /*eventData*/)
{
  switch (event)
  {
    case NFA_CE_UICC_LISTEN_CONFIGURED_EVT:
    {
      SyncEventGuard guard(mUiccListenEvent);
      mUiccListenEvent.notifyOne();
    }
    break;
  }
}

bool SecureElement::routeToSecureElement()
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  tNFA_TECHNOLOGY_MASK tech_mask = NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B;
  bool retval = false;

  if (!mIsInit) {
    ALOGE("%s: not init", __FUNCTION__);
    return false;
  }

  if (mCurrentRouteSelection == SecElemRoute) {
    ALOGE("%s: already sec elem route", __FUNCTION__);
    return true;
  }

  if (mActiveEeHandle == NFA_HANDLE_INVALID) {
    ALOGE("%s: invalid EE handle", __FUNCTION__);
    return false;
  }

  adjustRoutes(SecElemRoute);

  {
    unsigned long num = 0;
    if (GetNumValue("UICC_LISTEN_TECH_MASK", &num, sizeof(num))) {
      tech_mask = num;
    }

    ALOGD("%s: start UICC listen; h=0x%X; tech mask=0x%X", __FUNCTION__, mActiveEeHandle, tech_mask);
    SyncEventGuard guard(mUiccListenEvent);
    nfaStat = NFA_CeConfigureUiccListenTech(mActiveEeHandle, tech_mask);
    if (nfaStat == NFA_STATUS_OK) {
      mUiccListenEvent.wait();
      retval = true;
    } else {
      ALOGE("%s: fail to start UICC listen", __FUNCTION__);
    }
  }

  return retval;
}

bool SecureElement::routeToDefault()
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  bool retval = false;

  ALOGD("%s: enter", __FUNCTION__);
  if (!mIsInit) {
    ALOGE("%s: not init", __FUNCTION__);
    return false;
  }

  if (mCurrentRouteSelection == DefaultRoute) {
    ALOGD("%s: already default route", __FUNCTION__);
    return true;
  }

  if (mActiveEeHandle != NFA_HANDLE_INVALID) {
    ALOGD("%s: stop UICC listen; EE h=0x%X", __FUNCTION__, mActiveEeHandle);
    SyncEventGuard guard(mUiccListenEvent);
    nfaStat = NFA_CeConfigureUiccListenTech(mActiveEeHandle, 0);
    if (nfaStat == NFA_STATUS_OK) {
      mUiccListenEvent.wait();
      retval = true;
    } else {
      ALOGE("%s: fail to stop UICC listen", __FUNCTION__);
    }
  } else {
    retval = true;
  }

  adjustRoutes(DefaultRoute);

  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
  return retval;
}

bool SecureElement::isBusy()
{
  bool retval = (mCurrentRouteSelection == SecElemRoute) || mIsPiping;
  ALOGD("%s: %u", __FUNCTION__, retval);
  return retval;
}
