/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NfcTagManager.h"

#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "NdefMessage.h"
#include "TagTechnology.h"
#include "NfcUtil.h"
#include "NfcTag.h"
#include "config.h"
#include "Mutex.h"
#include "IntervalTimer.h"
#include "Pn544Interop.h"

extern "C"
{
  #include "nfa_api.h"
  #include "nfa_rw_api.h"
  #include "ndef_utils.h"
  #include "rw_api.h"
}

#undef LOG_TAG
#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

extern bool nfcManager_isNfcActive();
extern int gGeneralTransceiveTimeout;

// Flag for nfa callback indicating we are deactivating for RF interface switch.
bool    gIsTagDeactivating = false;
// Flag for nfa callback indicating we are selecting for RF interface switch.
bool    gIsSelectingRfInterface = false;

// Pre-defined tag type values. These must match the values in
// framework Ndef.java for Google public NFC API.
#define NDEF_UNKNOWN_TYPE          -1
#define NDEF_TYPE1_TAG             1
#define NDEF_TYPE2_TAG             2
#define NDEF_TYPE3_TAG             3
#define NDEF_TYPE4_TAG             4
#define NDEF_MIFARE_CLASSIC_TAG    101

#define STATUS_CODE_TARGET_LOST    146  // This error code comes from the service.

static uint32_t     sCheckNdefCurrentSize = 0;
static tNFA_STATUS  sCheckNdefStatus = 0;      // Whether tag already contains a NDEF message.
static bool         sCheckNdefCapable = false; // Whether tag has NDEF capability.
static tNFA_HANDLE  sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
static tNFA_INTF_TYPE   sCurrentRfInterface = NFA_INTERFACE_ISO_DEP;
static uint8_t*     sTransceiveData = NULL;
static uint32_t     sTransceiveDataLen = 0;
static bool         sWaitingForTransceive = false;
static bool         sNeedToSwitchRf = false;
static Mutex        sRfInterfaceMutex;
static uint32_t     sReadDataLen = 0;
static uint8_t*     sReadData = NULL;
static bool         sIsReadingNdefMessage = false;
static SyncEvent    sReadEvent;
static sem_t        sWriteSem;
static sem_t        sFormatSem;
static SyncEvent    sTransceiveEvent;
static SyncEvent    sReconnectEvent;
static sem_t        sCheckNdefSem;
static sem_t        sPresenceCheckSem;
static sem_t        sMakeReadonlySem;
static IntervalTimer sSwitchBackTimer; // Timer used to tell us to switch back to ISO_DEP frame interface.
static bool     	sWriteOk = false;
static bool     	sWriteWaitingForComplete = false;
static bool         sFormatOk = false;
static bool     	sConnectOk = false;
static bool     	sConnectWaitingForComplete = false;
static bool         sGotDeactivate = false;
static uint32_t     sCheckNdefMaxSize = 0;
static bool         sCheckNdefCardReadOnly = false;
static bool     	sCheckNdefWaitingForComplete = false;
static int          sCountTagAway = 0;  // Count the consecutive number of presence-check failures.
static tNFA_STATUS  sMakeReadonlyStatus = NFA_STATUS_FAILED;
static bool     	sMakeReadonlyWaitingForComplete = false;

static void ndefHandlerCallback(tNFA_NDEF_EVT event, tNFA_NDEF_EVT_DATA *eventData)
{
  ALOGD("%s: event=%u, eventData=%p", __FUNCTION__, event, eventData);

  switch (event) {
    case NFA_NDEF_REGISTER_EVT: {
      tNFA_NDEF_REGISTER& ndef_reg = eventData->ndef_reg;
      ALOGD("%s: NFA_NDEF_REGISTER_EVT; status=0x%X; h=0x%X", __FUNCTION__, ndef_reg.status, ndef_reg.ndef_type_handle);
      sNdefTypeHandlerHandle = ndef_reg.ndef_type_handle;
      break;
    }

    case NFA_NDEF_DATA_EVT: {
      ALOGD("%s: NFA_NDEF_DATA_EVT; data_len = %lu", __FUNCTION__, eventData->ndef_data.len);
      sReadDataLen = eventData->ndef_data.len;
      sReadData = (uint8_t*) malloc(sReadDataLen);
      memcpy(sReadData, eventData->ndef_data.p_data, eventData->ndef_data.len);
      break;
    }

    default:
      ALOGE("%s: Unknown event %u ????", __FUNCTION__, event);
      break;
  }
}

NfcTagManager::NfcTagManager()
{
  pthread_mutex_init(&mMutex, NULL);
}

NfcTagManager::~NfcTagManager()
{
}

NdefDetail* NfcTagManager::readNdefDetail()
{
  int ndefinfo[2];
  int status;
  NdefDetail* pNdefDetail = NULL;
  status = checkNdefWithStatus(ndefinfo);
  if (status != 0) {
    ALOGE("%s: Check NDEF Failed - status = %d", __FUNCTION__, status);
  } else {
    int ndefType = getNdefType(getConnectedLibNfcType());

    pNdefDetail = new NdefDetail();
    pNdefDetail->maxSupportedLength = ndefinfo[0];
    pNdefDetail->isReadOnly = (ndefinfo[1] == NDEF_MODE_READ_ONLY);
    pNdefDetail->canBeMadeReadOnly = (ndefType == NDEF_TYPE1_TAG || ndefType == NDEF_TYPE2_TAG);
  }

  return pNdefDetail;
}

NdefMessage* NfcTagManager::findAndReadNdef()
{
  NdefMessage* ndefMsg = NULL;
  bool foundFormattable = false;
  int formattableHandle = 0;
  int formattableLibNfcType = 0;
  int status;

  for(uint32_t techIndex = 0; techIndex < mTechList.size(); techIndex++) {
    // Have we seen this handle before?
    for (uint32_t i = 0; i < techIndex; i++) {
      if (mTechHandles[i] == mTechHandles[techIndex]) {
        continue;  // Don't check duplicate handles.
      }
    }

    status = connectWithStatus(mTechList[techIndex]);
    if (status != 0) {
      ALOGE("%s: Connect Failed - status = %d", __FUNCTION__, status);
      if (status == STATUS_CODE_TARGET_LOST) {
        break;
      }
      continue;  // Try next handle.
    } else {
      ALOGI("Connect Succeeded! (status = %d)", status);
    }

    // Check if this type is NDEF formatable
    if (!foundFormattable) {
      if (isNdefFormatable()) {
        foundFormattable = true;
        formattableHandle = getConnectedHandle();
        formattableLibNfcType = getConnectedLibNfcType();
        // We'll only add formattable tech if no ndef is
        // found - this is because libNFC refuses to format
        // an already NDEF formatted tag.
      }
      // TODO : check why Android call reconnect here
      //reconnect();
    }

    int ndefinfo[2];
    status = checkNdefWithStatus(ndefinfo);
    if (status != 0) {
      ALOGE("%s: Check NDEF Failed - status = %d", __FUNCTION__, status);
      if (status == STATUS_CODE_TARGET_LOST) {
        break;
      }
      continue;  // Try next handle.
    } else {
      ALOGI("Check Succeeded! (status = %d)", status);
    }

    // Found our NDEF handle.
    bool generateEmptyNdef = false;
    int supportedNdefLength = ndefinfo[0];
    int cardState = ndefinfo[1];
    std::vector<uint8_t> buf;
    readNdef(buf);
    if (buf.size() != 0) {
      ndefMsg = new NdefMessage();
      if (ndefMsg->init(buf)) {
        addTechnology(NDEF, getConnectedHandle(), getConnectedLibNfcType());
        // TODO : check why android call reconnect here
        //reconnect();
      } else {
        generateEmptyNdef = true;
      }
    } else {
        generateEmptyNdef = true;
    }

    if (cardState == NDEF_MODE_READ_WRITE) {
      addTechnology(NDEF_WRITABLE, getConnectedHandle(), getConnectedLibNfcType());
    }

    if (generateEmptyNdef == true) {
      ALOGI("Couldn't read NDEF!");
      // TODO : Implement generate empty Ndef
      addTechnology(NDEF, getConnectedHandle(), getConnectedLibNfcType());
      //reconnect();
    }
    break;
  }
  
  if (!ndefMsg && foundFormattable) {
    // Tag is not NDEF yet, and found a formattable target,
    // so add formattable tech to tech list.
    addTechnology(NDEF_FORMATABLE, formattableHandle, formattableLibNfcType);
  } 

  return ndefMsg;
}

bool NfcTagManager::disconnect()
{
  bool result = false;

  mIsPresent = false;

  pthread_mutex_lock(&mMutex);
  result = doDisconnect();
  pthread_mutex_unlock(&mMutex);

  mConnectedTechIndex = -1;
  mConnectedHandle = -1;

  mTechList.clear();
  mTechHandles.clear();
  mTechLibNfcTypes.clear();
  mTechPollBytes.clear();
  mTechActBytes.clear();
  mUid.clear();

  return result;
}

bool NfcTagManager::reconnect()
{
  return reconnectWithStatus() == 0;
}

int NfcTagManager::reconnectWithStatus()
{
  ALOGD("%s: enter", __FUNCTION__);
  int retCode = NFCSTATUS_SUCCESS;
  NfcTag& natTag = NfcTag::getInstance();

  if (natTag.getActivationState() != NfcTag::Active) {
    ALOGD("%s: tag already deactivated", __FUNCTION__);
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  // Special case for Kovio.
  if (NfcTag::getInstance().mTechList [0] == TARGET_TYPE_KOVIO_BARCODE) {
    ALOGD("%s: fake out reconnect for Kovio", __FUNCTION__);
    goto TheEnd;
  }

  // This is only supported for type 2 or 4 (ISO_DEP) tags.
  if (natTag.mTechLibNfcTypes[0] == NFA_PROTOCOL_ISO_DEP)
    retCode = reSelect(NFA_INTERFACE_ISO_DEP);
  else if (natTag.mTechLibNfcTypes[0] == NFA_PROTOCOL_T2T)
    retCode = reSelect(NFA_INTERFACE_FRAME);

TheEnd:
  ALOGD("%s: exit 0x%X", __FUNCTION__, retCode);
  return retCode;
}

int NfcTagManager::reconnectWithStatus(int technology)
{
  int status = -1;
  pthread_mutex_lock(&mMutex);
  status = doConnect(technology);
  pthread_mutex_unlock(&mMutex);
  return status;
}

int NfcTagManager::connectWithStatus(int technology)
{
  int status = -1;
  for (uint32_t i = 0; i < mTechList.size(); i++) {
    if (mTechList[i] == technology) {
      // Get the handle and connect, if not already connected.
      if (mConnectedHandle != mTechHandles[i]) {
        // We're not yet connected, there are a few scenario's
        // here:
        // 1) We are not connected to anything yet - allow
        // 2) We are connected to a technology which has
        //    a different handle (multi-protocol tag); we support
        //    switching to that.
        // 3) We are connected to a technology which has the same
        //    handle; we do not support connecting at a different
        //    level (libnfc auto-activates to the max level on
        //    any handle).
        // 4) We are connecting to the ndef technology - always
        //    allowed.
        if (mConnectedHandle == -1) {
          // Not connected yet.
          status = doConnect(i);
        } else {
          // Connect to a tech with a different handle.
          ALOGD("%s: Connect to a tech with a different handle", __FUNCTION__);
          status = reconnectWithStatus(i);
        }
        if (status == 0) {
          mConnectedHandle = mTechHandles[i];
          mConnectedTechIndex = i;
        }
      } else {
        // 1) We are connected to a technology which has the same
        //    handle; we do not support connecting at a different
        //    level (libnfc auto-activates to the max level on
        //    any handle).
        // 2) We are connecting to the ndef technology - always
        //    allowed.
        if ((technology == NDEF) ||
            (technology == NDEF_FORMATABLE)) {
          i = 0;
        }

        status = reconnectWithStatus(i);
        if (status == 0) {
          mConnectedTechIndex = i;
        }
      }
      break;
    }
  }
  return status;
}

void NfcTagManager::doRead(std::vector<uint8_t>& buf)
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS status = NFA_STATUS_FAILED;
    
  sReadDataLen = 0;
  if (sReadData != NULL) {
    free(sReadData);
    sReadData = NULL;
  }

  if (sCheckNdefCurrentSize > 0) {
    {
      SyncEventGuard g(sReadEvent);
      sIsReadingNdefMessage = true;
      status = NFA_RwReadNDef();
      sReadEvent.wait(); // Wait for NFA_READ_CPLT_EVT.
    }
    sIsReadingNdefMessage = false;
   
    if (sReadDataLen > 0) { // If stack actually read data from the tag.
      ALOGD("%s: read %u bytes", __FUNCTION__, sReadDataLen);
      for(uint32_t idx = 0; idx < sReadDataLen; idx++) {
        buf.push_back(sReadData[idx]);
      }
    }
  } else {
    ALOGD("%s: create emtpy buffer", __FUNCTION__);
    sReadDataLen = 0;
    sReadData = (uint8_t*) malloc(1);
    buf.push_back(sReadData[0]);
  }

  if (sReadData) {
    free(sReadData);
    sReadData = NULL;
  }
  sReadDataLen = 0;

  ALOGD("%s: exit", __FUNCTION__);
  return;
}

void NfcTagManager::doWriteStatus(bool isWriteOk)
{
  if (sWriteWaitingForComplete != false) {
    sWriteWaitingForComplete = false;
    sWriteOk = isWriteOk;
    sem_post(&sWriteSem);
  }
}

int NfcTagManager::doCheckNdef(int ndefInfo[])
{
  tNFA_STATUS status = NFA_STATUS_FAILED;

  ALOGD("%s: enter", __FUNCTION__);

  // Special case for Kovio.
  if (NfcTag::getInstance().mTechList [0] == TARGET_TYPE_KOVIO_BARCODE) {
    ALOGD("%s: Kovio tag, no NDEF", __FUNCTION__);
    ndefInfo[0] = 0;
    ndefInfo[1] = NDEF_MODE_READ_ONLY;
    return NFA_STATUS_FAILED;
  }

  // Special case for Kovio.
  if (NfcTag::getInstance().mTechList [0] == TARGET_TYPE_KOVIO_BARCODE) {
    ALOGD("%s: Kovio tag, no NDEF", __FUNCTION__);
    ndefInfo[0] = 0;
    ndefInfo[1] = NDEF_MODE_READ_ONLY;
    return NFA_STATUS_FAILED;
  }

  // Create the write semaphore.
  if (sem_init(&sCheckNdefSem, 0, 0) == -1) {
    ALOGE("%s: Check NDEF semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
    return false;
  }

  if (NfcTag::getInstance().getActivationState() != NfcTag::Active) {
    ALOGE("%s: tag already deactivated", __FUNCTION__);
    goto TheEnd;
  }

  ALOGD("%s: try NFA_RwDetectNDef", __FUNCTION__);
  sCheckNdefWaitingForComplete = true;
  status = NFA_RwDetectNDef();

  if (status != NFA_STATUS_OK) {
    ALOGE("%s: NFA_RwDetectNDef failed, status = 0x%X", __FUNCTION__, status);
    goto TheEnd;
  }

  // Wait for check NDEF completion status.
  if (sem_wait(&sCheckNdefSem)) {
    ALOGE("%s: Failed to wait for check NDEF semaphore (errno=0x%08x)", __FUNCTION__, errno);
    goto TheEnd;
  }

  if (sCheckNdefStatus == NFA_STATUS_OK) {
    // Stack found a NDEF message on the tag.
    if (NfcTag::getInstance().getProtocol() == NFA_PROTOCOL_T1T)
      ndefInfo[0] = NfcTag::getInstance().getT1tMaxMessageSize();
    else
      ndefInfo[0] = sCheckNdefMaxSize;
    if (sCheckNdefCardReadOnly)
      ndefInfo[1] = NDEF_MODE_READ_ONLY;
    else
      ndefInfo[1] = NDEF_MODE_READ_WRITE;
    status = NFA_STATUS_OK;
  } else if (sCheckNdefStatus == NFA_STATUS_FAILED) {
    // Stack did not find a NDEF message on the tag.
    if (NfcTag::getInstance().getProtocol() == NFA_PROTOCOL_T1T)
      ndefInfo[0] = NfcTag::getInstance().getT1tMaxMessageSize();
    else
      ndefInfo[0] = sCheckNdefMaxSize;
    if (sCheckNdefCardReadOnly)
      ndefInfo[1] = NDEF_MODE_READ_ONLY;
    else
      ndefInfo[1] = NDEF_MODE_READ_WRITE;
    status = NFA_STATUS_FAILED;
  } else if (sCheckNdefStatus == NFA_STATUS_TIMEOUT) {
    pn544InteropStopPolling();
    status = sCheckNdefStatus;
  } else {
    ALOGD("%s: unknown status 0x%X", __FUNCTION__, sCheckNdefStatus);
    status = sCheckNdefStatus;
  }

TheEnd:
  // Destroy semaphore.
  if (sem_destroy(&sCheckNdefSem)) {
    ALOGE("%s: Failed to destroy check NDEF semaphore (errno=0x%08x)", __FUNCTION__, errno);
  }
  sCheckNdefWaitingForComplete = false;
  ALOGD("%s: exit; status=0x%X", __FUNCTION__, status);
  return status;
}

void NfcTagManager::doAbortWaits()
{
  ALOGD("%s", __FUNCTION__);
  {
    SyncEventGuard g(sReadEvent);
    sReadEvent.notifyOne();
  }
  sem_post(&sWriteSem);
  sem_post(&sFormatSem);
  {
    SyncEventGuard g(sTransceiveEvent);
    sTransceiveEvent.notifyOne();
  }
  {
    SyncEventGuard g (sReconnectEvent);
    sReconnectEvent.notifyOne();
  }

  sem_post(&sCheckNdefSem);
  sem_post(&sPresenceCheckSem);
  sem_post(&sMakeReadonlySem);
}

void NfcTagManager::doReadCompleted(tNFA_STATUS status)
{
  ALOGD("%s: status=0x%X; is reading=%u", __FUNCTION__, status, sIsReadingNdefMessage);

  if (sIsReadingNdefMessage == false)
    return; // Not reading NDEF message right now, so just return.

  if (status != NFA_STATUS_OK) {
    sReadDataLen = 0;
    if (sReadData)
      free (sReadData);
    sReadData = NULL;
  }
  SyncEventGuard g(sReadEvent);
  sReadEvent.notifyOne();
}

void NfcTagManager::doConnectStatus(bool isConnectOk)
{
  if (sConnectWaitingForComplete != false) {
    sConnectWaitingForComplete = false;
    sConnectOk = isConnectOk;
    SyncEventGuard g(sReconnectEvent);
    sReconnectEvent.notifyOne();
  }
}

void NfcTagManager::doDeactivateStatus(int status)
{
  sGotDeactivate = (status == 0);

  SyncEventGuard g(sReconnectEvent);
  sReconnectEvent.notifyOne();
}

void NfcTagManager::doResetPresenceCheck()
{
  sCountTagAway = 0;
}

void NfcTagManager::doPresenceCheckResult(tNFA_STATUS status)
{
  if (status == NFA_STATUS_OK)
    sCountTagAway = 0;
  else
    sCountTagAway++;
  if (sCountTagAway > 0)
    ALOGD("%s: sCountTagAway=%d", __FUNCTION__, sCountTagAway);
  sem_post(&sPresenceCheckSem);
}

bool NfcTagManager::doNdefFormat()
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS status = NFA_STATUS_OK;

  sem_init(&sFormatSem, 0, 0);
  sFormatOk = false;
  status = NFA_RwFormatTag();
  if (status == NFA_STATUS_OK) {
    ALOGD("%s: wait for completion", __FUNCTION__);
    sem_wait(&sFormatSem);
    status = sFormatOk ? NFA_STATUS_OK : NFA_STATUS_FAILED;
  } else {
    ALOGE("%s: error status=%u", __FUNCTION__, status);
  }
  sem_destroy(&sFormatSem);

  ALOGD("%s: exit", __FUNCTION__);
  return status == NFA_STATUS_OK;
}

void NfcTagManager::doCheckNdefResult(tNFA_STATUS status, uint32_t maxSize, uint32_t currentSize, uint8_t flags)
{
  // This function's flags parameter is defined using the following macros
  // in nfc/include/rw_api.h;
  // #define RW_NDEF_FL_READ_ONLY  0x01    /* Tag is read only              */
  // #define RW_NDEF_FL_FORMATED   0x02    /* Tag formated for NDEF         */
  // #define RW_NDEF_FL_SUPPORTED  0x04    /* NDEF supported by the tag     */
  // #define RW_NDEF_FL_UNKNOWN    0x08    /* Unable to find if tag is ndef capable/formated/read only */
  // #define RW_NDEF_FL_FORMATABLE 0x10    /* Tag supports format operation */

  if (status == NFC_STATUS_BUSY) {
    ALOGE("%s: stack is busy", __FUNCTION__);
    return;
  }

  if (!sCheckNdefWaitingForComplete) {
    ALOGE("%s: not waiting", __FUNCTION__);
    return;
  }

  if (flags & RW_NDEF_FL_READ_ONLY)
    ALOGD("%s: flag read-only", __FUNCTION__);
  if (flags & RW_NDEF_FL_FORMATED)
    ALOGD("%s: flag formatted for ndef", __FUNCTION__);
  if (flags & RW_NDEF_FL_SUPPORTED)
    ALOGD("%s: flag ndef supported", __FUNCTION__);
  if (flags & RW_NDEF_FL_UNKNOWN)
    ALOGD("%s: flag all unknown", __FUNCTION__);
  if (flags & RW_NDEF_FL_FORMATABLE)
    ALOGD("%s: flag formattable", __FUNCTION__);

  sCheckNdefWaitingForComplete = false;
  sCheckNdefStatus = status;
  sCheckNdefCapable = false; // Assume tag is NOT ndef capable.
  if (sCheckNdefStatus == NFA_STATUS_OK) {
    // NDEF content is on the tag.
    sCheckNdefMaxSize = maxSize;
    sCheckNdefCurrentSize = currentSize;
    sCheckNdefCardReadOnly = flags & RW_NDEF_FL_READ_ONLY;
    sCheckNdefCapable = true;
  } else if (sCheckNdefStatus == NFA_STATUS_FAILED) {
    // No NDEF content on the tag.
    sCheckNdefMaxSize = 0;
    sCheckNdefCurrentSize = 0;
    sCheckNdefCardReadOnly = flags & RW_NDEF_FL_READ_ONLY;
    if ((flags & RW_NDEF_FL_UNKNOWN) == 0) { // If stack understands the tag.
      if (flags & RW_NDEF_FL_SUPPORTED) {    // If tag is ndef capable.
        sCheckNdefCapable = true;
      }
    }
  } else {
    ALOGE("%s: unknown status=0x%X", __FUNCTION__, status);
    sCheckNdefMaxSize = 0;
    sCheckNdefCurrentSize = 0;
    sCheckNdefCardReadOnly = false;
  }
  sem_post(&sCheckNdefSem);
}

void NfcTagManager::doMakeReadonlyResult(tNFA_STATUS status)
{
  if (sMakeReadonlyWaitingForComplete != false) {
    sMakeReadonlyWaitingForComplete = false;
    sMakeReadonlyStatus = status;

    sem_post(&sMakeReadonlySem);
  }
}

bool NfcTagManager::doMakeReadonly()
{
  bool result = false;
  tNFA_STATUS status;

  ALOGD("%s", __FUNCTION__);

  // Create the make_readonly semaphore.
  if (sem_init(&sMakeReadonlySem, 0, 0) == -1) {
    ALOGE("%s: Make readonly semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
    return false;
  }

  sMakeReadonlyWaitingForComplete = true;

  // Hard-lock the tag (cannot be reverted).
  status = NFA_RwSetTagReadOnly(true);

  if (status != NFA_STATUS_OK) {
    ALOGE("%s: NFA_RwSetTagReadOnly failed, status = %d", __FUNCTION__, status);
    goto TheEnd;
  }

  // Wait for check NDEF completion status.
  if (sem_wait(&sMakeReadonlySem)) {
    ALOGE("%s: Failed to wait for make_readonly semaphore (errno=0x%08x)", __FUNCTION__, errno);
    goto TheEnd;
  }

  if (sMakeReadonlyStatus == NFA_STATUS_OK) {
    result = true;
  }

TheEnd:
  // Destroy semaphore.
  if (sem_destroy(&sMakeReadonlySem)) {
    ALOGE("%s: Failed to destroy read_only semaphore (errno=0x%08x)", __FUNCTION__, errno);
  }
  sMakeReadonlyWaitingForComplete = false;
  return result;
}

// Register a callback to receive NDEF message from the tag
// from the NFA_NDEF_DATA_EVT.
void NfcTagManager::doRegisterNdefTypeHandler()
{
  ALOGD("%s", __FUNCTION__);
  sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
  NFA_RegisterNDefTypeHandler(TRUE, NFA_TNF_DEFAULT, (UINT8 *) "", 0, ndefHandlerCallback);
}

void NfcTagManager::doDeregisterNdefTypeHandler()
{
  ALOGD("%s", __FUNCTION__);
  NFA_DeregisterNDefTypeHandler(sNdefTypeHandlerHandle);
  sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
}

int NfcTagManager::doConnect(int targetHandle)
{
  ALOGD("%s: targetHandle = %d", __FUNCTION__, targetHandle);
  int i = targetHandle;
  NfcTag& natTag = NfcTag::getInstance();
  int retCode = NFCSTATUS_SUCCESS;

  sNeedToSwitchRf = false;
  if (i >= NfcTag::MAX_NUM_TECHNOLOGY) {
    ALOGE("%s: Handle not found", __FUNCTION__);
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  if (natTag.getActivationState() != NfcTag::Active) {
    ALOGE("%s: tag already deactivated", __FUNCTION__);
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  if (natTag.mTechLibNfcTypes[i] != NFC_PROTOCOL_ISO_DEP) {
    ALOGD("%s() Nfc type = %d, do nothing for non ISO_DEP", __FUNCTION__, natTag.mTechLibNfcTypes[i]);
    retCode = NFCSTATUS_SUCCESS;
    goto TheEnd;
  }

  if (natTag.mTechList[i] == TARGET_TYPE_ISO14443_3A || natTag.mTechList[i] == TARGET_TYPE_ISO14443_3B) {
    ALOGD("%s: switching to tech: %d need to switch rf intf to frame", __FUNCTION__, natTag.mTechList[i]);
    // Connecting to NfcA or NfcB don't actually switch until/unless we get a transceive.
    sNeedToSwitchRf = true;
  } else {
    // Connecting back to IsoDep or NDEF.
    return (switchRfInterface(NFA_INTERFACE_ISO_DEP) ? NFCSTATUS_SUCCESS : NFCSTATUS_FAILED);
  }

TheEnd:
  ALOGD("%s: exit 0x%X", __FUNCTION__, retCode);
  return retCode;
}

bool NfcTagManager::doPresenceCheck()
{
  ALOGD("%s", __FUNCTION__);
  tNFA_STATUS status = NFA_STATUS_OK;
  bool isPresent = false;

  // Special case for Kovio. The deactivation would have already occurred
  // but was ignored so that normal tag opertions could complete.  Now we
  // want to process as if the deactivate just happened.
  if (NfcTag::getInstance().mTechList [0] == TARGET_TYPE_KOVIO_BARCODE) {
    ALOGD("%s: Kovio, force deactivate handling", __FUNCTION__);
    tNFA_DEACTIVATED deactivated = {NFA_DEACTIVATE_TYPE_IDLE};

    NfcTag::getInstance().setDeactivationState(deactivated);
    doResetPresenceCheck();
    NfcTag::getInstance().connectionEventHandler(NFA_DEACTIVATED_EVT, NULL);
    doAbortWaits();
    NfcTag::getInstance().abort();

    return false;
  }

  if (nfcManager_isNfcActive() == false) {
    ALOGD("%s: NFC is no longer active.", __FUNCTION__);
    return false;
  }

  if (NfcTag::getInstance().getActivationState() != NfcTag::Active) {
    ALOGD("%s: tag already deactivated", __FUNCTION__);
    return false;
  }

  if (sem_init(&sPresenceCheckSem, 0, 0) == -1) {
    ALOGE("%s: semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
    return false;
  }

  status = NFA_RwPresenceCheck();
  if (status == NFA_STATUS_OK) {
    if (sem_wait(&sPresenceCheckSem)) {
      ALOGE("%s: failed to wait (errno=0x%08x)", __FUNCTION__, errno);
    } else {
      isPresent = (sCountTagAway > 3) ? false : true;
    }
  }

  if (sem_destroy(&sPresenceCheckSem)) {
    ALOGE("%s: Failed to destroy check NDEF semaphore (errno=0x%08x)", __FUNCTION__, errno);
  }

  if (isPresent == false)
    ALOGD("%s: tag absent ????", __FUNCTION__);

  return isPresent;
}

int NfcTagManager::reSelect(tNFA_INTF_TYPE rfInterface)
{
  ALOGD("%s: enter; rf intf = %d", __FUNCTION__, rfInterface);
  NfcTag& natTag = NfcTag::getInstance();

  tNFA_STATUS status;
  int rVal = 1;

  do
  {
    // If tag has shutdown, abort this method.
    if (NfcTag::getInstance().isNdefDetectionTimedOut()) {
      ALOGD("%s: ndef detection timeout; break", __FUNCTION__);
      rVal = STATUS_CODE_TARGET_LOST;
      break;
    }

    {
      SyncEventGuard g(sReconnectEvent);
      gIsTagDeactivating = true;
      sGotDeactivate = false;
      ALOGD("%s: deactivate to sleep", __FUNCTION__);
      if (NFA_STATUS_OK != (status = NFA_Deactivate(TRUE))) { // Deactivate to sleep state.
        ALOGE("%s: deactivate failed, status = %d", __FUNCTION__, status);
        break;
      }

      if (sReconnectEvent.wait(1000) == false) { // If timeout occurred.
        ALOGE("%s: timeout waiting for deactivate", __FUNCTION__);
      }
    }

    if (NfcTag::getInstance().getActivationState() != NfcTag::Sleep) {
      ALOGD("%s: tag is not in sleep", __FUNCTION__);
      rVal = STATUS_CODE_TARGET_LOST;
      break;
    }

    gIsTagDeactivating = false;

    {
      SyncEventGuard g2 (sReconnectEvent);

      sConnectWaitingForComplete = true;
      ALOGD("%s: select interface %u", __FUNCTION__, rfInterface);
      gIsSelectingRfInterface = true;
      if (NFA_STATUS_OK != (status = NFA_Select(natTag.mTechHandles[0], natTag.mTechLibNfcTypes[0], rfInterface))) {
        ALOGE("%s: NFA_Select failed, status = %d", __FUNCTION__, status);
        break;
      }

      sConnectOk = false;
      if (sReconnectEvent.wait(1000) == false) { // If timeout occured.
          ALOGE("%s: timeout waiting for select", __FUNCTION__);
          break;
      }
    }

    ALOGD("%s: select completed; sConnectOk=%d", __FUNCTION__, sConnectOk);
    if (NfcTag::getInstance().getActivationState() != NfcTag::Active) {
      ALOGD("%s: tag is not active", __FUNCTION__);
      rVal = STATUS_CODE_TARGET_LOST;
      break;
    }
    rVal = (sConnectOk) ? 0 : 1;
  } while (0);

  sConnectWaitingForComplete = false;
  gIsTagDeactivating = false;
  gIsSelectingRfInterface = false;
  ALOGD("%s: exit; status=%d", __FUNCTION__, rVal);
  return rVal;
}

bool NfcTagManager::switchRfInterface(tNFA_INTF_TYPE rfInterface)
{
  ALOGD("%s: rf intf = %d", __FUNCTION__, rfInterface);
  NfcTag& natTag = NfcTag::getInstance();

  if (natTag.mTechLibNfcTypes[0] != NFC_PROTOCOL_ISO_DEP) {
    ALOGD("%s: protocol: %d not ISO_DEP, do nothing", __FUNCTION__, natTag.mTechLibNfcTypes[0]);
    return true;
  }

  sRfInterfaceMutex.lock();
  ALOGD("%s: new rf intf = %d, cur rf intf = %d", __FUNCTION__, rfInterface, sCurrentRfInterface);

  bool rVal = true;
  if (rfInterface != sCurrentRfInterface) {
    if ((rVal = (0 == reSelect(rfInterface)))) {
      sCurrentRfInterface = rfInterface;
    }
  }

  sRfInterfaceMutex.unlock();
  return rVal;
}

int NfcTagManager::getNdefType(int libnfcType)
{
  int ndefType = NDEF_UNKNOWN_TYPE;

  // For NFA, libnfcType is mapped to the protocol value received
  // in the NFA_ACTIVATED_EVT and NFA_DISC_RESULT_EVT event.
  switch (libnfcType) {
    case NFA_PROTOCOL_T1T:
      ndefType = NDEF_TYPE1_TAG;
      break;
    case NFA_PROTOCOL_T2T:
      ndefType = NDEF_TYPE2_TAG;
      break;
    case NFA_PROTOCOL_T3T:
      ndefType = NDEF_TYPE3_TAG;
      break;
    case NFA_PROTOCOL_ISO_DEP:
      ndefType = NDEF_TYPE4_TAG;
      break;
    case NFA_PROTOCOL_ISO15693:
      ndefType = NDEF_UNKNOWN_TYPE;
      break;
    case NFA_PROTOCOL_INVALID:
      ndefType = NDEF_UNKNOWN_TYPE;
      break;
    default:
      ndefType = NDEF_UNKNOWN_TYPE;
      break;
  }

  return ndefType;
}

void NfcTagManager::addTechnology(TagTechnology tech, int handle, int libnfctype)
{
  mTechList.push_back(tech);
  mTechHandles.push_back(handle);
  mTechLibNfcTypes.push_back(libnfctype);
}

int NfcTagManager::getConnectedLibNfcType()
{
  if (mConnectedTechIndex != -1 && mConnectedTechIndex < (int)mTechLibNfcTypes.size()) {
    return mTechLibNfcTypes[mConnectedTechIndex];
  } else {
    return 0;
  }
}

bool NfcTagManager::doDisconnect()
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS nfaStat = NFA_STATUS_OK;

  gGeneralTransceiveTimeout = DEFAULT_GENERAL_TRANS_TIMEOUT;

  if (NfcTag::getInstance().getActivationState() != NfcTag::Active) {
    ALOGD("%s: tag already deactivated", __FUNCTION__);
    goto TheEnd;
  }

  nfaStat = NFA_Deactivate(FALSE);
  if (nfaStat != NFA_STATUS_OK)
    ALOGE("%s: deactivate failed; error=0x%X", __FUNCTION__, nfaStat);

TheEnd:
  ALOGD("%s: exit", __FUNCTION__);
  return (nfaStat == NFA_STATUS_OK) ? true : false;
}

void NfcTagManager::formatStatus(bool isOk)
{
  sFormatOk = isOk;
  sem_post(&sFormatSem);
}

bool NfcTagManager::doWrite(std::vector<uint8_t>& buf)
{
  bool result = false;
  tNFA_STATUS status = 0;
  const int maxBufferSize = 1024;
  UINT8 buffer[maxBufferSize] = { 0 };
  UINT32 curDataSize = 0;
 
  uint8_t* p_data = reinterpret_cast<uint8_t*>(malloc(buf.size()));
  for (uint8_t idx = 0; idx < buf.size(); idx++)
    p_data[idx] = buf[idx];

  ALOGD("%s: enter; len = %zu", __FUNCTION__, buf.size());

  // Create the write semaphore.
  if (sem_init(&sWriteSem, 0, 0) == -1) {
    ALOGE("%s: semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
    free(p_data);
    return false;
  }

  sWriteWaitingForComplete = true;
  if (sCheckNdefStatus == NFA_STATUS_FAILED) {
    // If tag does not contain a NDEF message
    // and tag is capable of storing NDEF message.
    if (sCheckNdefCapable) {
      ALOGD("%s: try format", __FUNCTION__);
      sem_init(&sFormatSem, 0, 0);
      sFormatOk = false;
      status = NFA_RwFormatTag();
      sem_wait(&sFormatSem);
      sem_destroy(&sFormatSem);
      if (sFormatOk == false) // If format operation failed.
        goto TheEnd;
    }
    ALOGD("%s: try write", __FUNCTION__);
    status = NFA_RwWriteNDef(p_data, buf.size());
  } else if (buf.size() == 0) {
    // If (NXP TagWriter wants to erase tag) then create and write an empty ndef message.
    NDEF_MsgInit(buffer, maxBufferSize, &curDataSize);
    status = NDEF_MsgAddRec(buffer, maxBufferSize, &curDataSize, NDEF_TNF_EMPTY, NULL, 0, NULL, 0, NULL, 0);
    ALOGD("%s: create empty ndef msg; status=%u; size=%lu", __FUNCTION__, status, curDataSize);
    status = NFA_RwWriteNDef(buffer, curDataSize);
  } else {
    ALOGD("%s: NFA_RwWriteNDef", __FUNCTION__);
    status = NFA_RwWriteNDef(p_data, buf.size());
  }

  if (status != NFA_STATUS_OK) {
    ALOGE("%s: write/format error=%d", __FUNCTION__, status);
    goto TheEnd;
  }

  // Wait for write completion status
  sWriteOk = false;
  if (sem_wait(&sWriteSem)) {
    ALOGE("%s: wait semaphore (errno=0x%08x)", __FUNCTION__, errno);
    goto TheEnd;
  }

  result = sWriteOk;

TheEnd:
  // Destroy semaphore
  if (sem_destroy(&sWriteSem)) {
    ALOGE("%s: failed destroy semaphore (errno=0x%08x)", __FUNCTION__, errno);
  }
  sWriteWaitingForComplete = false;
  ALOGD("%s: exit; result=%d", __FUNCTION__, result);
  free(p_data);
  return result;
}

bool NfcTagManager::doIsNdefFormatable()
{
  bool isFormattable = false;

  switch (NfcTag::getInstance().getProtocol())
  {
    case NFA_PROTOCOL_T1T:
    case NFA_PROTOCOL_ISO15693:
      isFormattable = true;
      break;
    case NFA_PROTOCOL_T2T:
        isFormattable = NfcTag::getInstance().isMifareUltralight() ? true : false;
  }
  ALOGD("%s: is formattable=%u", __FUNCTION__, isFormattable);
  return isFormattable; 
}

bool NfcTagManager::presenceCheck() 
{
  bool result;
  pthread_mutex_lock(&mMutex);
  result = doPresenceCheck();
  pthread_mutex_unlock(&mMutex);
  return result;
}

void NfcTagManager::readNdef(std::vector<uint8_t>& buf) 
{
  pthread_mutex_lock(&mMutex);
  doRead(buf);
  pthread_mutex_unlock(&mMutex);
}

int NfcTagManager::checkNdefWithStatus(int ndefinfo[]) 
{
  int status = -1;
  pthread_mutex_lock(&mMutex);
  status = doCheckNdef(ndefinfo);
  pthread_mutex_unlock(&mMutex);
  return status;
}

bool NfcTagManager::writeNdef(NdefMessage& ndef) 
{
  bool result;
  std::vector<uint8_t> buf;
  ndef.toByteArray(buf);
  pthread_mutex_lock(&mMutex);
  result = doWrite(buf);
  pthread_mutex_unlock(&mMutex);
  return result;
}

bool NfcTagManager::makeReadOnly() 
{
  bool result;
  pthread_mutex_lock(&mMutex);
  result = doMakeReadonly();
  pthread_mutex_unlock(&mMutex);
  return result;
}

bool NfcTagManager::isNdefFormatable()
{
  return doIsNdefFormatable();
}

bool NfcTagManager::formatNdef()
{
  bool result;
  pthread_mutex_lock(&mMutex);
  result = doNdefFormat();
  pthread_mutex_unlock(&mMutex);
  return result;
}
