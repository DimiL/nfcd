/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SecureElement.h"
#include "PowerSwitch.h"
#include "HostAidRouter.h"
#include "config.h"
#include "NfcUtil.h"

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

// secure element ID to use in connectEE(), -1 means not set.
int gSEId = -1;
// gate id or static pipe id to use in connectEE(), -1 means not set.
int gGatePipe = -1;
// if true, use gGatePipe as static pipe id.  if false, use as gate id.
bool gUseStaticPipe = false;

SecureElement SecureElement::sSecElem;
const char* SecureElement::APP_NAME = "nfc";

extern void startRfDiscovery(bool isStart);
extern void setUiccIdleTimeout(bool enable);

SecureElement::SecureElement()
 : mActiveEeHandle(NFA_HANDLE_INVALID)
 , mDestinationGate(4)  //loopback gate
 , mNfaHciHandle(NFA_HANDLE_INVALID)
 , mIsInit(false)
 , mActualNumEe(0)
 , mNumEePresent(0)
 , mbNewEE(true) // by default we start w/thinking there are new EE
 , mNewPipeId(0)
 , mNewSourceGate(0)
 , mActiveSeOverride(0)
 , mCommandStatus(NFA_STATUS_OK)
 , mIsPiping(false)
 , mCurrentRouteSelection(NoRoute)
 , mActualResponseSize(0)
 , mUseOberthurWarmReset(false)
 , mActivatedInListenMode(false)
 , mOberthurWarmResetCommand(3)
 , mRfFieldIsOn(false)
{
  memset(&mEeInfo, 0, sizeof(mEeInfo));
  memset(&mUiccInfo, 0, sizeof(mUiccInfo));
  memset(&mHciCfg, 0, sizeof(mHciCfg));
  memset(mResponseData, 0, sizeof(mResponseData));
  memset(mAidForEmptySelect, 0, sizeof(mAidForEmptySelect));
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
  tNFA_STATUS nfaStat;
  unsigned long num = 0;

  ALOGD("%s: enter", __FUNCTION__);

  if (GetNumValue("NFA_HCI_DEFAULT_DEST_GATE", &num, sizeof(num))) {
    mDestinationGate = num;
  }
  ALOGD("%s: Default destination gate: 0x%X", __FUNCTION__, mDestinationGate);

  // active SE, if not set active all SEs.
  if (GetNumValue("ACTIVE_SE", &num, sizeof(num))) {
    mActiveSeOverride = num;
  }
  ALOGD("%s: Active SE override: 0x%X", __FUNCTION__, mActiveSeOverride);

  if (GetNumValue("OBERTHUR_WARM_RESET_COMMAND", &num, sizeof(num))) {
    mUseOberthurWarmReset = true;
    mOberthurWarmResetCommand = (uint8_t)num;
  }

  mActiveEeHandle = NFA_HANDLE_INVALID;
  mNfaHciHandle = NFA_HANDLE_INVALID;

  mActualNumEe    = MAX_NUM_EE;
  mbNewEE         = true;
  mNewPipeId      = 0;
  mNewSourceGate  = 0;
  mRfFieldIsOn    = false;
  mActivatedInListenMode = false;
  mCurrentRouteSelection = NoRoute;
  memset(mEeInfo, 0, sizeof(mEeInfo));
  memset(&mUiccInfo, 0, sizeof(mUiccInfo));
  memset(&mHciCfg, 0, sizeof(mHciCfg));
  mUsedAids.clear();
  memset(mAidForEmptySelect, 0, sizeof(mAidForEmptySelect));

  // Get Fresh EE info.
  if (!getEeInfo()) {
    return false;
  }

  {
    SyncEventGuard guard(mEeRegisterEvent);
    ALOGD("%s: try ee register", __FUNCTION__);
    nfaStat = NFA_EeRegister(nfaEeCallback);
    if (nfaStat != NFA_STATUS_OK) {
      ALOGE("%s: fail ee register; error=0x%X", __FUNCTION__, nfaStat);
      return false;
    }
    mEeRegisterEvent.wait();
  }

  // If the controller has an HCI Network, register for that
  for (size_t i = 0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface > 0) &&
        (mEeInfo[i].ee_interface[0] == NCI_NFCEE_INTERFACE_HCI_ACCESS) ) {
      ALOGD("%s: Found HCI network, try hci register", __FUNCTION__);

      SyncEventGuard guard(mHciRegisterEvent);

      nfaStat = NFA_HciRegister(const_cast<char*>(APP_NAME), nfaHciCallback, TRUE);
      if (nfaStat != NFA_STATUS_OK) {
        ALOGE("%s: fail hci register; error=0x%X", __FUNCTION__, nfaStat);
        return false;
      }
      mHciRegisterEvent.wait();
      break;
    }
  }

  mRouteDataSet.initialize();
  mRouteDataSet.import();      //read XML file.
  HostAidRouter::getInstance().initialize();

  GetStrValue(NAME_AID_FOR_EMPTY_SELECT, (char*)&mAidForEmptySelect[0],
              sizeof(mAidForEmptySelect));

  mIsInit = true;
  ALOGD("%s: exit", __FUNCTION__);

  return true;
}

void SecureElement::finalize()
{
  ALOGD("%s: enter", __FUNCTION__);

  NFA_EeDeregister(nfaEeCallback);

  if (mNfaHciHandle != NFA_HANDLE_INVALID) {
    NFA_HciDeregister(const_cast<char*>(APP_NAME));
  }

  mNfaHciHandle = NFA_HANDLE_INVALID;
  mIsInit       = false;
  mActualNumEe  = 0;
  mNumEePresent = 0;
  mNewPipeId    = 0;
  mNewSourceGate = 0;
  mIsPiping = false;
  memset(mEeInfo, 0, sizeof(mEeInfo));
  memset(&mUiccInfo, 0, sizeof(mUiccInfo));

  ALOGD("%s: exit", __FUNCTION__);
}

bool SecureElement::getEeInfo()
{
  ALOGD("%s: enter; mbNewEE=%d, mActualNumEe=%d", __FUNCTION__, mbNewEE, mActualNumEe);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;

  // If mbNewEE is true then there is new EE info.
  if (mbNewEE) {
    mActualNumEe = MAX_NUM_EE;

    if ((nfaStat = NFA_EeGetInfo(&mActualNumEe, mEeInfo)) != NFA_STATUS_OK) {
      ALOGE("%s: fail get info; error=0x%X", __FUNCTION__, nfaStat);
      mActualNumEe = 0;
    } else {
      mbNewEE = false;

      ALOGD("%s: num EEs discovered: %u", __FUNCTION__, mActualNumEe);
      if (mActualNumEe != 0) {
        for (uint8_t i = 0; i < mActualNumEe; i++) {
          if ((mEeInfo[i].num_interface != 0) &&
              (mEeInfo[i].ee_interface[0] != NCI_NFCEE_INTERFACE_HCI_ACCESS)) {
            mNumEePresent++;
          }

          ALOGD("%s: EE[%u] Handle: 0x%04x Status: %s Num I/f: %u: (0x%02x, 0x%02x) Num TLVs: %u",
                 __FUNCTION__, i,
                 mEeInfo[i].ee_handle,
                 eeStatusToString(mEeInfo[i].ee_status),
                 mEeInfo[i].num_interface,
                 mEeInfo[i].ee_interface[0],
                 mEeInfo[i].ee_interface[1],
                 mEeInfo[i].num_tlvs);

          for (size_t j = 0; j < mEeInfo[i].num_tlvs; j++) {
            ALOGD("%s: EE[%u] TLV[%u] Tag: 0x%02x  Len: %u Values[]: 0x%02x 0x%02x 0x%02x ...",
                  __FUNCTION__, i, j,
                  mEeInfo[i].ee_tlv[j].tag,
                  mEeInfo[i].ee_tlv[j].len,
                  mEeInfo[i].ee_tlv[j].info[0],
                  mEeInfo[i].ee_tlv[j].info[1],
                  mEeInfo[i].ee_tlv[j].info[2]);
          }
        }
      }
    }
  }

  ALOGD("%s: exit; mActualNumEe=%d, mNumEePresent=%d",
        __FUNCTION__, mActualNumEe, mNumEePresent);
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
  if (TimeDiff(mLastRfFieldToggle, now) < 50) {
    // If it was less than 50ms ago that RF field
    // was turned off, still return ON.
    return true;
  } else {
    return false;
  }
}

bool SecureElement::isActivatedInListenMode()
{
  return mActivatedInListenMode;
}

void SecureElement::getListOfEeHandles(std::vector<uint32_t>& listSe)
{
  ALOGD("%s: enter", __FUNCTION__);
  if (mNumEePresent == 0) {
    return;
  }

  if (!mIsInit) {
    ALOGE("%s: not init", __FUNCTION__);
    return;
  }

  // Get Fresh EE info.
  if (!getEeInfo()) {
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

bool SecureElement::activate(uint32_t seID)
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  int numActivatedEe = 0;

  ALOGD("%s: enter; seID=0x%X", __FUNCTION__, seID);

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

  uint16_t overrideEeHandle = 0;
  if (mActiveSeOverride) {
    overrideEeHandle = NFA_HANDLE_GROUP_EE | mActiveSeOverride;
  }

  if (mRfFieldIsOn) {
    ALOGE("%s: RF field indication still on, resetting", __FUNCTION__);
    mRfFieldIsOn = false;
  }

  ALOGD("%s: override ee h=0x%X", __FUNCTION__, overrideEeHandle);
  //activate every discovered secure element
  for (int index = 0; index < mActualNumEe; index++) {
    tNFA_EE_INFO& eeItem = mEeInfo[index];
    if ((eeItem.ee_handle == EE_HANDLE_0xF3) || (eeItem.ee_handle == EE_HANDLE_0xF4) ||
        (eeItem.ee_handle == EE_HANDLE_0x01) || (eeItem.ee_handle == EE_HANDLE_0x02)) {
      if (overrideEeHandle && (overrideEeHandle != eeItem.ee_handle)) {
        continue;   // do not enable all SEs; only the override one
      }

      if (eeItem.ee_status != NFC_NFCEE_STATUS_INACTIVE) {
        ALOGD("%s: h=0x%X already activated", __FUNCTION__, eeItem.ee_handle);
        numActivatedEe++;
        continue;
      }

      {
        SyncEventGuard guard(mEeSetModeEvent);
        ALOGD("%s: set EE mode activate; h=0x%X", __FUNCTION__, eeItem.ee_handle);
        if ((nfaStat = NFA_EeModeSet(eeItem.ee_handle, NFA_EE_MD_ACTIVATE)) == NFA_STATUS_OK) {
          mEeSetModeEvent.wait(); //wait for NFA_EE_MODE_SET_EVT
          if (eeItem.ee_status == NFC_NFCEE_STATUS_ACTIVE) {
            numActivatedEe++;
          } else {
            ALOGE("%s: NFA_EeModeSet failed; error=0x%X", __FUNCTION__, nfaStat);
          }
        }
      }
    } //for
  }

  mActiveEeHandle = getDefaultEeHandle();
  if (mActiveEeHandle == NFA_HANDLE_INVALID) {
    ALOGE("%s: ee handle not found", __FUNCTION__);
  }
  ALOGD("%s: exit; active ee h=0x%X", __FUNCTION__, mActiveEeHandle);

  return mActiveEeHandle != NFA_HANDLE_INVALID;
}

bool SecureElement::deactivate(uint32_t seID)
{
  bool retval = false;

  ALOGD("%s: enter; seID=0x%X, mActiveEeHandle=0x%X",
        __FUNCTION__, seID, mActiveEeHandle);

  if (!mIsInit) {
    ALOGE ("%s: not init", __FUNCTION__);
    goto TheEnd;
  }

  //if the controller is routing to sec elems or piping,
  //then the secure element cannot be deactivated
  if ((mCurrentRouteSelection == SecElemRoute) || mIsPiping) {
    ALOGE("%s: still busy", __FUNCTION__);
    goto TheEnd;
  }

  if (mActiveEeHandle == NFA_HANDLE_INVALID) {
    ALOGE("%s: invalid EE handle", __FUNCTION__);
    goto TheEnd;
  }

  mActiveEeHandle = NFA_HANDLE_INVALID;
  retval = true;

TheEnd:
  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
  return retval;
}

void SecureElement::notifyTransactionListenersOfAid(const uint8_t* aidBuffer,
                                                    uint8_t aidBufferLen)
{
  ALOGD("%s: enter; aid len=%u", __FUNCTION__, aidBufferLen);

  if (aidBufferLen == 0) {
    return;
  }

  const uint16_t tlvMaxLen = aidBufferLen + 10;
  uint8_t* tlv = new uint8_t[tlvMaxLen];
  if (tlv == NULL) {
    ALOGE("%s: fail allocate tlv", __FUNCTION__);
    return;
  }

  memcpy(tlv, aidBuffer, aidBufferLen);
  uint16_t tlvActualLen = aidBufferLen;

  // TODO: Implement
TheEnd:
  delete []tlv;
  ALOGD("%s: exit", __FUNCTION__);
}

bool SecureElement::connectEE()
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  bool retVal = false;
  uint8_t destHost = 0;
  unsigned long num = 0;
  char pipeConfName[40];
  tNFA_HANDLE eeHandle = mActiveEeHandle;

  ALOGD("%s: enter, mActiveEeHandle: 0x%04x, SEID: 0x%x, pipe_gate_num=%d, use pipe=%d",
         __FUNCTION__, mActiveEeHandle, gSEId, gGatePipe, gUseStaticPipe);

  if (!mIsInit) {
    ALOGE("%s: not init", __FUNCTION__);
    return false;
  }

  if (gSEId != -1) {
    eeHandle = gSEId | NFA_HANDLE_GROUP_EE;
    ALOGD("%s: Using SEID: 0x%x", __FUNCTION__, eeHandle);
  }

  if (eeHandle == NFA_HANDLE_INVALID) {
    ALOGE("%s: invalid handle 0x%X", __FUNCTION__, eeHandle);
    return false;
  }

  tNFA_EE_INFO *pEE = findEeByHandle(eeHandle);

  if (pEE == NULL) {
    ALOGE("%s: Handle 0x%04x  NOT FOUND !!", __FUNCTION__, eeHandle);
    return false;
  }

  // Disable RF discovery completely while the DH is connected
  startRfDiscovery(false);

  // Disable UICC idle timeout while the DH is connected
  setUiccIdleTimeout(false);

  mNewSourceGate = 0;

  if (gGatePipe == -1) {
    // pipe/gate num was not specifed by app, get from config file
    mNewPipeId = 0;

    // Construct the PIPE name based on the EE handle
    // (e.g. NFA_HCI_STATIC_PIPE_ID_F3 for UICC0).
    snprintf(pipeConfName, sizeof(pipeConfName), "NFA_HCI_STATIC_PIPE_ID_%02X",
             eeHandle & NFA_HANDLE_MASK);

    if (GetNumValue(pipeConfName, &num, sizeof(num)) && (num != 0)) {
      mNewPipeId = num;
      ALOGD("%s: Using static pipe id: 0x%X", __FUNCTION__, mNewPipeId);
    } else {
      ALOGD("%s: Did not find value '%s' defined in the .conf",
            __FUNCTION__, pipeConfName);
    }
  } else {
    if(gUseStaticPipe) {
      mNewPipeId = gGatePipe;
    } else {
      mNewPipeId = 0;
      mDestinationGate= gGatePipe;
    }
  }

  // If the .conf file had a static pipe to use, just use it.
  if (mNewPipeId != 0) {
    uint8_t host = (mNewPipeId == STATIC_PIPE_0x70) ? 0x02 : 0x03;
    uint8_t gate = (mNewPipeId == STATIC_PIPE_0x70) ? 0xF0 : 0xF1;
    nfaStat = NFA_HciAddStaticPipe(mNfaHciHandle, host, gate, mNewPipeId);
    if (nfaStat != NFA_STATUS_OK) {
      ALOGE("%s: fail create static pipe; error=0x%X", __FUNCTION__, nfaStat);
      retVal = false;
      goto TheEnd;
    }
  } else {
    if ((pEE->num_tlvs >= 1) && (pEE->ee_tlv[0].tag == NFA_EE_TAG_HCI_HOST_ID)) {
      destHost = pEE->ee_tlv[0].info[0];
    } else {
      destHost = 2;
    }

    // Get a list of existing gates and pipes
    {
      ALOGD("%s: get gate, pipe list", __FUNCTION__);
      SyncEventGuard guard(mPipeListEvent);
      nfaStat = NFA_HciGetGateAndPipeList(mNfaHciHandle);
      if (nfaStat == NFA_STATUS_OK) {
        mPipeListEvent.wait();
        if (mHciCfg.status == NFA_STATUS_OK) {
          for (uint8_t xx = 0; xx < mHciCfg.num_pipes; xx++) {
            if ((mHciCfg.pipe[xx].dest_host == destHost) &&
                (mHciCfg.pipe[xx].dest_gate == mDestinationGate)) {
              mNewSourceGate = mHciCfg.pipe[xx].local_gate;
              mNewPipeId = mHciCfg.pipe[xx].pipe_id;

              ALOGD("%s: found configured gate: 0x%02x  pipe: 0x%02x",
                    __FUNCTION__, mNewSourceGate, mNewPipeId);
              break;
            }
          }
        }
      }
    }

    if (mNewSourceGate == 0) {
      ALOGD("%s: allocate gate", __FUNCTION__);
      //allocate a source gate and store in mNewSourceGate
      SyncEventGuard guard(mAllocateGateEvent);
      if ((nfaStat = NFA_HciAllocGate(mNfaHciHandle)) != NFA_STATUS_OK) {
        ALOGE("%s: fail allocate source gate; error=0x%X", __FUNCTION__, nfaStat);
        goto TheEnd;
      }
      mAllocateGateEvent.wait();
      if (mCommandStatus != NFA_STATUS_OK) {
        goto TheEnd;
      }
    }

    if (mNewPipeId == 0) {
      ALOGD("%s: create pipe", __FUNCTION__);
      SyncEventGuard guard(mCreatePipeEvent);
      nfaStat = NFA_HciCreatePipe(mNfaHciHandle, mNewSourceGate, destHost, mDestinationGate);
      if (nfaStat != NFA_STATUS_OK) {
        ALOGE("%s: fail create pipe; error=0x%X", __FUNCTION__, nfaStat);
        goto TheEnd;
      }
      mCreatePipeEvent.wait();
      if (mCommandStatus != NFA_STATUS_OK) {
        goto TheEnd;
      }
    }

    {
      ALOGD("%s: open pipe", __FUNCTION__);
      SyncEventGuard guard(mPipeOpenedEvent);
      nfaStat = NFA_HciOpenPipe(mNfaHciHandle, mNewPipeId);
      if (nfaStat != NFA_STATUS_OK) {
        ALOGE("%s: fail open pipe; error=0x%X", __FUNCTION__, nfaStat);
        goto TheEnd;
      }
      mPipeOpenedEvent.wait();
      if (mCommandStatus != NFA_STATUS_OK) {
        goto TheEnd;
      }
    }
  }
  retVal = true;

TheEnd:
  mIsPiping = retVal;
  if (!retVal) {
    // if open failed we need to de-allocate the gate
    disconnectEE(0);
  }

  ALOGD("%s: exit; ok=%u", __FUNCTION__, retVal);
  return retVal;
}

bool SecureElement::disconnectEE(uint32_t seID)
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  tNFA_HANDLE eeHandle = seID;

  ALOGD("%s: seID=0x%X; handle=0x%04x", __FUNCTION__, seID, eeHandle);

  if (mUseOberthurWarmReset) {
    //send warm-reset command to Oberthur secure element which deselects the applet;
    //this is an Oberthur-specific command;
    ALOGD("%s: try warm-reset on pipe id 0x%X; cmd=0x%X",
          __FUNCTION__, mNewPipeId, mOberthurWarmResetCommand);
    SyncEventGuard guard(mRegistryEvent);
    nfaStat = NFA_HciSetRegistry(mNfaHciHandle, mNewPipeId, 1, 1,
                                 &mOberthurWarmResetCommand);
    if (nfaStat == NFA_STATUS_OK) {
      mRegistryEvent.wait();
      ALOGD("%s: completed warm-reset on pipe 0x%X", __FUNCTION__, mNewPipeId);
    }
  }

  if (mNewSourceGate) {
    SyncEventGuard guard(mDeallocateGateEvent);
    if ((nfaStat = NFA_HciDeallocGate(mNfaHciHandle, mNewSourceGate)) == NFA_STATUS_OK) {
      mDeallocateGateEvent.wait();
    } else {
      ALOGE("%s: fail dealloc gate; error=0x%X", __FUNCTION__, nfaStat);
    }
  }

  mIsPiping = false;

  // Re-enable UICC low-power mode
  setUiccIdleTimeout(true);
  // Re-enable RF discovery
  // Note that it only effactuates the current configuration,
  // so if polling/listening were configured OFF (forex because
  // the screen was off), they will stay OFF with this call.
  startRfDiscovery(true);

  return true;
}

bool SecureElement::transceive(uint8_t* xmitBuffer, uint32_t xmitBufferSize,
                               uint8_t* recvBuffer, uint32_t recvBufferMaxSize,
                               uint32_t& recvBufferActualSize, uint32_t timeoutMillisec)
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  bool isSuccess = false;
  bool waitOk = false;
  uint8_t newSelectCmd[NCI_MAX_AID_LEN + 10];

  ALOGD("%s: enter; xmitBufferSize=%u; recvBufferMaxSize=%u; timeout=%u"
        , __FUNCTION__, xmitBufferSize, recvBufferMaxSize, timeoutMillisec);

  // Check if we need to replace an "empty" SELECT command.
  // 1. Has there been a AID configured, and
  // 2. Is that AID a valid length (i.e 16 bytes max), and
  // 3. Is the APDU at least 4 bytes (for header), and
  // 4. Is INS == 0xA4 (SELECT command), and
  // 5. Is P1 == 0x04 (SELECT by AID), and
  // 6. Is the APDU len 4 or 5 bytes.
  //
  // Note, the length of the configured AID is in the first
  //   byte, and AID starts from the 2nd byte.
  if (mAidForEmptySelect[0]                           // 1
      && (mAidForEmptySelect[0] <= NCI_MAX_AID_LEN)   // 2
      && (xmitBufferSize >= 4)                        // 3
      && (xmitBuffer[1] == 0xA4)                      // 4
      && (xmitBuffer[2] == 0x04)                      // 5
      && (xmitBufferSize <= 5))                       // 6
  {
    uint8_t idx = 0;

    // Copy APDU command header from the input buffer.
    memcpy(&newSelectCmd[0], &xmitBuffer[0], 4);
    idx = 4;

    // Set the Lc value to length of the new AID
    newSelectCmd[idx++] = mAidForEmptySelect[0];

    // Copy the AID
    memcpy(&newSelectCmd[idx], &mAidForEmptySelect[1], mAidForEmptySelect[0]);
    idx += mAidForEmptySelect[0];

    // If there is an Le (5th byte of APDU), add it to the end.
    if (xmitBufferSize == 5)
      newSelectCmd[idx++] = xmitBuffer[4];

    // Point to the new APDU
    xmitBuffer = &newSelectCmd[0];
    xmitBufferSize = idx;

    ALOGD("%s: Empty AID SELECT cmd detected, substituting AID from config file, new length=%d",
          __FUNCTION__, idx);
  }

  {
    SyncEventGuard guard(mTransceiveEvent);
    mActualResponseSize = 0;
    memset(mResponseData, 0, sizeof(mResponseData));
    if ((mNewPipeId == STATIC_PIPE_0x70) || (mNewPipeId == STATIC_PIPE_0x71)) {
      nfaStat = NFA_HciSendEvent(mNfaHciHandle, mNewPipeId, EVT_SEND_DATA,
                                 xmitBufferSize, xmitBuffer, sizeof(mResponseData),
                                 mResponseData, 0);
    } else {
      nfaStat = NFA_HciSendEvent(mNfaHciHandle, mNewPipeId, NFA_HCI_EVT_POST_DATA,
                                 xmitBufferSize, xmitBuffer, sizeof(mResponseData),
                                 mResponseData, 0);
    }

    if (nfaStat == NFA_STATUS_OK) {
      waitOk = mTransceiveEvent.wait(timeoutMillisec);
      if (waitOk == false) //timeout occurs
      {
        ALOGE("%s: wait response timeout", __FUNCTION__);
        goto TheEnd;
      }
    } else {
      ALOGE("%s: fail send data; error=0x%X", __FUNCTION__, nfaStat);
      goto TheEnd;
    }
  }

  if (mActualResponseSize > recvBufferMaxSize) {
    recvBufferActualSize = recvBufferMaxSize;
  } else {
    recvBufferActualSize = mActualResponseSize;
  }

  memcpy(recvBuffer, mResponseData, recvBufferActualSize);
  isSuccess = true;

TheEnd:
  ALOGD("%s: exit; isSuccess: %d; recvBufferActualSize: %d",
        __FUNCTION__, isSuccess, recvBufferActualSize);
  return isSuccess;

}

void SecureElement::notifyListenModeState(bool isActivated)
{
  ALOGD("%s: enter; listen mode active=%u", __FUNCTION__, isActivated);

  // TODO Implement
  mActivatedInListenMode = isActivated;
  if (isActivated) {
    //e->CallVoidMethod (mNativeData->manager, android::gCachedNfcManagerNotifySeListenActivated);
  } else {
    //e->CallVoidMethod (mNativeData->manager, android::gCachedNfcManagerNotifySeListenDeactivated);
  }

  ALOGD("%s: exit", __FUNCTION__);
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
  if (isActive) {
    mRfFieldIsOn = true;
    //e->CallVoidMethod (mNativeData->manager, android::gCachedNfcManagerNotifySeFieldActivated);
  } else {
    mRfFieldIsOn = false;
    //e->CallVoidMethod (mNativeData->manager, android::gCachedNfcManagerNotifySeFieldDeactivated);
  }
  mMutex.unlock();

  ALOGD("%s: exit", __FUNCTION__);
}

void SecureElement::resetRfFieldStatus()
{
  ALOGD("%s: enter;", __FUNCTION__);

  mMutex.lock();
  mRfFieldIsOn = false;
  int ret = clock_gettime(CLOCK_MONOTONIC, &mLastRfFieldToggle);
  if (ret == -1) {
    ALOGE("%s: clock_gettime failed", __FUNCTION__);
    // There is no good choice here...
  }
  mMutex.unlock();

  ALOGD("%s: exit", __FUNCTION__);
}

void SecureElement::storeUiccInfo(tNFA_EE_DISCOVER_REQ& info)
{
  ALOGD("%s:  Status: %u   Num EE: %u", __FUNCTION__, info.status, info.num_ee);

  SyncEventGuard guard(mUiccInfoEvent);
  memcpy(&mUiccInfo, &info, sizeof(mUiccInfo));
  for (uint8_t xx = 0; xx < info.num_ee; xx++) {
    //for each technology (A, B, F, B'), print the bit field that shows
    //what protocol(s) is support by that technology
    ALOGD("%s   EE[%u] Handle: 0x%04x  techA: 0x%02x  techB: 0x%02x techF: 0x%02x  techBprime: 0x%02x",
          __FUNCTION__, xx, info.ee_disc_info[xx].ee_handle,
          info.ee_disc_info[xx].la_protocol,
          info.ee_disc_info[xx].lb_protocol,
          info.ee_disc_info[xx].lf_protocol,
          info.ee_disc_info[xx].lbp_protocol);
  }
  mUiccInfoEvent.notifyOne();
}

bool SecureElement::getUiccId(tNFA_HANDLE eeHandle, std::vector<uint8_t>& uid)
{
  ALOGD("%s: ee h=0x%X", __FUNCTION__, eeHandle);
  bool retval = false;

  findUiccByHandle(eeHandle);
  //cannot get UID from the stack; nothing to do

  // TODO: uid is unused --- bug?

  // TODO: retval is always false --- bug?
  ALOGD("%s: exit; ret=%u", __FUNCTION__, retval);
  return retval;
}

bool SecureElement::getTechnologyList(tNFA_HANDLE eeHandle, std::vector<uint32_t>& techList)
{
  ALOGD("%s: ee h=0x%X", __FUNCTION__, eeHandle);
  bool retval = false;

  tNFA_EE_DISCOVER_INFO *pUICC = findUiccByHandle(eeHandle);

  // TODO: theList is written but not set --- bug?
  uint32_t theList = 0;
  if (pUICC->la_protocol != 0) {
    theList = TARGET_TYPE_ISO14443_3A;
  } else if (pUICC->lb_protocol != 0) {
    theList = TARGET_TYPE_ISO14443_3B;
  } else if (pUICC->lf_protocol != 0) {
    theList = TARGET_TYPE_FELICA;
  } else if (pUICC->lbp_protocol != 0) {
    theList = TARGET_TYPE_ISO14443_3B;
  } else {
    theList = TARGET_TYPE_UNKNOWN;
  }

  // TODO: techList is neither read nor written --- bug?

  // TODO: retval is always false --- bug?
  ALOGD("%s: exit; ret=%u", __FUNCTION__, retval);
  return retval;
}

void SecureElement::adjustRoutes(RouteSelection selection)
{
  ALOGD("%s: enter; selection=%u", __FUNCTION__, selection);
  RouteDataSet::Database* db = mRouteDataSet.getDatabase(RouteDataSet::DefaultRouteDatabase);

  if (selection == SecElemRoute) {
    db = mRouteDataSet.getDatabase(RouteDataSet::SecElemRouteDatabase);
  }

  mCurrentRouteSelection = selection;
  adjustProtocolRoutes(db, selection);
  adjustTechnologyRoutes(db, selection);
  HostAidRouter::getInstance().deleteAllRoutes(); //stop all AID routes to host

  if (db->empty()) {
    ALOGD("%s: no route configuration", __FUNCTION__);
    goto TheEnd;
  }

TheEnd:
  NFA_EeUpdateNow(); //apply new routes now.
  ALOGD("%s: exit", __FUNCTION__);
}

void SecureElement::applyRoutes()
{
  ALOGD("%s: enter", __FUNCTION__);
  if (mCurrentRouteSelection != NoRoute) {
    mRouteDataSet.import(); //read XML file
    adjustRoutes(mCurrentRouteSelection);
  }
  ALOGD("%s: exit", __FUNCTION__);
}

void SecureElement::adjustProtocolRoutes(RouteDataSet::Database* db,
                                         RouteSelection routeSelection)
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
    if ((nfaStat = NFA_EeSetDefaultProtoRouting(NFA_EE_HANDLE_DH, 0, 0, 0)) ==
                                                NFA_STATUS_OK) {
      mRoutingEvent.wait();
    } else {
      ALOGE("%s: fail delete route to host; error=0x%X", __FUNCTION__, nfaStat);
    }
  }

  /**
   * delete route to every sec elem
   */
  for (int i=0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NFC_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status == NFA_EE_STATUS_ACTIVE)) {
      ALOGD("%s: delete route to EE h=0x%X", __FUNCTION__, mEeInfo[i].ee_handle);
      SyncEventGuard guard(mRoutingEvent);
      if ((nfaStat = NFA_EeSetDefaultProtoRouting(mEeInfo[i].ee_handle, 0, 0, 0)) ==
                                                  NFA_STATUS_OK) {
        mRoutingEvent.wait();
      } else {
        ALOGE("%s: fail delete route to EE; error=0x%X", __FUNCTION__, nfaStat);
      }
    }
  }

  /**
   * configure route for every discovered sec elem
   */
  for (int i=0; i < mActualNumEe; i++) {
    //if sec elem is active
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NFC_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status == NFA_EE_STATUS_ACTIVE)) {
      //all protocols that are active at full power.
      tNFA_PROTOCOL_MASK protocolsSwitchOn = 0;
      //all protocols that are active when phone is turned off.
      tNFA_PROTOCOL_MASK protocolsSwitchOff = 0;
      //all protocols that are active when there is no power.
      tNFA_PROTOCOL_MASK protocolsBatteryOff = 0;

      //for every route in XML, look for protocol route;
      //collect every protocol according to it's desired power mode
      for (RouteDataSet::Database::iterator iter = db->begin(); iter != db->end(); iter++) {
        RouteData* routeData = *iter;
        RouteDataForProtocol* route = NULL;
        if (routeData->mRouteType != RouteData::ProtocolRoute) {
          continue; //skip other kinds of routing data
        }
        route = (RouteDataForProtocol*)(*iter);
        if (route->mNfaEeHandle == mEeInfo[i].ee_handle) {
          if (route->mSwitchOn) {
            protocolsSwitchOn |= route->mProtocol;
          }
          if (route->mSwitchOff) {
            protocolsSwitchOff |= route->mProtocol;
          }
          if (route->mBatteryOff) {
            protocolsBatteryOff |= route->mProtocol;
          }
        }
      }

      if (protocolsSwitchOn | protocolsSwitchOff | protocolsBatteryOff) {
        ALOGD("%s: route to EE h=0x%X", __FUNCTION__, mEeInfo[i].ee_handle);
        SyncEventGuard guard(mRoutingEvent);
        nfaStat = NFA_EeSetDefaultProtoRouting(mEeInfo[i].ee_handle, protocolsSwitchOn,
                                               protocolsSwitchOff, protocolsBatteryOff);
        if (nfaStat == NFA_STATUS_OK) {
          mRoutingEvent.wait();
        } else {
          ALOGE("%s: fail route to EE; error=0x%X", __FUNCTION__, nfaStat);
        }
      }
    } //if sec elem is active
  } //for every discovered sec elem

  /**
   * configure route to host.
   */
  {
    //all protocols that are active at full power.
    tNFA_PROTOCOL_MASK protocolsSwitchOn = 0;
    //all protocols that are active when phone is turned off.
    tNFA_PROTOCOL_MASK protocolsSwitchOff = 0;
    //all protocols that are active when there is no power.
    tNFA_PROTOCOL_MASK protocolsBatteryOff = 0;

    //for every route in XML, look for protocol route;
    //collect every protocol according to it's desired power mode
    for (RouteDataSet::Database::iterator iter = db->begin(); iter != db->end(); iter++) {
      RouteData* routeData = *iter;
      RouteDataForProtocol* route = NULL;
      if (routeData->mRouteType != RouteData::ProtocolRoute) {
        continue; //skip other kinds of routing data
      }
      route = (RouteDataForProtocol*)(*iter);
      if (route->mNfaEeHandle == NFA_EE_HANDLE_DH) {
        if (route->mSwitchOn) {
          protocolsSwitchOn |= route->mProtocol;
        }
        if (route->mSwitchOff) {
          protocolsSwitchOff |= route->mProtocol;
        }
        if (route->mBatteryOff) {
          protocolsBatteryOff |= route->mProtocol;
        }
      }
    }
    if (protocolsSwitchOn | protocolsSwitchOff | protocolsBatteryOff) {
      ALOGD("%s: route to EE h=0x%X", __FUNCTION__, NFA_EE_HANDLE_DH);
      SyncEventGuard guard(mRoutingEvent);
      nfaStat = NFA_EeSetDefaultProtoRouting(NFA_EE_HANDLE_DH,
              protocolsSwitchOn, protocolsSwitchOff, protocolsBatteryOff);
      if (nfaStat == NFA_STATUS_OK) {
        mRoutingEvent.wait();
      } else {
        ALOGE ("%s: fail route to EE; error=0x%X", __FUNCTION__, nfaStat);
      }
    }
  }
  /**
   * if route database is empty, setup a default route.
   */
  if (db->empty()) {
    tNFA_HANDLE eeHandle = NFA_EE_HANDLE_DH;
    if (routeSelection == SecElemRoute) {
      eeHandle = mActiveEeHandle;
    }
    ALOGD("%s: route to default EE h=0x%X", __FUNCTION__, eeHandle);
    SyncEventGuard guard(mRoutingEvent);
    nfaStat = NFA_EeSetDefaultProtoRouting(eeHandle, protoMask, 0, 0);
    if (nfaStat == NFA_STATUS_OK) {
      mRoutingEvent.wait();
    } else {
      ALOGE("%s: fail route to EE; error=0x%X", __FUNCTION__, nfaStat);
    }
  }
  ALOGD("%s: exit", __FUNCTION__);
}

void SecureElement::adjustTechnologyRoutes(RouteDataSet::Database* db,
                                           RouteSelection routeSelection)
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
    if ((nfaStat = NFA_EeSetDefaultTechRouting(NFA_EE_HANDLE_DH, 0, 0, 0)) ==
                                               NFA_STATUS_OK) {
      mRoutingEvent.wait();
    } else {
      ALOGE("%s: fail delete route to host; error=0x%X", __FUNCTION__, nfaStat);
    }
  }

  /**
   * delete route to every sec elem.
   */
  for (int i=0; i < mActualNumEe; i++) {
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NFC_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status == NFA_EE_STATUS_ACTIVE)) {
      ALOGD("%s: delete route to EE h=0x%X", __FUNCTION__, mEeInfo[i].ee_handle);
      SyncEventGuard guard(mRoutingEvent);
      if ((nfaStat = NFA_EeSetDefaultTechRouting(mEeInfo[i].ee_handle, 0, 0, 0)) ==
                                                 NFA_STATUS_OK) {
        mRoutingEvent.wait();
      } else {
        ALOGE("%s: fail delete route to EE; error=0x%X", __FUNCTION__, nfaStat);
      }
    }
  }

  /**
   * configure route for every discovered sec elem.
   */
  for (int i=0; i < mActualNumEe; i++) {
    //if sec elem is active
    if ((mEeInfo[i].num_interface != 0) &&
        (mEeInfo[i].ee_interface[0] != NFC_NFCEE_INTERFACE_HCI_ACCESS) &&
        (mEeInfo[i].ee_status == NFA_EE_STATUS_ACTIVE)) {
      //all techs that are active at full power$,
      tNFA_TECHNOLOGY_MASK techsSwitchOn = 0;
      //all techs that are active when phone is turned off.
      tNFA_TECHNOLOGY_MASK techsSwitchOff = 0;
      //all techs that are active when there is no power.
      tNFA_TECHNOLOGY_MASK techsBatteryOff = 0;

      //for every route in XML, look for tech route;
      //collect every tech according to it's desired power mode
      for (RouteDataSet::Database::iterator iter = db->begin(); iter != db->end(); iter++) {
        RouteData* routeData = *iter;
        RouteDataForTechnology* route = NULL;
        if (routeData->mRouteType != RouteData::TechnologyRoute) {
          continue; //skip other kinds of routing data
        }
        route = (RouteDataForTechnology*) (*iter);
        if (route->mNfaEeHandle == mEeInfo[i].ee_handle) {
          if (route->mSwitchOn)
            techsSwitchOn |= route->mTechnology;
          if (route->mSwitchOff)
            techsSwitchOff |= route->mTechnology;
          if (route->mBatteryOff)
            techsBatteryOff |= route->mTechnology;
        }
      }

      if (techsSwitchOn | techsSwitchOff | techsBatteryOff) {
        ALOGD("%s: route to EE h=0x%X", __FUNCTION__, mEeInfo[i].ee_handle);
        SyncEventGuard guard (mRoutingEvent);
        nfaStat = NFA_EeSetDefaultTechRouting (mEeInfo[i].ee_handle,
                techsSwitchOn, techsSwitchOff, techsBatteryOff);
        if (nfaStat == NFA_STATUS_OK)
          mRoutingEvent.wait();
        else
          ALOGE("%s: fail route to EE; error=0x%X", __FUNCTION__, nfaStat);
      }
    } //if sec elem is active
  } //for every discovered sec elem

  /**
   * configure route to host.
   */
  {
    //all techs that are active at full power.
    tNFA_TECHNOLOGY_MASK techsSwitchOn = 0;
    //all techs that are active when phone is turned off$.
    tNFA_TECHNOLOGY_MASK techsSwitchOff = 0;
    //all techs that are active when there is no power$.
    tNFA_TECHNOLOGY_MASK techsBatteryOff = 0;

    //for every route in XML, look for protocol route;
    //collect every protocol according to it's desired power mode
    for (RouteDataSet::Database::iterator iter = db->begin(); iter != db->end(); iter++) {
      RouteData* routeData = *iter;
      RouteDataForTechnology * route = NULL;
      if (routeData->mRouteType != RouteData::TechnologyRoute) {
        continue; //skip other kinds of routing data
      }
      route = (RouteDataForTechnology*) (*iter);
      if (route->mNfaEeHandle == NFA_EE_HANDLE_DH)
      {
        if (route->mSwitchOn) {
          techsSwitchOn |= route->mTechnology;
        }
        if (route->mSwitchOff) {
          techsSwitchOff |= route->mTechnology;
        }
        if (route->mBatteryOff) {
          techsBatteryOff |= route->mTechnology;
        }
      }
    }

    if (techsSwitchOn | techsSwitchOff | techsBatteryOff) {
      ALOGD("%s: route to EE h=0x%X", __FUNCTION__, NFA_EE_HANDLE_DH);
      SyncEventGuard guard(mRoutingEvent);
      nfaStat = NFA_EeSetDefaultTechRouting(NFA_EE_HANDLE_DH, techsSwitchOn,
                                            techsSwitchOff, techsBatteryOff);
      if (nfaStat == NFA_STATUS_OK) {
        mRoutingEvent.wait();
      } else {
        ALOGE("%s: fail route to EE; error=0x%X", __FUNCTION__, nfaStat);
      }
    }
  }

  /**
   * if route database is empty, setup a default route.
   */
  if (db->empty()) {
    tNFA_HANDLE eeHandle = NFA_EE_HANDLE_DH;
    if (routeSelection == SecElemRoute) {
        eeHandle = mActiveEeHandle;
    }
    ALOGD("%s: route to default EE h=0x%X", __FUNCTION__, eeHandle);
    SyncEventGuard guard(mRoutingEvent);
    nfaStat = NFA_EeSetDefaultTechRouting(eeHandle, techMask, 0, 0);
    if (nfaStat == NFA_STATUS_OK) {
      mRoutingEvent.wait();
    } else {
      ALOGE("%s: fail route to EE; error=0x%X", __FUNCTION__, nfaStat);
    }
  }

  ALOGD("%s: exit", __FUNCTION__);
}

void SecureElement::nfaEeCallback(tNFA_EE_EVT event, tNFA_EE_CBACK_DATA* eventData)
{
  ALOGD("%s: event=0x%X", __FUNCTION__, event);
  switch (event)
  {
  case NFA_EE_REGISTER_EVT:
    {
      SyncEventGuard guard(sSecElem.mEeRegisterEvent);
      ALOGD("%s: NFA_EE_REGISTER_EVT; status=%u",
            __FUNCTION__, eventData->ee_register);
      sSecElem.mEeRegisterEvent.notifyOne();
    }
    break;
  case NFA_EE_MODE_SET_EVT:
    {
      ALOGD("%s: NFA_EE_MODE_SET_EVT; status: 0x%04X  handle: 0x%04X mActiveEeHandle: 0x%04X",
            __FUNCTION__, eventData->mode_set.status,
            eventData->mode_set.ee_handle, sSecElem.mActiveEeHandle);

      if (eventData->mode_set.status == NFA_STATUS_OK) {
        tNFA_EE_INFO *pEE = sSecElem.findEeByHandle(eventData->mode_set.ee_handle);
        if (pEE) {
          pEE->ee_status ^= 1;
          ALOGD("%s: NFA_EE_MODE_SET_EVT; pEE->ee_status: %s (0x%04x)", __FUNCTION__,
                SecureElement::eeStatusToString(pEE->ee_status), pEE->ee_status);
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
        //if app-init operation is successful;
        //app_init.data[] contains two bytes, which are the status codes of the event;
        //app_init.data[] does not contain an APDU response;
        //see EMV Contactless Specification for Payment Systems; Book B; Entry Point Specification;
        //version 2.1; March 2011; section 3.3.3.5;
        if ((app_init.len_data > 1) &&
            (app_init.data[0] == 0x90) &&
            (app_init.data[1] == 0x00)) {
          sSecElem.notifyTransactionListenersOfAid(app_init.aid, app_init.len_aid);
        }
      } else if (action.trigger == NFC_EE_TRIG_RF_PROTOCOL) {
        ALOGD("%s: NFA_EE_ACTION_EVT; h=0x%X; trigger=rf protocol (0x%X)",
              __FUNCTION__, action.ee_handle, action.trigger);
      } else if (action.trigger == NFC_EE_TRIG_RF_TECHNOLOGY) {
        ALOGD("%s: NFA_EE_ACTION_EVT; h=0x%X; trigger=rf tech (0x%X)",
              __FUNCTION__, action.ee_handle, action.trigger);
      } else {
        ALOGE("%s: NFA_EE_ACTION_EVT; h=0x%X; unknown trigger (0x%X)",
              __FUNCTION__, action.ee_handle, action.trigger);
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
      ALOGD("%s: NFA_EE_NEW_EE_EVT  h=0x%X; status=%u",
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

bool SecureElement::getSeVerInfo(int seIndex, char * verInfo, int verInfoSz, uint8_t * seid)
{
  ALOGD("%s: enter, seIndex=%d", __FUNCTION__, seIndex);

  if (seIndex > (mActualNumEe-1)) {
    ALOGE("%s: invalid se index: %d, only %d SEs in system",
          __FUNCTION__, seIndex, mActualNumEe);
    return false;
  }

  *seid = mEeInfo[seIndex].ee_handle;

  if ((mEeInfo[seIndex].num_interface == 0) ||
      (mEeInfo[seIndex].ee_interface[0] == NCI_NFCEE_INTERFACE_HCI_ACCESS)) {
    return false;
  }

  strncpy(verInfo, "Version info not available", verInfoSz-1);
  verInfo[verInfoSz-1] = '\0';

  uint8_t pipe = (mEeInfo[seIndex].ee_handle == EE_HANDLE_0xF3) ? 0x70 : 0x71;
  uint8_t host = (pipe == STATIC_PIPE_0x70) ? 0x02 : 0x03;
  uint8_t gate = (pipe == STATIC_PIPE_0x70) ? 0xF0 : 0xF1;

  tNFA_STATUS nfaStat = NFA_HciAddStaticPipe(mNfaHciHandle, host, gate, pipe);
  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("%s: NFA_HciAddStaticPipe() failed, pipe = 0x%x, error=0x%X", __FUNCTION__, pipe, nfaStat);
    return true;
  }

  SyncEventGuard guard(mVerInfoEvent);
  if (NFA_STATUS_OK == (nfaStat = NFA_HciGetRegistry(mNfaHciHandle, pipe, 0x02))) {
    if (false == mVerInfoEvent.wait(200)) {
      ALOGE("%s: wait response timeout", __FUNCTION__);
    } else {
      snprintf(verInfo, verInfoSz-1, "Oberthur OS S/N: 0x%02x%02x%02x",
               mVerInfo[0], mVerInfo[1], mVerInfo[2]);
      verInfo[verInfoSz-1] = '\0';
    }
  } else {
    ALOGE("%s: NFA_HciGetRegistry() failed: 0x%X", __FUNCTION__, nfaStat);
  }
  return true;
}

uint8_t SecureElement::getActualNumEe()
{
  return mActualNumEe;
}

void SecureElement::nfaHciCallback(tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA* eventData)
{
  ALOGD("%s: event=0x%X", __FUNCTION__, event);

  switch (event)
  {
  case NFA_HCI_REGISTER_EVT:
    {
      ALOGD("%s: NFA_HCI_REGISTER_EVT; status=0x%X; handle=0x%X",
            __FUNCTION__, eventData->hci_register.status,
            eventData->hci_register.hci_handle);
      SyncEventGuard guard(sSecElem.mHciRegisterEvent);
      sSecElem.mNfaHciHandle = eventData->hci_register.hci_handle;
      sSecElem.mHciRegisterEvent.notifyOne();
    }
    break;
  case NFA_HCI_ALLOCATE_GATE_EVT:
    {
      ALOGD("%s: NFA_HCI_ALLOCATE_GATE_EVT; status=0x%X; gate=0x%X",
            __FUNCTION__, eventData->status, eventData->allocated.gate);
      SyncEventGuard guard(sSecElem.mAllocateGateEvent);
      sSecElem.mCommandStatus = eventData->status;
      sSecElem.mNewSourceGate = (eventData->allocated.status == NFA_STATUS_OK) ?
                                eventData->allocated.gate : 0;
      sSecElem.mAllocateGateEvent.notifyOne();
    }
    break;
  case NFA_HCI_DEALLOCATE_GATE_EVT:
    {
      tNFA_HCI_DEALLOCATE_GATE& deallocated = eventData->deallocated;
      ALOGD("%s: NFA_HCI_DEALLOCATE_GATE_EVT; status=0x%X; gate=0x%X",
            __FUNCTION__, deallocated.status, deallocated.gate);
      SyncEventGuard guard(sSecElem.mDeallocateGateEvent);
      sSecElem.mDeallocateGateEvent.notifyOne();
    }
    break;
  case NFA_HCI_GET_GATE_PIPE_LIST_EVT:
    {
      ALOGD("%s: NFA_HCI_GET_GATE_PIPE_LIST_EVT; status=0x%X; num_pipes: %u  num_gates: %u",
            __FUNCTION__, eventData->gates_pipes.status,
            eventData->gates_pipes.num_pipes, eventData->gates_pipes.num_gates);
      SyncEventGuard guard(sSecElem.mPipeListEvent);
      sSecElem.mCommandStatus = eventData->gates_pipes.status;
      sSecElem.mHciCfg = eventData->gates_pipes;
      sSecElem.mPipeListEvent.notifyOne();
    }
    break;
  case NFA_HCI_CREATE_PIPE_EVT:
    {
      ALOGD("%s: NFA_HCI_CREATE_PIPE_EVT; status=0x%X; pipe=0x%X; src gate=0x%X; dest host=0x%X; dest gate=0x%X",
            __FUNCTION__, eventData->created.status, eventData->created.pipe, eventData->created.source_gate,
            eventData->created.dest_host, eventData->created.dest_gate);
      SyncEventGuard guard(sSecElem.mCreatePipeEvent);
      sSecElem.mCommandStatus = eventData->created.status;
      sSecElem.mNewPipeId = eventData->created.pipe;
      sSecElem.mCreatePipeEvent.notifyOne();
    }
    break;
  case NFA_HCI_OPEN_PIPE_EVT:
    {
      ALOGD("%s: NFA_HCI_OPEN_PIPE_EVT; status=0x%X; pipe=0x%X",
             __FUNCTION__, eventData->opened.status, eventData->opened.pipe);
      SyncEventGuard guard(sSecElem.mPipeOpenedEvent);
      sSecElem.mCommandStatus = eventData->opened.status;
      sSecElem.mPipeOpenedEvent.notifyOne();
    }
    break;
  case NFA_HCI_EVENT_SENT_EVT:
    {
      ALOGD("%s: NFA_HCI_EVENT_SENT_EVT; status=0x%X",
            __FUNCTION__, eventData->evt_sent.status);
    }
    break;
  case NFA_HCI_RSP_RCVD_EVT: //response received from secure element
    {
      tNFA_HCI_RSP_RCVD& rsp_rcvd = eventData->rsp_rcvd;
      ALOGD("%s: NFA_HCI_RSP_RCVD_EVT; status: 0x%X; code: 0x%X; pipe: 0x%X; len: %u",
            __FUNCTION__, rsp_rcvd.status, rsp_rcvd.rsp_code, rsp_rcvd.pipe, rsp_rcvd.rsp_len);
    }
    break;
  case NFA_HCI_GET_REG_RSP_EVT:
    {
      ALOGD("%s: NFA_HCI_GET_REG_RSP_EVT; status: 0x%X; pipe: 0x%X, len: %d",
            __FUNCTION__, eventData->registry.status, eventData->registry.pipe,
            eventData->registry.data_len);
      if (eventData->registry.data_len >= 19 &&
          ((eventData->registry.pipe == STATIC_PIPE_0x70) ||
           (eventData->registry.pipe == STATIC_PIPE_0x71))) {
        SyncEventGuard guard(sSecElem.mVerInfoEvent);
        // Oberthur OS version is in bytes 16,17, and 18
        sSecElem.mVerInfo[0] = eventData->registry.reg_data[16];
        sSecElem.mVerInfo[1] = eventData->registry.reg_data[17];
        sSecElem.mVerInfo[2] = eventData->registry.reg_data[18];
        sSecElem.mVerInfoEvent.notifyOne();
      }
    }
    break;
  case NFA_HCI_EVENT_RCVD_EVT:
    {
      ALOGD("%s: NFA_HCI_EVENT_RCVD_EVT; code: 0x%X; pipe: 0x%X; data len: %u",
            __FUNCTION__, eventData->rcvd_evt.evt_code,
            eventData->rcvd_evt.pipe, eventData->rcvd_evt.evt_len);
      if ((eventData->rcvd_evt.pipe == STATIC_PIPE_0x70) ||
          (eventData->rcvd_evt.pipe == STATIC_PIPE_0x71)) {
        ALOGD("%s: NFA_HCI_EVENT_RCVD_EVT; data from static pipe", __FUNCTION__);
        SyncEventGuard guard(sSecElem.mTransceiveEvent);
        sSecElem.mActualResponseSize = (eventData->rcvd_evt.evt_len > MAX_RESPONSE_SIZE)
                                       ? MAX_RESPONSE_SIZE : eventData->rcvd_evt.evt_len;
        sSecElem.mTransceiveEvent.notifyOne();
      } else if (eventData->rcvd_evt.evt_code == NFA_HCI_EVT_POST_DATA) {
        ALOGD("%s: NFA_HCI_EVENT_RCVD_EVT; NFA_HCI_EVT_POST_DATA", __FUNCTION__);
        SyncEventGuard guard(sSecElem.mTransceiveEvent);
        sSecElem.mActualResponseSize = (eventData->rcvd_evt.evt_len > MAX_RESPONSE_SIZE)
                                       ? MAX_RESPONSE_SIZE : eventData->rcvd_evt.evt_len;
        sSecElem.mTransceiveEvent.notifyOne();
      } else if (eventData->rcvd_evt.evt_code == NFA_HCI_EVT_TRANSACTION) {
        ALOGD ("%s: NFA_HCI_EVENT_RCVD_EVT; NFA_HCI_EVT_TRANSACTION", __FUNCTION__);
        // If we got an AID, notify any listeners
        if ((eventData->rcvd_evt.evt_len > 3) &&
            (eventData->rcvd_evt.p_evt_buf[0] == 0x81)) {
          sSecElem.notifyTransactionListenersOfAid(&eventData->rcvd_evt.p_evt_buf[2],
                                                   eventData->rcvd_evt.p_evt_buf[1]);
        }
      }
    }
    break;
  case NFA_HCI_SET_REG_RSP_EVT: //received response to write registry command
    {
      tNFA_HCI_REGISTRY& registry = eventData->registry;
      ALOGD("%s: NFA_HCI_SET_REG_RSP_EVT; status=0x%X; pipe=0x%X",
            __FUNCTION__, registry.status, registry.pipe);
      SyncEventGuard guard(sSecElem.mRegistryEvent);
      sSecElem.mRegistryEvent.notifyOne();
    }
    break;
  default:
    ALOGE("%s: unknown event code=0x%X ????", __FUNCTION__, event);
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
  return NFA_HANDLE_INVALID;
}

tNFA_EE_DISCOVER_INFO *SecureElement::findUiccByHandle(tNFA_HANDLE eeHandle)
{
  for (uint8_t index = 0; index < mUiccInfo.num_ee; index++) {
    if (mUiccInfo.ee_disc_info[index].ee_handle == eeHandle) {
      return &mUiccInfo.ee_disc_info[index];
    }
  }
  ALOGE("SecureElement::findUiccByHandle:  ee h=0x%4x not found", eeHandle);
  return NULL;
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
  }
  return "?? Unknown ??";
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

    ALOGD("%s: start UICC listen; h=0x%X; tech mask=0x%X",
          __FUNCTION__, mActiveEeHandle, tech_mask);
    SyncEventGuard guard(mUiccListenEvent);
    nfaStat = NFA_CeConfigureUiccListenTech(mActiveEeHandle, tech_mask);
    if (nfaStat == NFA_STATUS_OK) {
      mUiccListenEvent.wait();
      retval = true;
    } else {
      ALOGE("%s: fail to start UICC listen", __FUNCTION__);
    }
  }

  ALOGD("%s: exit; ok=%u", __FUNCTION__, retval);
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
