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

#include "NfcTagManager.h"

#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "NdefMessage.h"
#include "TagTechnology.h"
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
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

extern bool IsNfcActive();
extern int gGeneralTransceiveTimeout;

// Flag for nfa callback indicating we are deactivating for RF interface switch.
bool    gIsTagDeactivating = false;
// Flag for nfa callback indicating we are selecting for RF interface switch.
bool    gIsSelectingRfInterface = false;

#define STATUS_CODE_TARGET_LOST    146  // This error code comes from the service.

static uint32_t        sCheckNdefCurrentSize = 0;
static tNFA_STATUS     sCheckNdefStatus = 0;      // Whether tag already contains a NDEF message.
static bool            sCheckNdefCapable = false; // Whether tag has NDEF capability.
static tNFA_HANDLE     sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
static tNFA_INTF_TYPE  sCurrentRfInterface = NFA_INTERFACE_ISO_DEP;
static uint8_t*        sTransceiveData = NULL;
static uint32_t        sTransceiveDataLen = 0;
static bool            sWaitingForTransceive = false;
static bool            sTransceiveRfTimeout = false;
static bool            sNeedToSwitchRf = false;
static Mutex           sRfInterfaceMutex;
static uint32_t        sReadDataLen = 0;
static uint8_t*        sReadData = NULL;
static bool            sIsReadingNdefMessage = false;
static SyncEvent       sReadEvent;
static sem_t           sWriteSem;
static sem_t           sFormatSem;
static SyncEvent       sTransceiveEvent;
static SyncEvent       sReconnectEvent;
static sem_t           sCheckNdefSem;
static sem_t           sPresenceCheckSem;
static sem_t           sMakeReadonlySem;
static IntervalTimer   sSwitchBackTimer; // Timer used to tell us to switch back to ISO_DEP frame interface.
static bool            sWriteOk = false;
static bool            sWriteWaitingForComplete = false;
static bool            sFormatOk = false;
static bool            sConnectOk = false;
static bool            sConnectWaitingForComplete = false;
static uint32_t        sCheckNdefMaxSize = 0;
static bool            sCheckNdefCardReadOnly = false;
static bool            sCheckNdefWaitingForComplete = false;
static int             sCountTagAway = 0;  // Count the consecutive number of presence-check failures.
static tNFA_STATUS     sMakeReadonlyStatus = NFA_STATUS_FAILED;
static bool            sMakeReadonlyWaitingForComplete = false;
static TechnologyType  sCurrentConnectedTargetType = TECHNOLOGY_TYPE_UNKNOWN;

static void NdefHandlerCallback(tNFA_NDEF_EVT event, tNFA_NDEF_EVT_DATA *eventData)
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

NdefInfo* NfcTagManager::DoReadNdefInfo()
{
  int ndefinfo[2];
  int status;
  NdefInfo* pNdefInfo = NULL;
  status = DoCheckNdef(ndefinfo);
  if (status != 0) {
    ALOGE("%s: Check NDEF Failed - status = %d", __FUNCTION__, status);
  } else {
    NdefType ndefType = GetNdefType(GetConnectedLibNfcType());

    pNdefInfo = new NdefInfo();
    pNdefInfo->ndefType = ndefType;
    pNdefInfo->maxSupportedLength = ndefinfo[0];
    pNdefInfo->isReadOnly = (ndefinfo[1] == NDEF_MODE_READ_ONLY);
    pNdefInfo->isFormatable = DoIsNdefFormatable();
  }

  return pNdefInfo;
}

NdefMessage* NfcTagManager::DoReadNdef()
{
  NdefMessage* ndefMsg = NULL;
  bool foundFormattable = false;
  int formattableHandle = 0;
  int formattableLibNfcType = 0;
  int status;
  NfcTag& tag = NfcTag::GetInstance();

  for(uint32_t techIndex = 0; techIndex < mTechList.size(); techIndex++) {
    // Have we seen this handle before?
    for (uint32_t i = 0; i < techIndex; i++) {
      if (tag.mTechHandles[i] == tag.mTechHandles[techIndex]) {
        continue;  // Don't check duplicate handles.
      }
    }

    status = ConnectWithStatus(tag.mTechList[techIndex]);
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
      if (DoIsNdefFormatable()) {
        foundFormattable = true;
        formattableHandle = GetConnectedHandle();
        formattableLibNfcType = GetConnectedLibNfcType();
        // We'll only add formattable tech if no ndef is
        // found - this is because libNFC refuses to format
        // an already NDEF formatted tag.
      }
      // TODO : check why Android call reconnect here
      //reconnect();
    }

    int ndefinfo[2];
    status = DoCheckNdef(ndefinfo);
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
    DoRead(buf);
    if (buf.size() != 0) {
      ndefMsg = new NdefMessage();
      if (ndefMsg->Init(buf)) {
        // TODO : check why android call reconnect here
        //reconnect();
      } else {
        generateEmptyNdef = true;
      }
    } else {
        generateEmptyNdef = true;
    }

    if (generateEmptyNdef == true) {
      delete ndefMsg;
      ndefMsg = NULL;
      //reconnect();
    }
    break;
  }

  return ndefMsg;
}

int NfcTagManager::ReconnectWithStatus()
{
  ALOGD("%s: enter", __FUNCTION__);
  int retCode = NFCSTATUS_SUCCESS;
  NfcTag& tag = NfcTag::GetInstance();

  if (tag.GetActivationState() != NfcTag::Active) {
    ALOGD("%s: tag already deactivated", __FUNCTION__);
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  // Special case for Kovio.
  if (tag.mTechList[0] == TECHNOLOGY_TYPE_KOVIO_BARCODE) {
    ALOGD("%s: fake out reconnect for Kovio", __FUNCTION__);
    goto TheEnd;
  }

  // This is only supported for type 2 or 4 (ISO_DEP) tags.
  if (tag.mTechLibNfcTypes[0] == NFA_PROTOCOL_ISO_DEP) {
    retCode = ReSelect(NFA_INTERFACE_ISO_DEP);
  } else if (tag.mTechLibNfcTypes[0] == NFA_PROTOCOL_T2T) {
    retCode = ReSelect(NFA_INTERFACE_FRAME);
  }

TheEnd:
  ALOGD("%s: exit 0x%X", __FUNCTION__, retCode);
  return retCode;
}

int NfcTagManager::ReconnectWithStatus(int aTargetHandle)
{
  int status = -1;
  status = DoConnect(aTargetHandle);
  return status;
}

int NfcTagManager::ConnectWithStatus(TechnologyType aTechnology)
{
  int status = -1;
  NfcTag& tag = NfcTag::GetInstance();

  for (uint32_t i = 0; i < mTechList.size(); i++) {
    if (tag.mTechList[i] == aTechnology) {
      // Get the handle and connect, if not already connected.
      if (mConnectedHandle != tag.mTechHandles[i]) {
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
          status = DoConnect(i);
        } else {
          // Connect to a tech with a different handle.
          ALOGD("%s: Connect to a tech with a different handle", __FUNCTION__);
          status = ReconnectWithStatus(i);
        }
        if (status == 0) {
          mConnectedHandle = tag.mTechHandles[i];
          mConnectedTechIndex = i;
        }
      } else {
        // 1) We are connected to a technology which has the same
        //    handle; we do not support connecting at a different
        //    level (libnfc auto-activates to the max level on
        //    any handle).
        // 2) We are connecting to the ndef technology - always
        //    allowed.
        if (aTechnology == TECHNOLOGY_TYPE_NDEF) {
          i = 0;
        }

        status = ReconnectWithStatus(i);
        if (status == 0) {
          mConnectedTechIndex = i;
        }
      }
      break;
    }
  }

  return status;
}

void NfcTagManager::NotifyRfTimeout()
{
  SyncEventGuard g(sTransceiveEvent);
  ALOGD("%s: waiting for transceive: %d", __FUNCTION__, sWaitingForTransceive);
  if (!sWaitingForTransceive) {
    return;
  }

  sTransceiveRfTimeout = true;
  sTransceiveEvent.NotifyOne();
}

void NfcTagManager::DoTransceiveComplete(uint8_t* aBuf,
                                         uint32_t aBufLen)
{
  ALOGD("%s: data len=%d, waiting for transceive: %d", __FUNCTION__, aBufLen, sWaitingForTransceive);
  if (!sWaitingForTransceive) {
    return;
  }

  sTransceiveDataLen = 0;
  if (aBufLen) {
    sTransceiveData = new uint8_t[aBufLen];
    if (!sTransceiveData) {
      ALOGE("%s: memory allocation error", __FUNCTION__);
    } else {
      sTransceiveDataLen = aBufLen;
      memcpy(sTransceiveData, aBuf, aBufLen);
    }
  }

  {
    SyncEventGuard g(sTransceiveEvent);
    sTransceiveEvent.NotifyOne();
  }
}

bool NfcTagManager::DoTransceive(const std::vector<uint8_t>& aCommand,
                                 std::vector<uint8_t>& aOutResponse)
{
  bool waitOk = false;
  bool isNack = false;
  bool targetLost = false;
  NfcTag& tag = NfcTag::GetInstance();

  if (tag.GetActivationState() != NfcTag::Active) {
    return false;
  }

  do {
    {
      SyncEventGuard g(sTransceiveEvent);
      sTransceiveRfTimeout = false;
      sWaitingForTransceive = true;
      delete sTransceiveData;

      uint32_t size = aCommand.size();
      uint8_t* cmd = new uint8_t[size];
      std::copy(aCommand.begin(), aCommand.end(), cmd);

      tNFA_STATUS status = NFA_SendRawFrame(cmd, size,
                             NFA_DM_DEFAULT_PRESENCE_CHECK_START_DELAY);
      delete cmd;

      if (status != NFA_STATUS_OK) {
        ALOGE("%s: fail send; error=%d", __FUNCTION__, status);
        break;
      }
      waitOk = sTransceiveEvent.Wait(
                 tag.GetTransceiveTimeout(sCurrentConnectedTargetType));
    }

    if (!waitOk ||
        sTransceiveRfTimeout ||
        (tag.GetActivationState() != NfcTag::Active)) {
      targetLost = true;
      break;
    }

    if (!sTransceiveDataLen) {
      break;
    }

    if ((tag.GetProtocol() == NFA_PROTOCOL_T2T) &&
        tag.IsT2tNackResponse(sTransceiveData, sTransceiveDataLen)) {
      isNack = true;
    }

    if (!isNack) {
      for (size_t i = 0; i < sTransceiveDataLen; i++) {
        aOutResponse.push_back(sTransceiveData[i]);
      }
    }

    delete sTransceiveData;
    sTransceiveData = NULL;
    sTransceiveDataLen = 0;
  } while(0);

  sWaitingForTransceive = false;

  return !targetLost;
}


void NfcTagManager::DoRead(std::vector<uint8_t>& aBuf)
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
      sReadEvent.Wait(); // Wait for NFA_READ_CPLT_EVT.
    }
    sIsReadingNdefMessage = false;

    if (sReadDataLen > 0) { // If stack actually read data from the tag.
      ALOGD("%s: read %u bytes", __FUNCTION__, sReadDataLen);
      for(uint32_t idx = 0; idx < sReadDataLen; idx++) {
        aBuf.push_back(sReadData[idx]);
      }
    }
  } else {
    ALOGD("%s: create emtpy buffer", __FUNCTION__);
    sReadDataLen = 0;
    sReadData = (uint8_t*) malloc(1);
    aBuf.push_back(sReadData[0]);
  }

  if (sReadData) {
    free(sReadData);
    sReadData = NULL;
  }
  sReadDataLen = 0;

  ALOGD("%s: exit", __FUNCTION__);
  return;
}

void NfcTagManager::DoWriteStatus(bool aIsWriteOk)
{
  if (sWriteWaitingForComplete != false) {
    sWriteWaitingForComplete = false;
    sWriteOk = aIsWriteOk;
    sem_post(&sWriteSem);
  }
}

int NfcTagManager::DoCheckNdef(int aNdefInfo[])
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS status = NFA_STATUS_FAILED;
  NfcTag& tag = NfcTag::GetInstance();

  // Special case for Kovio.
  if (tag.mTechList[0] == TECHNOLOGY_TYPE_KOVIO_BARCODE) {
    ALOGD("%s: Kovio tag, no NDEF", __FUNCTION__);
    aNdefInfo[0] = 0;
    aNdefInfo[1] = NDEF_MODE_READ_ONLY;
    return NFA_STATUS_FAILED;
  }

  // Special case for Kovio.
  if (tag.mTechList[0] == TECHNOLOGY_TYPE_KOVIO_BARCODE) {
    ALOGD("%s: Kovio tag, no NDEF", __FUNCTION__);
    aNdefInfo[0] = 0;
    aNdefInfo[1] = NDEF_MODE_READ_ONLY;
    return NFA_STATUS_FAILED;
  }

  // Create the write semaphore.
  if (sem_init(&sCheckNdefSem, 0, 0) == -1) {
    ALOGE("%s: Check NDEF semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
    return false;
  }

  if (tag.GetActivationState() != NfcTag::Active) {
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
    if (tag.GetProtocol() == NFA_PROTOCOL_T1T) {
      aNdefInfo[0] = tag.GetT1tMaxMessageSize();
    } else {
      aNdefInfo[0] = sCheckNdefMaxSize;
    }

    if (sCheckNdefCardReadOnly) {
      aNdefInfo[1] = NDEF_MODE_READ_ONLY;
    } else {
      aNdefInfo[1] = NDEF_MODE_READ_WRITE;
    }
    status = NFA_STATUS_OK;
  } else if (sCheckNdefStatus == NFA_STATUS_FAILED) {
    // Stack did not find a NDEF message on the tag.
    if (tag.GetProtocol() == NFA_PROTOCOL_T1T) {
      aNdefInfo[0] = tag.GetT1tMaxMessageSize();
    } else {
      aNdefInfo[0] = sCheckNdefMaxSize;
    }
    if (sCheckNdefCardReadOnly)
      aNdefInfo[1] = NDEF_MODE_READ_ONLY;
    else
      aNdefInfo[1] = NDEF_MODE_READ_WRITE;
    status = NFA_STATUS_FAILED;
  } else if (sCheckNdefStatus == NFA_STATUS_TIMEOUT) {
    Pn544InteropStopPolling();
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

void NfcTagManager::DoAbortWaits()
{
  ALOGD("%s", __FUNCTION__);
  {
    SyncEventGuard g(sReadEvent);
    sReadEvent.NotifyOne();
  }
  sem_post(&sWriteSem);
  sem_post(&sFormatSem);
  {
    SyncEventGuard g(sTransceiveEvent);
    sTransceiveEvent.NotifyOne();
  }
  {
    SyncEventGuard g (sReconnectEvent);
    sReconnectEvent.NotifyOne();
  }

  sem_post(&sCheckNdefSem);
  sem_post(&sPresenceCheckSem);
  sem_post(&sMakeReadonlySem);

  sCurrentConnectedTargetType = TECHNOLOGY_TYPE_UNKNOWN;
}

void NfcTagManager::DoReadCompleted(tNFA_STATUS aStatus)
{
  ALOGD("%s: status=0x%X; is reading=%u", __FUNCTION__, aStatus, sIsReadingNdefMessage);

  if (sIsReadingNdefMessage == false)
    return; // Not reading NDEF message right now, so just return.

  if (aStatus != NFA_STATUS_OK) {
    sReadDataLen = 0;
    if (sReadData)
      free (sReadData);
    sReadData = NULL;
  }
  SyncEventGuard g(sReadEvent);
  sReadEvent.NotifyOne();
}

void NfcTagManager::DoConnectStatus(bool aIsConnectOk)
{
  if (sConnectWaitingForComplete != false) {
    sConnectWaitingForComplete = false;
    sConnectOk = aIsConnectOk;
    SyncEventGuard g(sReconnectEvent);
    sReconnectEvent.NotifyOne();
  }
}

void NfcTagManager::DoDeactivateStatus(int aStatus)
{
  SyncEventGuard g(sReconnectEvent);
  sReconnectEvent.NotifyOne();
}

void NfcTagManager::DoResetPresenceCheck()
{
  sCountTagAway = 0;
}

void NfcTagManager::DoPresenceCheckResult(tNFA_STATUS aStatus)
{
  if (aStatus == NFA_STATUS_OK) {
    sCountTagAway = 0;
  } else {
    sCountTagAway++;
  }
  if (sCountTagAway > 0)
    ALOGD("%s: sCountTagAway=%d", __FUNCTION__, sCountTagAway);
  sem_post(&sPresenceCheckSem);
}

bool NfcTagManager::DoNdefFormat()
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

void NfcTagManager::DoCheckNdefResult(tNFA_STATUS aStatus,
                                      uint32_t aMaxSize,
                                      uint32_t aCurrentSize,
                                      uint8_t aFlags)
{
  // This function's flags parameter is defined using the following macros
  // in nfc/include/rw_api.h;
  // #define RW_NDEF_FL_READ_ONLY  0x01    /* Tag is read only              */
  // #define RW_NDEF_FL_FORMATED   0x02    /* Tag formated for NDEF         */
  // #define RW_NDEF_FL_SUPPORTED  0x04    /* NDEF supported by the tag     */
  // #define RW_NDEF_FL_UNKNOWN    0x08    /* Unable to find if tag is ndef capable/formated/read only */
  // #define RW_NDEF_FL_FORMATABLE 0x10    /* Tag supports format operation */

  if (aStatus == NFC_STATUS_BUSY) {
    ALOGE("%s: stack is busy", __FUNCTION__);
    return;
  }

  if (!sCheckNdefWaitingForComplete) {
    ALOGE("%s: not waiting", __FUNCTION__);
    return;
  }

  if (aFlags & RW_NDEF_FL_READ_ONLY) {
    ALOGD("%s: flag read-only", __FUNCTION__);
  }
  if (aFlags & RW_NDEF_FL_FORMATED) {
    ALOGD("%s: flag formatted for ndef", __FUNCTION__);
  }
  if (aFlags & RW_NDEF_FL_SUPPORTED) {
    ALOGD("%s: flag ndef supported", __FUNCTION__);
  }
  if (aFlags & RW_NDEF_FL_UNKNOWN) {
    ALOGD("%s: flag all unknown", __FUNCTION__);
  }
  if (aFlags & RW_NDEF_FL_FORMATABLE) {
    ALOGD("%s: flag formattable", __FUNCTION__);
  }

  sCheckNdefWaitingForComplete = false;
  sCheckNdefStatus = aStatus;
  sCheckNdefCapable = false; // Assume tag is NOT ndef capable.
  if (sCheckNdefStatus == NFA_STATUS_OK) {
    // NDEF content is on the tag.
    sCheckNdefMaxSize = aMaxSize;
    sCheckNdefCurrentSize = aCurrentSize;
    sCheckNdefCardReadOnly = aFlags & RW_NDEF_FL_READ_ONLY;
    sCheckNdefCapable = true;
  } else if (sCheckNdefStatus == NFA_STATUS_FAILED) {
    // No NDEF content on the tag.
    sCheckNdefMaxSize = 0;
    sCheckNdefCurrentSize = 0;
    sCheckNdefCardReadOnly = aFlags & RW_NDEF_FL_READ_ONLY;
    if ((aFlags & RW_NDEF_FL_UNKNOWN) == 0) { // If stack understands the tag.
      if (aFlags & RW_NDEF_FL_SUPPORTED) {    // If tag is ndef capable.
        sCheckNdefCapable = true;
      }
    }
  } else {
    ALOGE("%s: unknown status=0x%X", __FUNCTION__, aStatus);
    sCheckNdefMaxSize = 0;
    sCheckNdefCurrentSize = 0;
    sCheckNdefCardReadOnly = false;
  }
  sem_post(&sCheckNdefSem);
}

void NfcTagManager::DoMakeReadonlyResult(tNFA_STATUS aStatus)
{
  if (sMakeReadonlyWaitingForComplete != false) {
    sMakeReadonlyWaitingForComplete = false;
    sMakeReadonlyStatus = aStatus;

    sem_post(&sMakeReadonlySem);
  }
}

bool NfcTagManager::DoMakeReadonly()
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
void NfcTagManager::DoRegisterNdefTypeHandler()
{
  ALOGD("%s", __FUNCTION__);
  sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
  NFA_RegisterNDefTypeHandler(TRUE, NFA_TNF_DEFAULT, (UINT8 *) "", 0, NdefHandlerCallback);
}

void NfcTagManager::DoDeregisterNdefTypeHandler()
{
  ALOGD("%s", __FUNCTION__);
  NFA_DeregisterNDefTypeHandler(sNdefTypeHandlerHandle);
  sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
}

int NfcTagManager::DoConnect(int aTargetHandle)
{
  ALOGD("%s: targetHandle = %d", __FUNCTION__, aTargetHandle);
  int i = aTargetHandle;
  int retCode = NFCSTATUS_SUCCESS;
  NfcTag& tag = NfcTag::GetInstance();

  sNeedToSwitchRf = false;
  if (i >= NfcTag::MAX_NUM_TECHNOLOGY) {
    ALOGE("%s: Handle not found", __FUNCTION__);
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  if (tag.GetActivationState() != NfcTag::Active) {
    ALOGE("%s: tag already deactivated", __FUNCTION__);
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  sCurrentConnectedTargetType = tag.mTechList[i];
  if (tag.mTechLibNfcTypes[i] != NFC_PROTOCOL_ISO_DEP) {
    ALOGD("%s() Nfc type = %d, do nothing for non ISO_DEP", __FUNCTION__, tag.mTechLibNfcTypes[i]);
    retCode = NFCSTATUS_SUCCESS;
    goto TheEnd;
  }

  if (tag.mTechList[i] == TECHNOLOGY_TYPE_ISO14443_3A || tag.mTechList[i] == TECHNOLOGY_TYPE_ISO14443_3B) {
    ALOGD("%s: switching to tech: %d need to switch rf intf to frame", __FUNCTION__, tag.mTechList[i]);
    // Connecting to NfcA or NfcB don't actually switch until/unless we get a transceive.
    sNeedToSwitchRf = true;
  } else {
    // Connecting back to IsoDep or NDEF.
    return (SwitchRfInterface(NFA_INTERFACE_ISO_DEP) ? NFCSTATUS_SUCCESS : NFCSTATUS_FAILED);
  }

TheEnd:
  ALOGD("%s: exit 0x%X", __FUNCTION__, retCode);
  return retCode;
}

bool NfcTagManager::DoPresenceCheck()
{
  ALOGD("%s", __FUNCTION__);
  tNFA_STATUS status = NFA_STATUS_OK;
  bool isPresent = false;
  NfcTag& tag = NfcTag::GetInstance();

  // Special case for Kovio. The deactivation would have already occurred
  // but was ignored so that normal tag opertions could complete.  Now we
  // want to process as if the deactivate just happened.
  if (tag.mTechList[0] == TECHNOLOGY_TYPE_KOVIO_BARCODE) {
    ALOGD("%s: Kovio, force deactivate handling", __FUNCTION__);
    tNFA_DEACTIVATED deactivated = {NFA_DEACTIVATE_TYPE_IDLE};

    tag.SetDeactivationState(deactivated);
    DoResetPresenceCheck();
    tag.ConnectionEventHandler(NFA_DEACTIVATED_EVT, NULL);
    DoAbortWaits();
    tag.Abort();

    return false;
  }

  if (IsNfcActive() == false) {
    ALOGD("%s: NFC is no longer active.", __FUNCTION__);
    return false;
  }

  if (tag.GetActivationState() != NfcTag::Active) {
    ALOGD("%s: tag already deactivated", __FUNCTION__);
    return false;
  }

  if (sem_init(&sPresenceCheckSem, 0, 0) == -1) {
    ALOGE("%s: semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
    return false;
  }

#ifdef NFA_DM_PRESENCE_CHECK_OPTION
  status = NFA_RwPresenceCheck(NFA_RW_PRES_CHK_DEFAULT);
#else
  status = NFA_RwPresenceCheck();
#endif

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

int NfcTagManager::ReSelect(tNFA_INTF_TYPE aRfInterface)
{
  ALOGD("%s: enter; rf intf = %d", __FUNCTION__, aRfInterface);
  NfcTag& tag = NfcTag::GetInstance();

  tNFA_STATUS status;
  int rVal = 1;

  do
  {
    // If tag has shutdown, abort this method.
    if (tag.IsNdefDetectionTimedOut()) {
      ALOGD("%s: ndef detection timeout; break", __FUNCTION__);
      rVal = STATUS_CODE_TARGET_LOST;
      break;
    }

    {
      SyncEventGuard g(sReconnectEvent);
      gIsTagDeactivating = true;
      ALOGD("%s: deactivate to sleep", __FUNCTION__);
      if (NFA_STATUS_OK != (status = NFA_Deactivate(TRUE))) { // Deactivate to sleep state.
        ALOGE("%s: deactivate failed, status = %d", __FUNCTION__, status);
        break;
      }

      if (sReconnectEvent.Wait(1000) == false) { // If timeout occurred.
        ALOGE("%s: timeout waiting for deactivate", __FUNCTION__);
      }
    }

    if (tag.GetActivationState() != NfcTag::Sleep) {
      ALOGD("%s: tag is not in sleep", __FUNCTION__);
      rVal = STATUS_CODE_TARGET_LOST;
      break;
    }

    gIsTagDeactivating = false;

    {
      SyncEventGuard g2 (sReconnectEvent);

      sConnectWaitingForComplete = true;
      ALOGD("%s: select interface %u", __FUNCTION__, aRfInterface);
      gIsSelectingRfInterface = true;
      status = NFA_Select(
                 tag.mTechHandles[0], tag.mTechLibNfcTypes[0], aRfInterface);
      if (NFA_STATUS_OK != status) {
        ALOGE("%s: NFA_Select failed, status = %d", __FUNCTION__, status);
        break;
      }

      sConnectOk = false;
      if (sReconnectEvent.Wait(1000) == false) { // If timeout occured.
          ALOGE("%s: timeout waiting for select", __FUNCTION__);
          break;
      }
    }

    ALOGD("%s: select completed; sConnectOk=%d", __FUNCTION__, sConnectOk);
    if (tag.GetActivationState() != NfcTag::Active) {
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

bool NfcTagManager::SwitchRfInterface(tNFA_INTF_TYPE aRfInterface)
{
  ALOGD("%s: rf intf = %d", __FUNCTION__, aRfInterface);
  NfcTag& tag = NfcTag::GetInstance();

  if (tag.mTechLibNfcTypes[0] != NFC_PROTOCOL_ISO_DEP) {
    ALOGD("%s: protocol: %d not ISO_DEP, do nothing", __FUNCTION__, tag.mTechLibNfcTypes[0]);
    return true;
  }

  sRfInterfaceMutex.Lock();
  ALOGD("%s: new rf intf = %d, cur rf intf = %d", __FUNCTION__, aRfInterface, sCurrentRfInterface);

  bool rVal = true;
  if (aRfInterface != sCurrentRfInterface) {
    if ((rVal = (0 == ReSelect(aRfInterface)))) {
      sCurrentRfInterface = aRfInterface;
    }
  }

  sRfInterfaceMutex.Unlock();
  return rVal;
}

NdefType NfcTagManager::GetNdefType(int libnfcType)
{
  NdefType ndefType = NDEF_UNKNOWN_TYPE;

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

int NfcTagManager::GetConnectedLibNfcType()
{
  NfcTag& tag = NfcTag::GetInstance();

  if (mConnectedTechIndex != -1 && mConnectedTechIndex < (int)mTechLibNfcTypes.size()) {
    return tag.mTechLibNfcTypes[mConnectedTechIndex];
  } else {
    return 0;
  }
}

bool NfcTagManager::DoDisconnect()
{
  ALOGD("%s: enter", __FUNCTION__);
  tNFA_STATUS nfaStat = NFA_STATUS_OK;
  NfcTag& tag = NfcTag::GetInstance();

  gGeneralTransceiveTimeout = DEFAULT_GENERAL_TRANS_TIMEOUT;

  if (tag.GetActivationState() != NfcTag::Active) {
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

void NfcTagManager::FormatStatus(bool aIsOk)
{
  sFormatOk = aIsOk;
  sem_post(&sFormatSem);
}

bool NfcTagManager::DoWrite(std::vector<uint8_t>& aBuf)
{
  bool result = false;
  tNFA_STATUS status = 0;
  const int maxBufferSize = 1024;
  UINT8 buffer[maxBufferSize] = { 0 };
  UINT32 curDataSize = 0;

  uint8_t* p_data = reinterpret_cast<uint8_t*>(malloc(aBuf.size()));
  for (uint8_t idx = 0; idx < aBuf.size(); idx++)
    p_data[idx] = aBuf[idx];

  ALOGD("%s: enter; len = %zu", __FUNCTION__, aBuf.size());

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
    status = NFA_RwWriteNDef(p_data, aBuf.size());
  } else if (aBuf.size() == 0) {
    // If (NXP TagWriter wants to erase tag) then create and write an empty ndef message.
    NDEF_MsgInit(buffer, maxBufferSize, &curDataSize);
    status = NDEF_MsgAddRec(buffer, maxBufferSize, &curDataSize, NDEF_TNF_EMPTY, NULL, 0, NULL, 0, NULL, 0);
    ALOGD("%s: create empty ndef msg; status=%u; size=%lu", __FUNCTION__, status, curDataSize);
    status = NFA_RwWriteNDef(buffer, curDataSize);
  } else {
    ALOGD("%s: NFA_RwWriteNDef", __FUNCTION__);
    status = NFA_RwWriteNDef(p_data, aBuf.size());
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

bool NfcTagManager::DoIsNdefFormatable()
{
  bool isFormattable = false;
  NfcTag& tag = NfcTag::GetInstance();

  switch (tag.GetProtocol())
  {
    case NFA_PROTOCOL_T1T:
    case NFA_PROTOCOL_ISO15693:
      isFormattable = true;
      break;
    case NFA_PROTOCOL_T2T:
        isFormattable = tag.IsMifareUltralight() ? true : false;
  }
  ALOGD("%s: is formattable=%u", __FUNCTION__, isFormattable);
  return isFormattable;
}

bool NfcTagManager::Connect(TagTechnology aTechnology)
{
  pthread_mutex_lock(&mMutex);
  int status = ConnectWithStatus(NfcNciUtil::ToTechnologyType(aTechnology));
  pthread_mutex_unlock(&mMutex);
  return NFCSTATUS_SUCCESS == status;
}

bool NfcTagManager::Disconnect()
{
  bool result = false;

  mIsPresent = false;

  pthread_mutex_lock(&mMutex);
  result = DoDisconnect();
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

bool NfcTagManager::Reconnect()
{
  pthread_mutex_lock(&mMutex);
  int status = ReconnectWithStatus();
  pthread_mutex_unlock(&mMutex);
  return NFCSTATUS_SUCCESS == status;
}

bool NfcTagManager::PresenceCheck()
{
  bool result;
  pthread_mutex_lock(&mMutex);
  result = DoPresenceCheck();
  pthread_mutex_unlock(&mMutex);
  return result;
}

NdefMessage* NfcTagManager::ReadNdef()
{
  pthread_mutex_lock(&mMutex);
  NdefMessage* ndef = DoReadNdef();
  pthread_mutex_unlock(&mMutex);
  return ndef;
}

NdefInfo* NfcTagManager::ReadNdefInfo()
{
  pthread_mutex_lock(&mMutex);
  NdefInfo* ndefInfo = DoReadNdefInfo();
  pthread_mutex_unlock(&mMutex);
  return ndefInfo;
}

bool NfcTagManager::WriteNdef(NdefMessage& aNdef)
{
  bool result;
  std::vector<uint8_t> buf;
  aNdef.ToByteArray(buf);
  pthread_mutex_lock(&mMutex);
  result = DoWrite(buf);
  pthread_mutex_unlock(&mMutex);
  return result;
}

bool NfcTagManager::MakeReadOnly()
{
  bool result;
  pthread_mutex_lock(&mMutex);
  result = DoMakeReadonly();
  pthread_mutex_unlock(&mMutex);
  return result;
}

bool NfcTagManager::IsNdefFormatable()
{
  return DoIsNdefFormatable();
}

bool NfcTagManager::FormatNdef()
{
  bool result;
  pthread_mutex_lock(&mMutex);
  result = DoNdefFormat();
  pthread_mutex_unlock(&mMutex);
  return result;
}

bool NfcTagManager::Transceive(const std::vector<uint8_t>& aCommand,
                               std::vector<uint8_t>& aOutResponse)
{
  bool result;
  pthread_mutex_lock(&mMutex);
  result = DoTransceive(aCommand, aOutResponse);
  pthread_mutex_unlock(&mMutex);
  return result;
}
