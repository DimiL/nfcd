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
#include "NfcDebug.h"
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
#ifdef NFCC_PN547
  #include "phNxpExtns.h"
#endif
}

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
  NCI_DEBUG("event=%u, eventData=%p", event, eventData);

  switch (event) {
    case NFA_NDEF_REGISTER_EVT: {
      tNFA_NDEF_REGISTER& ndef_reg = eventData->ndef_reg;
      NCI_DEBUG("NFA_NDEF_REGISTER_EVT; status=0x%X; h=0x%X",
                ndef_reg.status, ndef_reg.ndef_type_handle);
      sNdefTypeHandlerHandle = ndef_reg.ndef_type_handle;
      break;
    }

    case NFA_NDEF_DATA_EVT: {
      NCI_DEBUG("NFA_NDEF_DATA_EVT; data_len = %lu", eventData->ndef_data.len);
      sReadDataLen = eventData->ndef_data.len;
      sReadData = (uint8_t*) malloc(sReadDataLen);
      memcpy(sReadData, eventData->ndef_data.p_data, eventData->ndef_data.len);
      break;
    }

    default:
      NCI_ERROR("Unknown event %u ????", event);
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
    NCI_ERROR("Check NDEF Failed - status = %d", status);
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
      NCI_ERROR("Connect Failed - status = %d", status);
      if (status == STATUS_CODE_TARGET_LOST) {
        break;
      }
      continue;  // Try next handle.
    } else {
      NCI_DEBUG("Connect Succeeded! (status = %d)", status);
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
      NCI_ERROR("Check NDEF Failed - status = %d", status);
      if (status == STATUS_CODE_TARGET_LOST) {
        break;
      }
      continue;  // Try next handle.
    } else {
      NCI_DEBUG("Check Succeeded! (status = %d)", status);
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
  NCI_DEBUG("enter");
  int retCode = NFCSTATUS_SUCCESS;
  NfcTag& tag = NfcTag::GetInstance();

  if (tag.GetActivationState() != NfcTag::Active) {
    NCI_DEBUG("tag already deactivated");
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  // Special case for Kovio.
  if (tag.mTechList[0] == TECHNOLOGY_TYPE_KOVIO_BARCODE) {
    NCI_DEBUG("fake out reconnect for Kovio");
    goto TheEnd;
  }

  // This is only supported for type 2 or 4 (ISO_DEP) tags.
  if (tag.mTechLibNfcTypes[0] == NFA_PROTOCOL_ISO_DEP) {
    retCode = ReSelect(NFA_INTERFACE_ISO_DEP);
  } else if (tag.mTechLibNfcTypes[0] == NFA_PROTOCOL_T2T) {
    retCode = ReSelect(NFA_INTERFACE_FRAME);
  } else if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
    retCode = ReSelect(NFA_INTERFACE_MIFARE);
#endif
  }

TheEnd:
  NCI_DEBUG("exit 0x%X", retCode);
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
          NCI_DEBUG("Connect to a tech with a different handle");
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
  NCI_DEBUG("waiting for transceive: %d", sWaitingForTransceive);
  if (!sWaitingForTransceive) {
    return;
  }

  sTransceiveRfTimeout = true;
  sTransceiveEvent.NotifyOne();
}

void NfcTagManager::DoTransceiveComplete(uint8_t* aBuf,
                                         uint32_t aBufLen)
{
  NCI_DEBUG("data len=%d, waiting for transceive: %d", aBufLen, sWaitingForTransceive);
  NfcTag& tag = NfcTag::GetInstance();

  if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
    if (!EXTNS_GetCallBackFlag()) {
      EXTNS_MfcCallBack(aBuf, aBufLen);

      SyncEventGuard g(sTransceiveEvent);
      sTransceiveEvent.NotifyOne();
      return;
    }
#endif
  }

  if (!sWaitingForTransceive) {
    return;
  }

  sTransceiveDataLen = 0;
  if (aBufLen) {
    sTransceiveData = new uint8_t[aBufLen];
    if (!sTransceiveData) {
      NCI_ERROR("memory allocation error");
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

      tNFA_STATUS status = NFA_STATUS_FAILED;
      if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
        status = EXTNS_MfcTransceive(cmd, size);
#endif
      } else {
        status = NFA_SendRawFrame(cmd, size,
                   NFA_DM_DEFAULT_PRESENCE_CHECK_START_DELAY);
      }

      delete cmd;

      if (status != NFA_STATUS_OK) {
        NCI_ERROR("fail send; error=%d", status);
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
      if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
        uint8_t** sTransData = &sTransceiveData;
        if (NFCSTATUS_FAILED ==
            EXTNS_CheckMfcResponse(sTransData, &sTransceiveDataLen)) {
          NCI_ERROR("fail get response");
        }
#endif
      }
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
  NCI_DEBUG("enter");
  NfcTag& tag = NfcTag::GetInstance();

  sReadDataLen = 0;
  if (sReadData != NULL) {
    free(sReadData);
    sReadData = NULL;
  }

  if (sCheckNdefCurrentSize > 0) {
    {
      SyncEventGuard g(sReadEvent);
      sIsReadingNdefMessage = true;

      tNFA_STATUS status = NFA_STATUS_FAILED;
      if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
        status = EXTNS_MfcReadNDef();
#endif
      } else {
        status = NFA_RwReadNDef();
      }
      sReadEvent.Wait(); // Wait for NFA_READ_CPLT_EVT.
    }
    sIsReadingNdefMessage = false;

    if (sReadDataLen > 0) { // If stack actually read data from the tag.
      NCI_DEBUG("read %u bytes", sReadDataLen);
      for(uint32_t idx = 0; idx < sReadDataLen; idx++) {
        aBuf.push_back(sReadData[idx]);
      }
    }
  } else {
    NCI_DEBUG("create emtpy buffer");
    sReadDataLen = 0;
    sReadData = (uint8_t*) malloc(1);
    aBuf.push_back(sReadData[0]);
  }

  if (sReadData) {
    free(sReadData);
    sReadData = NULL;
  }
  sReadDataLen = 0;

  NCI_DEBUG("exit");
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
  NCI_DEBUG("enter");
  NfcTag& tag = NfcTag::GetInstance();

  // Special case for Kovio.
  if (tag.mTechList[0] == TECHNOLOGY_TYPE_KOVIO_BARCODE) {
    NCI_DEBUG("Kovio tag, no NDEF");
    aNdefInfo[0] = 0;
    aNdefInfo[1] = NDEF_MODE_READ_ONLY;
    return NFA_STATUS_FAILED;
  }

  // Create the write semaphore.
  if (sem_init(&sCheckNdefSem, 0, 0) == -1) {
    NCI_ERROR("Check NDEF semaphore creation failed (errno=0x%08x)", errno);
    return false;
  }

  tNFA_STATUS status = NFA_STATUS_FAILED;
  if (tag.GetActivationState() != NfcTag::Active) {
    NCI_ERROR("tag already deactivated");
    goto TheEnd;
  }

  sCheckNdefWaitingForComplete = true;
  if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
    NCI_DEBUG("try EXTNS_MfcCheckNDef");
    status = EXTNS_MfcCheckNDef();
#endif
  } else {
    NCI_DEBUG("try NFA_RwDetectNDef");
    status = NFA_RwDetectNDef();
  }

  if (status != NFA_STATUS_OK) {
    NCI_ERROR("NFA_RwDetectNDef failed, status = 0x%X", status);
    goto TheEnd;
  }

  // Wait for check NDEF completion status.
  if (sem_wait(&sCheckNdefSem)) {
    NCI_ERROR("Failed to wait for check NDEF semaphore (errno=0x%08x)", errno);
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
    NCI_DEBUG("unknown status 0x%X", sCheckNdefStatus);
    status = sCheckNdefStatus;
  }

TheEnd:
  // Destroy semaphore.
  if (sem_destroy(&sCheckNdefSem)) {
    NCI_ERROR("Failed to destroy check NDEF semaphore (errno=0x%08x)", errno);
  }
  sCheckNdefWaitingForComplete = false;
  NCI_DEBUG("exit; status=0x%X", status);
  return status;
}

void NfcTagManager::DoAbortWaits()
{
  NCI_DEBUG("enter");
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
  NCI_DEBUG("status=0x%X; is reading=%u", aStatus, sIsReadingNdefMessage);

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
    NCI_DEBUG("sCountTagAway=%d", sCountTagAway);
  sem_post(&sPresenceCheckSem);
}

bool NfcTagManager::DoNdefFormat()
{
  NCI_DEBUG("enter");
  sem_init(&sFormatSem, 0, 0);
  sFormatOk = false;

  tNFA_STATUS status = NFA_STATUS_FAILED;
  NfcTag& tag = NfcTag::GetInstance();
  if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
    uint8_t key1[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	status = EXTNS_MfcFormatTag(key1, sizeof(key1));
#endif
  } else {
    status = NFA_RwFormatTag();
  }

  if (status == NFA_STATUS_OK) {
    NCI_DEBUG("wait for completion");
    sem_wait(&sFormatSem);
    status = sFormatOk ? NFA_STATUS_OK : NFA_STATUS_FAILED;
  } else {
    NCI_ERROR("error status=%u", status);
  }
  sem_destroy(&sFormatSem);

  if(IsMifareTech(tag.mTechLibNfcTypes[0]) && !sFormatOk) {
#ifdef NFCC_PN547
    uint8_t key2[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
    sem_init(&sFormatSem, 0, 0);

    NCI_DEBUG("Format with Second Key");
    status = EXTNS_MfcFormatTag(key2, sizeof(key2));
    if (status == NFA_STATUS_OK) {
      NCI_DEBUG("2nd try wait for completion");
      sem_wait(&sFormatSem);
      status = sFormatOk ? NFA_STATUS_OK : NFA_STATUS_FAILED;
    } else {
      NCI_ERROR("error status=%u", status);
      sem_destroy(&sFormatSem);
    }

    if(!sFormatOk) {
      NCI_ERROR("Format with Second Key Failed");
    }
#endif
  }

  NCI_DEBUG("exit");
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
    NCI_ERROR("stack is busy");
    return;
  }

  if (!sCheckNdefWaitingForComplete) {
    NCI_ERROR("not waiting");
    return;
  }

  if (aFlags & RW_NDEF_FL_READ_ONLY) {
    NCI_DEBUG("flag read-only");
  }
  if (aFlags & RW_NDEF_FL_FORMATED) {
    NCI_DEBUG("flag formatted for ndef");
  }
  if (aFlags & RW_NDEF_FL_SUPPORTED) {
    NCI_DEBUG("flag ndef supported");
  }
  if (aFlags & RW_NDEF_FL_UNKNOWN) {
    NCI_DEBUG("flag all unknown");
  }
  if (aFlags & RW_NDEF_FL_FORMATABLE) {
    NCI_DEBUG("flag formattable");
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
    NCI_ERROR("unknown status=0x%X", aStatus);
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
  NfcTag& tag = NfcTag::GetInstance();

  NCI_DEBUG("enter");

  // Create the make_readonly semaphore.
  if (sem_init(&sMakeReadonlySem, 0, 0) == -1) {
    NCI_ERROR("Make readonly semaphore creation failed (errno=0x%08x)", errno);
    return false;
  }

  tNFA_STATUS status = NFA_STATUS_FAILED;
  sMakeReadonlyWaitingForComplete = true;

  if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
    status = EXTNS_MfcSetReadOnly();
#endif
  } else {
    // Hard-lock the tag (cannot be reverted).
    status = NFA_RwSetTagReadOnly(true);
  }

  // Hard-lock the tag (cannot be reverted).
  status = NFA_RwSetTagReadOnly(true);

  if (status != NFA_STATUS_OK) {
    NCI_ERROR("NFA_RwSetTagReadOnly failed, status = %d", status);
    goto TheEnd;
  }

  // Wait for check NDEF completion status.
  if (sem_wait(&sMakeReadonlySem)) {
    NCI_ERROR("Failed to wait for make_readonly semaphore (errno=0x%08x)", errno);
    goto TheEnd;
  }

  if (sMakeReadonlyStatus == NFA_STATUS_OK) {
    result = true;
  }

TheEnd:
  // Destroy semaphore.
  if (sem_destroy(&sMakeReadonlySem)) {
    NCI_ERROR("Failed to destroy read_only semaphore (errno=0x%08x)", errno);
  }
  sMakeReadonlyWaitingForComplete = false;
  return result;
}

// Register a callback to receive NDEF message from the tag
// from the NFA_NDEF_DATA_EVT.
void NfcTagManager::DoRegisterNdefTypeHandler()
{
  NCI_DEBUG("enter");
  sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
  NFA_RegisterNDefTypeHandler(TRUE, NFA_TNF_DEFAULT, (UINT8 *) "", 0, NdefHandlerCallback);
}

void NfcTagManager::DoDeregisterNdefTypeHandler()
{
  NCI_DEBUG("enter");
  NFA_DeregisterNDefTypeHandler(sNdefTypeHandlerHandle);
  sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
}

int NfcTagManager::DoConnect(int aTargetHandle)
{
  NCI_DEBUG("targetHandle = %d", aTargetHandle);
  int i = aTargetHandle;
  int retCode = NFCSTATUS_SUCCESS;
  NfcTag& tag = NfcTag::GetInstance();

  sNeedToSwitchRf = false;
  if (i >= NfcTag::MAX_NUM_TECHNOLOGY) {
    NCI_ERROR("Handle not found");
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  if (tag.GetActivationState() != NfcTag::Active) {
    NCI_ERROR("tag already deactivated");
    retCode = NFCSTATUS_FAILED;
    goto TheEnd;
  }

  sCurrentConnectedTargetType = tag.mTechList[i];
  if (tag.mTechLibNfcTypes[i] != NFC_PROTOCOL_ISO_DEP) {
    NCI_DEBUG("Nfc type = %d, do nothing for non ISO_DEP", tag.mTechLibNfcTypes[i]);
    retCode = NFCSTATUS_SUCCESS;
    goto TheEnd;
  }

  if (tag.mTechList[i] == TECHNOLOGY_TYPE_ISO14443_3A || tag.mTechList[i] == TECHNOLOGY_TYPE_ISO14443_3B) {
    NCI_DEBUG("switching to tech: %d need to switch rf intf to frame", tag.mTechList[i]);
    // Connecting to NfcA or NfcB don't actually switch until/unless we get a transceive.
    sNeedToSwitchRf = true;
  } else {
    // Connecting back to IsoDep or NDEF.
    return (SwitchRfInterface(NFA_INTERFACE_ISO_DEP) ? NFCSTATUS_SUCCESS : NFCSTATUS_FAILED);
  }

TheEnd:
  NCI_DEBUG("exit 0x%X", retCode);
  return retCode;
}

bool NfcTagManager::DoPresenceCheck()
{
  NCI_DEBUG("enter");
  bool isPresent = false;
  NfcTag& tag = NfcTag::GetInstance();

  // Special case for Kovio. The deactivation would have already occurred
  // but was ignored so that normal tag opertions could complete.  Now we
  // want to process as if the deactivate just happened.
  if (tag.mTechList[0] == TECHNOLOGY_TYPE_KOVIO_BARCODE) {
    NCI_DEBUG("Kovio, force deactivate handling");
    tNFA_DEACTIVATED deactivated = {NFA_DEACTIVATE_TYPE_IDLE};

    tag.SetDeactivationState(deactivated);
    DoResetPresenceCheck();
    tag.ConnectionEventHandler(NFA_DEACTIVATED_EVT, NULL);
    DoAbortWaits();
    tag.Abort();

    return false;
  }

  if (IsNfcActive() == false) {
    NCI_DEBUG("NFC is no longer active.");
    return false;
  }

  if (tag.GetActivationState() != NfcTag::Active) {
    NCI_DEBUG("tag already deactivated");
    return false;
  }

  if (sem_init(&sPresenceCheckSem, 0, 0) == -1) {
    NCI_ERROR("semaphore creation failed (errno=0x%08x)", errno);
    return false;
  }

  tNFA_STATUS status = NFA_STATUS_OK;
#ifdef NFA_DM_PRESENCE_CHECK_OPTION
  status = NFA_RwPresenceCheck(NFA_RW_PRES_CHK_DEFAULT);
#else
  status = NFA_RwPresenceCheck();
#endif

  if (status == NFA_STATUS_OK) {
    if (sem_wait(&sPresenceCheckSem)) {
      NCI_ERROR("failed to wait (errno=0x%08x)", errno);
    } else {
      isPresent = (sCountTagAway > 3) ? false : true;
    }
  }

  if (sem_destroy(&sPresenceCheckSem)) {
    NCI_ERROR("Failed to destroy check NDEF semaphore (errno=0x%08x)", errno);
  }

  if (isPresent == false)
    NCI_DEBUG("tag absent ????");

  return isPresent;
}

int NfcTagManager::ReSelect(tNFA_INTF_TYPE aRfInterface)
{
  NCI_DEBUG("enter; rf intf = %d", aRfInterface);
  NfcTag& tag = NfcTag::GetInstance();

  tNFA_STATUS status;
  int rVal = 1;

  do
  {
    // If tag has shutdown, abort this method.
    if (tag.IsNdefDetectionTimedOut()) {
      NCI_DEBUG("ndef detection timeout; break");
      rVal = STATUS_CODE_TARGET_LOST;
      break;
    }

    {
      SyncEventGuard g(sReconnectEvent);
      gIsTagDeactivating = true;
      NCI_DEBUG("deactivate to sleep");
      if (NFA_STATUS_OK != (status = NFA_Deactivate(TRUE))) { // Deactivate to sleep state.
        NCI_ERROR("deactivate failed, status = %d", status);
        break;
      }

      if (sReconnectEvent.Wait(1000) == false) { // If timeout occurred.
        NCI_ERROR("timeout waiting for deactivate");
      }
    }

    if (tag.GetActivationState() != NfcTag::Sleep) {
      NCI_DEBUG("tag is not in sleep");
      rVal = STATUS_CODE_TARGET_LOST;
      break;
    }

    gIsTagDeactivating = false;

    {
      SyncEventGuard g2 (sReconnectEvent);

      sConnectWaitingForComplete = true;
      NCI_DEBUG("select interface %u", aRfInterface);
      gIsSelectingRfInterface = true;
      status = NFA_Select(
                 tag.mTechHandles[0], tag.mTechLibNfcTypes[0], aRfInterface);
      if (NFA_STATUS_OK != status) {
        NCI_ERROR("NFA_Select failed, status = %d", status);
        break;
      }

      sConnectOk = false;
      if (sReconnectEvent.Wait(1000) == false) { // If timeout occured.
          NCI_ERROR("timeout waiting for select");
          break;
      }
    }

    NCI_DEBUG("select completed; sConnectOk=%d", sConnectOk);
    if (tag.GetActivationState() != NfcTag::Active) {
      NCI_DEBUG("tag is not active");
      rVal = STATUS_CODE_TARGET_LOST;
      break;
    }
    rVal = (sConnectOk) ? 0 : 1;
  } while (0);

  sConnectWaitingForComplete = false;
  gIsTagDeactivating = false;
  gIsSelectingRfInterface = false;
  NCI_DEBUG("exit; status=%d", rVal);
  return rVal;
}

bool NfcTagManager::SwitchRfInterface(tNFA_INTF_TYPE aRfInterface)
{
  NCI_DEBUG("rf intf = %d", aRfInterface);
  NfcTag& tag = NfcTag::GetInstance();

  if (tag.mTechLibNfcTypes[0] != NFC_PROTOCOL_ISO_DEP) {
    NCI_DEBUG("protocol: %d not ISO_DEP, do nothing", tag.mTechLibNfcTypes[0]);
    return true;
  }

  sRfInterfaceMutex.Lock();
  NCI_DEBUG("new rf intf = %d, cur rf intf = %d", aRfInterface, sCurrentRfInterface);

  bool rVal = true;
  if (aRfInterface != sCurrentRfInterface) {
    if ((rVal = (0 == ReSelect(aRfInterface)))) {
      sCurrentRfInterface = aRfInterface;
    }
  }

  sRfInterfaceMutex.Unlock();
  return rVal;
}

bool NfcTagManager::IsMifareTech(int aTechTypes)
{
#ifdef NFCC_PN547
  return aTechTypes == NFA_PROTOCOL_MIFARE;
#endif
  return false;
}

NdefType NfcTagManager::GetNdefType(int aLibnfcType)
{
  NdefType ndefType = NDEF_UNKNOWN_TYPE;

  // For NFA, libnfcType is mapped to the protocol value received
  // in the NFA_ACTIVATED_EVT and NFA_DISC_RESULT_EVT event.
  switch (aLibnfcType) {
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
#ifdef NFCC_PN547
    case NFA_PROTOCOL_MIFARE:
      ndefType = NDEF_MIFARE_CLASSIC_TAG;
      break;
#endif
    case NFA_PROTOCOL_ISO15693:
    case NFA_PROTOCOL_INVALID:
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
  NCI_DEBUG("enter");
  tNFA_STATUS nfaStat = NFA_STATUS_OK;
  NfcTag& tag = NfcTag::GetInstance();

  gGeneralTransceiveTimeout = DEFAULT_GENERAL_TRANS_TIMEOUT;

  if (tag.GetActivationState() != NfcTag::Active) {
    NCI_DEBUG("tag already deactivated");
    goto TheEnd;
  }

  nfaStat = NFA_Deactivate(FALSE);
  if (nfaStat != NFA_STATUS_OK)
    NCI_ERROR("deactivate failed; error=0x%X", nfaStat);

TheEnd:
  NCI_DEBUG("exit");
  return (nfaStat == NFA_STATUS_OK) ? true : false;
}

void NfcTagManager::DoFormatStatus(bool aIsOk)
{
  sFormatOk = aIsOk;
  sem_post(&sFormatSem);
}

bool NfcTagManager::DoWrite(std::vector<uint8_t>& aBuf)
{
  bool result = false;
  const int maxBufferSize = 1024;
  UINT8 buffer[maxBufferSize] = { 0 };
  UINT32 curDataSize = 0;
  NfcTag& tag = NfcTag::GetInstance();

  uint8_t* p_data = reinterpret_cast<uint8_t*>(malloc(aBuf.size()));
  for (uint8_t idx = 0; idx < aBuf.size(); idx++) {
    p_data[idx] = aBuf[idx];
  }

  NCI_DEBUG("enter; len = %zu", aBuf.size());

  // Create the write semaphore.
  if (sem_init(&sWriteSem, 0, 0) == -1) {
    NCI_ERROR("semaphore creation failed (errno=0x%08x)", errno);
    free(p_data);
    return false;
  }

  tNFA_STATUS status = NFA_STATUS_FAILED;
  sWriteWaitingForComplete = true;
  if (sCheckNdefStatus == NFA_STATUS_FAILED) {
    // If tag does not contain a NDEF message
    // and tag is capable of storing NDEF message.
    if (sCheckNdefCapable) {
      NCI_DEBUG("try format");
      sem_init(&sFormatSem, 0, 0);
      sFormatOk = false;

      if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
        uint8_t key1[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        status = EXTNS_MfcFormatTag(key1, sizeof(key1));
#endif
      } else {
        status = NFA_RwFormatTag();
      }

      sem_wait(&sFormatSem);
      sem_destroy(&sFormatSem);

      if(IsMifareTech(tag.mTechLibNfcTypes[0]) && !sFormatOk) {
#ifdef NFCC_PN547
        uint8_t key2[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
        sem_init (&sFormatSem, 0, 0);
        status = EXTNS_MfcFormatTag(key2, sizeof(key2));
        sem_wait(&sFormatSem);
        sem_destroy(&sFormatSem);
#endif
      }

      if (!sFormatOk) { // If format operation failed.
        goto TheEnd;
      }
    }
    NCI_DEBUG("try write");
    status = NFA_RwWriteNDef(p_data, aBuf.size());
  } else if (aBuf.size() == 0) {
    // If (NXP TagWriter wants to erase tag) then create and write an empty ndef message.
    NDEF_MsgInit(buffer, maxBufferSize, &curDataSize);
    status = NDEF_MsgAddRec(buffer, maxBufferSize, &curDataSize, NDEF_TNF_EMPTY, NULL, 0, NULL, 0, NULL, 0);
    NCI_DEBUG("create empty ndef msg; status=%u; size=%lu", status, curDataSize);

    if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
      status = EXTNS_MfcWriteNDef(buffer, curDataSize);
#endif
    } else {
      status = NFA_RwWriteNDef(buffer, curDataSize);
    }
  } else {
    NCI_DEBUG("NFA_RwWriteNDef");
    if (IsMifareTech(tag.mTechLibNfcTypes[0])) {
#ifdef NFCC_PN547
      status = EXTNS_MfcWriteNDef(p_data, aBuf.size());
#endif
    } else {
      status = NFA_RwWriteNDef(p_data, aBuf.size());
    }
  }

  if (status != NFA_STATUS_OK) {
    NCI_ERROR("write/format error=%d", status);
    goto TheEnd;
  }

  // Wait for write completion status
  sWriteOk = false;
  if (sem_wait(&sWriteSem)) {
    NCI_ERROR("wait semaphore (errno=0x%08x)", errno);
    goto TheEnd;
  }

  result = sWriteOk;

TheEnd:
  // Destroy semaphore
  if (sem_destroy(&sWriteSem)) {
    NCI_ERROR("failed destroy semaphore (errno=0x%08x)", errno);
  }
  sWriteWaitingForComplete = false;
  NCI_DEBUG("exit; result=%d", result);
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
#ifdef NFCC_PN547
    case NFA_PROTOCOL_MIFARE:
#endif
      isFormattable = true;
      break;
    case NFA_PROTOCOL_T2T:
      isFormattable = tag.IsMifareUltralight();
      break;
  }
  NCI_DEBUG("is formattable=%u", isFormattable);
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
