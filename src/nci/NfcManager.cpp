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

#include "NfcManager.h"

#include "OverrideLog.h"
#include "config.h"
#include "NfcAdaptation.h"

#include "SyncEvent.h"
#include "SecureElement.h"
#include "PeerToPeer.h"
#include "PowerSwitch.h"
#include "NfcTag.h"
#include "Pn544Interop.h"
#include "LlcpSocket.h"
#include "LlcpServiceSocket.h"
#include "NfcTagManager.h"
#include "P2pDevice.h"

extern "C"
{
  #include "nfa_api.h"
  #include "nfa_p2p_api.h"
  #include "rw_api.h"
  #include "nfa_ee_api.h"
  #include "nfc_brcm_defs.h"
  #include "ce_api.h"
}

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

extern bool gIsTagDeactivating;
extern bool gIsSelectingRfInterface;

/**
 * public variables and functions
 */
nfc_data                gNat;
int                     gGeneralTransceiveTimeout = 1000;
void                    doStartupConfig();
void                    startRfDiscovery(bool isStart);
bool                    startStopPolling(bool isStartPolling);

/**
 * private variables and functions
 */
static SyncEvent            sNfaEnableEvent;                // Event for NFA_Enable().
static SyncEvent            sNfaDisableEvent;               // Event for NFA_Disable().
static SyncEvent            sNfaEnableDisablePollingEvent;  // Event for NFA_EnablePolling(), NFA_DisablePolling().
static SyncEvent            sNfaSetConfigEvent;             // Event for Set_Config....
static SyncEvent            sNfaGetConfigEvent;             // Event for Get_Config....

static bool                 sIsNfaEnabled = false;
static bool                 sDiscoveryEnabled = false;      // Is polling for tag?
static bool                 sIsDisabling = false;
static bool                 sRfEnabled = false;             // Whether RF discovery is enabled.
static bool                 sSeRfActive = false;            // Whether RF with SE is likely active.
static bool                 sP2pActive = false;             // Whether p2p was last active.
static bool                 sAbortConnlessWait = false;
static bool                 sIsSecElemSelected = false;     // Has NFC service selected a sec elem.

#define CONFIG_UPDATE_TECH_MASK     (1 << 1)
#define DEFAULT_TECH_MASK           (NFA_TECHNOLOGY_MASK_A \
                                     | NFA_TECHNOLOGY_MASK_B \
                                     | NFA_TECHNOLOGY_MASK_F \
                                     | NFA_TECHNOLOGY_MASK_ISO15693 \
                                     | NFA_TECHNOLOGY_MASK_B_PRIME \
                                     | NFA_TECHNOLOGY_MASK_A_ACTIVE \
                                     | NFA_TECHNOLOGY_MASK_F_ACTIVE \
                                     | NFA_TECHNOLOGY_MASK_KOVIO)


static void nfaConnectionCallback(UINT8 event, tNFA_CONN_EVT_DATA *eventData);
static void nfaDeviceManagementCallback(UINT8 event, tNFA_DM_CBACK_DATA *eventData);
static bool isPeerToPeer(tNFA_ACTIVATED& activated);
static bool isListenMode(tNFA_ACTIVATED& activated);

static UINT16 sCurrentConfigLen;
static UINT8 sConfig[256];

NfcManager::NfcManager()
 : mP2pDevice(NULL)
 , mNfcTagManager(NULL)
{
  mP2pDevice = new P2pDevice();
  mNfcTagManager = new NfcTagManager();
}

NfcManager::~NfcManager()
{
  delete mP2pDevice;
  delete mNfcTagManager;
}

/**
 * Interfaces.
 */

void* NfcManager::queryInterface(const char* name)
{
  if (0 == strcmp(name, INTERFACE_P2P_DEVICE))
    return reinterpret_cast<void*>(mP2pDevice);
  else if (0 == strcmp(name, INTERFACE_TAG_MANAGER))
    return reinterpret_cast<void*>(mNfcTagManager);

  return NULL;
}

bool NfcManager::initialize()
{
  tNFA_STATUS stat = NFA_STATUS_OK;
  unsigned long num = 5;

  // Initialize PowerSwitch.
  PowerSwitch::getInstance().initialize(PowerSwitch::FULL_POWER);

  // Start GKI, NCI task, NFC task.
  NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
  theInstance.Initialize();

  {
    SyncEventGuard guard(sNfaEnableEvent);
    tHAL_NFC_ENTRY* halFuncEntries = theInstance.GetHalEntryFuncs();
    NFA_Init(halFuncEntries);

    stat = NFA_Enable(nfaDeviceManagementCallback, nfaConnectionCallback);
    if (stat == NFA_STATUS_OK) {
      num = initializeGlobalAppLogLevel();
      CE_SetTraceLevel(num);
      LLCP_SetTraceLevel(num);
      NFC_SetTraceLevel(num);
      RW_SetTraceLevel(num);
      NFA_SetTraceLevel(num);
      NFA_P2pSetTraceLevel(num);

      sNfaEnableEvent.wait(); // Wait for NFA command to finish.
    } else {
      ALOGE("%s: NFA_Enable fail, error = 0x%X", __FUNCTION__, stat);
    }
  }

  if (stat == NFA_STATUS_OK) {
    if (sIsNfaEnabled) {
      SecureElement::getInstance().initialize(this);
      NfcTagManager::doRegisterNdefTypeHandler();
      NfcTag::getInstance().initialize(this);

      PeerToPeer::getInstance().initialize(this);
      PeerToPeer::getInstance().handleNfcOnOff(true);

      // Add extra configuration here (work-arounds, etc.).
      {
        if (GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
          gNat.tech_mask = num;
        else
          gNat.tech_mask = DEFAULT_TECH_MASK;

        ALOGD("%s: tag polling tech mask = 0x%X", __FUNCTION__, gNat.tech_mask);
      }

      // If this value exists, set polling interval.
      if (GetNumValue(NAME_NFA_DM_DISC_DURATION_POLL, &num, sizeof(num)))
        NFA_SetRfDiscoveryDuration(num);

      // Do custom NFCA startup configuration.
      doStartupConfig();
      goto TheEnd;
    }
  }

  if (sIsNfaEnabled)
    stat = NFA_Disable(FALSE /* ungraceful */);

  theInstance.Finalize();

TheEnd:
  if (sIsNfaEnabled) {
    PowerSwitch::getInstance().setLevel(PowerSwitch::LOW_POWER);
  }

  return sIsNfaEnabled;
}

bool NfcManager::deinitialize()
{
  ALOGD("%s: enter", __FUNCTION__);

  sIsDisabling = true;
  pn544InteropAbortNow();
  SecureElement::getInstance().finalize();

  if (sIsNfaEnabled) {
    SyncEventGuard guard(sNfaDisableEvent);

    tNFA_STATUS stat = NFA_Disable(TRUE /* graceful */);
    if (stat == NFA_STATUS_OK) {
      ALOGD("%s: wait for completion", __FUNCTION__);
      sNfaDisableEvent.wait(); // Wait for NFA command to finish.
      PeerToPeer::getInstance().handleNfcOnOff(false);
    } else {
      ALOGE("%s: NFA_Disable fail; error = 0x%X", __FUNCTION__, stat);
    }
  }

  NfcTagManager::doAbortWaits();
  NfcTag::getInstance().abort();
  sAbortConnlessWait = true;
  // TODO : Implement LLCP.
  sIsNfaEnabled = false;
  sDiscoveryEnabled = false;
  sIsDisabling = false;
  sIsSecElemSelected = false;

  {
    // Unblock NFA_EnablePolling() and NFA_DisablePolling().
    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    sNfaEnableDisablePollingEvent.notifyOne();
  }

  NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
  theInstance.Finalize();

  ALOGD("%s: exit", __FUNCTION__);
  return true;
}

bool NfcManager::enableDiscovery()
{
  tNFA_TECHNOLOGY_MASK tech_mask = DEFAULT_TECH_MASK;

  tech_mask = (tNFA_TECHNOLOGY_MASK)gNat.tech_mask;

  if (sDiscoveryEnabled) {
    ALOGW("%s: already polling", __FUNCTION__);
    return true;
  }

  tNFA_STATUS stat = NFA_STATUS_OK;

  PowerSwitch::getInstance().setLevel(PowerSwitch::FULL_POWER);

  if (sRfEnabled) {
    // Stop RF discovery to reconfigure.
    startRfDiscovery(false);
  }

  {
    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    stat = NFA_EnablePolling(tech_mask);
    if (stat == NFA_STATUS_OK) {
      ALOGD("%s: wait for enable event", __FUNCTION__);
      sDiscoveryEnabled = true;
      sNfaEnableDisablePollingEvent.wait(); // Wait for NFA_POLL_ENABLED_EVT.
      ALOGD("%s: got enabled event", __FUNCTION__);
    } else {
      ALOGE("%s: NFA_EnablePolling fail; error = 0x%X", __FUNCTION__, stat);
    }
  }

  // Start P2P listening if tag polling was enabled or the mask was 0.
  if (sDiscoveryEnabled || (tech_mask == 0)) {
    ALOGD("%s: enable p2pListening", __FUNCTION__);
    PeerToPeer::getInstance().enableP2pListening(true);

    //if NFC service has deselected the sec elem, then apply default routes.
    if (!sIsSecElemSelected) {
      // TODO : Emulator to support SE routing .
      SecureElement::getInstance().routeToDefault();
      //stat = SecureElement::getInstance().routeToDefault() ?
      //  NFA_STATUS_OK : NFA_STATUS_FAILED;
    }
  }

  // Actually start discovery.
  startRfDiscovery(true);

  PowerSwitch::getInstance().setModeOn(PowerSwitch::DISCOVERY);

  ALOGD("%s: exit", __FUNCTION__);
  return stat == NFA_STATUS_OK;
}

bool NfcManager::disableDiscovery()
{
  tNFA_STATUS status = NFA_STATUS_OK;
  ALOGD("%s: enter;", __FUNCTION__);

  pn544InteropAbortNow();
  if (!sDiscoveryEnabled) {
    ALOGD("%s: already disabled", __FUNCTION__);
    goto TheEnd;
  }

  // Stop RF Discovery.
  startRfDiscovery(false);

  if (sDiscoveryEnabled) {
    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    status = NFA_DisablePolling();
    if (status == NFA_STATUS_OK) {
      sDiscoveryEnabled = false;
      sNfaEnableDisablePollingEvent.wait(); // Wait for NFA_POLL_DISABLED_EVT.
    } else {
      ALOGE("%s: NFA_DisablePolling fail, error=0x%X", __FUNCTION__, status);
    }
  }

  PeerToPeer::getInstance().enableP2pListening(false);

  // If nothing is active after this, then tell the controller to power down.
  if (!(PowerSwitch::getInstance().setModeOff(PowerSwitch::DISCOVERY))) {
    PowerSwitch::getInstance().setLevel(PowerSwitch::LOW_POWER);
  }

  // We may have had RF field notifications that did not cause
  // any activate/deactive events. For example, caused by wireless
  // charging orbs. Those may cause us to go to sleep while the last
  // field event was indicating a field. To prevent sticking in that
  // state, always reset the rf field status when we disable discovery.

  SecureElement::getInstance().resetRfFieldStatus();
TheEnd:
  ALOGD("%s: exit", __FUNCTION__);
  return status == NFA_STATUS_OK;
}

bool NfcManager::enablePolling()
{
  return startStopPolling(true);
}

bool NfcManager::disablePolling()
{
  return startStopPolling(false);
}

bool NfcManager::enableP2pListening()
{
  return PeerToPeer::getInstance().enableP2pListening(true);
}

bool NfcManager::disableP2pListening()
{
  return PeerToPeer::getInstance().enableP2pListening(false);
}

bool NfcManager::checkLlcp()
{
  // Not used in NCI case.
  return true;
}

bool NfcManager::activateLlcp()
{
  // Not used in NCI case.
  return true;
}

ILlcpSocket* NfcManager::createLlcpSocket(int sap, int miu, int rw, int linearBufferLength)
{
  ALOGD("%s: enter; sap=%d; miu=%d; rw=%d; buffer len=%d", __FUNCTION__, sap, miu, rw, linearBufferLength);

  const uint32_t handle = PeerToPeer::getInstance().getNewHandle();
  if(!(PeerToPeer::getInstance().createClient(handle, miu, rw)))
    ALOGE("%s: fail create p2p client", __FUNCTION__);

  LlcpSocket* pLlcpSocket = new LlcpSocket(handle, sap, miu, rw);

  ALOGD("%s: exit", __FUNCTION__);
  return static_cast<ILlcpSocket*>(pLlcpSocket);
}

ILlcpServerSocket* NfcManager::createLlcpServerSocket(int sap, const char* sn, int miu, int rw, int linearBufferLength)
{
  ALOGD("%s: enter; sap=%d; sn =%s; miu=%d; rw=%d; buffer len= %d", __FUNCTION__, sap, sn, miu, rw, linearBufferLength);
  const uint32_t handle = PeerToPeer::getInstance().getNewHandle();
  LlcpServiceSocket* pLlcpServiceSocket = new LlcpServiceSocket(handle, linearBufferLength, miu, rw);

  if (!(PeerToPeer::getInstance().registerServer(handle, sn))) {
    ALOGE("%s: register server fail", __FUNCTION__);
    return NULL;
  }

  ALOGD("%s: exit", __FUNCTION__);
  return static_cast<ILlcpServerSocket*>(pLlcpServiceSocket);
}

void NfcManager::setP2pInitiatorModes(int modes)
{
  ALOGD ("%s: modes=0x%X", __FUNCTION__, modes);

  tNFA_TECHNOLOGY_MASK mask = 0;
  if (modes & 0x01) mask |= NFA_TECHNOLOGY_MASK_A;
  if (modes & 0x02) mask |= NFA_TECHNOLOGY_MASK_F;
  if (modes & 0x04) mask |= NFA_TECHNOLOGY_MASK_F;
  if (modes & 0x08) mask |= NFA_TECHNOLOGY_MASK_A_ACTIVE;
  if (modes & 0x10) mask |= NFA_TECHNOLOGY_MASK_F_ACTIVE;
  if (modes & 0x20) mask |= NFA_TECHNOLOGY_MASK_F_ACTIVE;
  gNat.tech_mask = mask;

  //this function is not called by the NFC service nor exposed by public API.
}

void NfcManager::setP2pTargetModes(int modes)
{
  ALOGD("%s: modes=0x%X", __FUNCTION__, modes);
  // Map in the right modes.
  tNFA_TECHNOLOGY_MASK mask = 0;
  if (modes & 0x01) mask |= NFA_TECHNOLOGY_MASK_A;
  if (modes & 0x02) mask |= NFA_TECHNOLOGY_MASK_F;
  if (modes & 0x04) mask |= NFA_TECHNOLOGY_MASK_F;
  if (modes & 0x08) mask |= NFA_TECHNOLOGY_MASK_A_ACTIVE | NFA_TECHNOLOGY_MASK_F_ACTIVE;

  PeerToPeer::getInstance().setP2pListenMask(mask);
  // This function is not called by the NFC service nor exposed by public API.
}

bool NfcManager::enableSecureElement()
{
  bool result = true;

  if (sIsSecElemSelected) {
    ALOGD("%s: already selected", __FUNCTION__);
    return result;
  }

  PowerSwitch::getInstance().setLevel(PowerSwitch::FULL_POWER);

  if (sRfEnabled) {
    // Stop RF Discovery if we were polling.
    startRfDiscovery(false);
  }

  result = SecureElement::getInstance().activate();
  if (result) {
    SecureElement::getInstance().routeToSecureElement();
  }

  sIsSecElemSelected = true;

  startRfDiscovery(true);
  PowerSwitch::getInstance().setModeOn(PowerSwitch::SE_ROUTING);

  return result;
}

bool NfcManager::disableSecureElement()
{
  bool result = false;
  bool reDiscover = false;

  if (!sIsSecElemSelected) {
    ALOGE("%s: already deselected", __FUNCTION__);
    goto TheEnd;
  }

  if (PowerSwitch::getInstance().getLevel() == PowerSwitch::LOW_POWER) {
    ALOGD("%s: do not deselect while power is OFF", __FUNCTION__);
    sIsSecElemSelected = false;
    goto TheEnd;
  }

  if (sRfEnabled) {
    // Stop RF Discovery if we were polling.
    startRfDiscovery(false);
    reDiscover = true;
  }

  result = SecureElement::getInstance().routeToDefault();
  sIsSecElemSelected = false;

  //if controller is not routing to sec elems AND there is no pipe connected,
  //then turn off the sec elems
  if (!SecureElement::getInstance().isBusy()) {
    SecureElement::getInstance().deactivate();
  }

TheEnd:
  if (reDiscover) {
    startRfDiscovery(true);
  }

  //if nothing is active after this, then tell the controller to power down
  if (!PowerSwitch::getInstance().setModeOff(PowerSwitch::SE_ROUTING)) {
    PowerSwitch::getInstance().setLevel(PowerSwitch::LOW_POWER);
  }

  return result;
}

/**
 * Private functions.
 */
static void handleRfDiscoveryEvent(tNFC_RESULT_DEVT* discoveredDevice)
{
  if (discoveredDevice->more) {
    // There is more discovery notification coming.
    return;
  }

  bool isP2p = NfcTag::getInstance().isP2pDiscovered();
  if (isP2p) {
    // Select the peer that supports P2P.
    NfcTag::getInstance().selectP2p();
  } else {
    // Select the first of multiple tags that is discovered.
    NfcTag::getInstance().selectFirstTag();
  }
}

void nfaDeviceManagementCallback(UINT8 dmEvent, tNFA_DM_CBACK_DATA* eventData)
{
  ALOGD("%s: enter; event=0x%X", __FUNCTION__, dmEvent);

  switch (dmEvent) {
    // Result of NFA_Enable.
    case NFA_DM_ENABLE_EVT: {
      SyncEventGuard guard(sNfaEnableEvent);
      ALOGD("%s: NFA_DM_ENABLE_EVT; status=0x%X",__FUNCTION__, eventData->status);
      sIsNfaEnabled = eventData->status == NFA_STATUS_OK;
      sIsDisabling = false;
      sNfaEnableEvent.notifyOne();
      break;
    }
    // Result of NFA_Disable.
    case NFA_DM_DISABLE_EVT: {
      SyncEventGuard guard(sNfaDisableEvent);
      ALOGD("%s: NFA_DM_DISABLE_EVT", __FUNCTION__);
      sIsNfaEnabled = false;
      sIsDisabling = false;
      sNfaDisableEvent.notifyOne();
      break;
    }
    // Result of NFA_SetConfig.
    case NFA_DM_SET_CONFIG_EVT:
      ALOGD("%s: NFA_DM_SET_CONFIG_EVT", __FUNCTION__);
      {
        SyncEventGuard guard(sNfaSetConfigEvent);
        sNfaSetConfigEvent.notifyOne();
      }
      break;
    // Result of NFA_GetConfig.
    case NFA_DM_GET_CONFIG_EVT:
      ALOGD("%s: NFA_DM_GET_CONFIG_EVT", __FUNCTION__);
      {
        SyncEventGuard guard(sNfaGetConfigEvent);
        if (eventData->status == NFA_STATUS_OK &&
            eventData->get_config.tlv_size <= sizeof(sConfig)) {
          sCurrentConfigLen = eventData->get_config.tlv_size;
          memcpy(sConfig, eventData->get_config.param_tlvs, eventData->get_config.tlv_size);
        } else {
          ALOGE("%s: NFA_DM_GET_CONFIG failed; status=0x%X", __FUNCTION__, eventData->status);
          sCurrentConfigLen = 0;
        }
        sNfaGetConfigEvent.notifyOne();
      }
      break;

    case NFA_DM_RF_FIELD_EVT:
      ALOGD("%s: NFA_DM_RF_FIELD_EVT; status=0x%X; field status=%u", __FUNCTION__,
              eventData->rf_field.status, eventData->rf_field.rf_field_status);

      if (sIsDisabling || !sIsNfaEnabled) {
        break;
      }

      if (!sP2pActive && eventData->rf_field.status == NFA_STATUS_OK) {
        SecureElement::getInstance().notifyRfFieldEvent(
          eventData->rf_field.rf_field_status == NFA_DM_RF_FIELD_ON);
      }
      break;

    case NFA_DM_NFCC_TRANSPORT_ERR_EVT:
    case NFA_DM_NFCC_TIMEOUT_EVT: {
      if (dmEvent == NFA_DM_NFCC_TIMEOUT_EVT)
        ALOGD("%s: NFA_DM_NFCC_TIMEOUT_EVT; abort all outstanding operations", __FUNCTION__);
      else
        ALOGD("%s: NFA_DM_NFCC_TRANSPORT_ERR_EVT; abort all outstanding operations", __FUNCTION__);

      NfcTagManager::doAbortWaits();
      NfcTag::getInstance().abort();
      sAbortConnlessWait = true;
      // TODO : Implement LLCP.
      {
        ALOGD("%s: aborting sNfaEnableDisablePollingEvent", __FUNCTION__);
        SyncEventGuard guard(sNfaEnableDisablePollingEvent);
        sNfaEnableDisablePollingEvent.notifyOne();
      }
      {
        ALOGD("%s: aborting sNfaEnableEvent", __FUNCTION__);
        SyncEventGuard guard(sNfaEnableEvent);
        sNfaEnableEvent.notifyOne();
      }
      {
        ALOGD("%s: aborting sNfaDisableEvent", __FUNCTION__);
        SyncEventGuard guard(sNfaDisableEvent);
        sNfaDisableEvent.notifyOne();
      }
      sDiscoveryEnabled = false;
      PowerSwitch::getInstance().abort();

      if (!sIsDisabling && sIsNfaEnabled) {
        NFA_Disable(FALSE);
        sIsDisabling = true;
      } else {
        sIsNfaEnabled = false;
        sIsDisabling = false;
      }
      PowerSwitch::getInstance().initialize(PowerSwitch::UNKNOWN_LEVEL);
      ALOGD("%s: aborted all waiting events", __FUNCTION__);
    }
    break;

    case NFA_DM_PWR_MODE_CHANGE_EVT:
      PowerSwitch::getInstance().deviceManagementCallback(dmEvent, eventData);
      break;
    default:
      ALOGD("%s: unhandled event", __FUNCTION__);
      break;
  }
}

static void nfaConnectionCallback(UINT8 connEvent, tNFA_CONN_EVT_DATA* eventData)
{
  tNFA_STATUS status = NFA_STATUS_FAILED;
  ALOGD("%s: enter; event=0x%X", __FUNCTION__, connEvent);

  switch (connEvent) {
    // Whether polling successfully started.
    case NFA_POLL_ENABLED_EVT: {
      ALOGD("%s: NFA_POLL_ENABLED_EVT: status = 0x%X", __FUNCTION__, eventData->status);

      SyncEventGuard guard(sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.notifyOne();
      break;
    }
    // Listening/Polling stopped.
    case NFA_POLL_DISABLED_EVT: {
      ALOGD("%s: NFA_POLL_DISABLED_EVT: status = 0x%X", __FUNCTION__, eventData->status);

      SyncEventGuard guard(sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.notifyOne();
      break;
    }
    // RF Discovery started.
    case NFA_RF_DISCOVERY_STARTED_EVT: {
      ALOGD("%s: NFA_RF_DISCOVERY_STARTED_EVT: status = 0x%X", __FUNCTION__, eventData->status);

      SyncEventGuard guard(sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.notifyOne();
      break;
    }
    // RF Discovery stopped event.
    case NFA_RF_DISCOVERY_STOPPED_EVT: {
      ALOGD("%s: NFA_RF_DISCOVERY_STOPPED_EVT: status = 0x%X", __FUNCTION__, eventData->status);

      SyncEventGuard guard(sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.notifyOne();
      break;
    }
    // NFC link/protocol discovery notificaiton.
    case NFA_DISC_RESULT_EVT:
      status = eventData->disc_result.status;
      ALOGD("%s: NFA_DISC_RESULT_EVT: status = 0x%X", __FUNCTION__, status);
      if (status != NFA_STATUS_OK) {
        ALOGE("%s: NFA_DISC_RESULT_EVT error: status = 0x%x", __FUNCTION__, status);
      } else {
        NfcTag::getInstance().connectionEventHandler(connEvent, eventData);
        handleRfDiscoveryEvent(&eventData->disc_result.discovery_ntf);
      }
      break;
    // NFC link/protocol discovery select response.
    case NFA_SELECT_RESULT_EVT:
      ALOGD("%s: NFA_SELECT_RESULT_EVT: status = %d, gIsSelectingRfInterface = %d, sIsDisabling=%d", __FUNCTION__, eventData->status, gIsSelectingRfInterface, sIsDisabling);
      break;
    case NFA_DEACTIVATE_FAIL_EVT:
      ALOGD("%s: NFA_DEACTIVATE_FAIL_EVT: status = 0x%x", __FUNCTION__, eventData->status);
      break;
    // NFC link/protocol activated.
    case NFA_ACTIVATED_EVT:
      ALOGD("%s: NFA_ACTIVATED_EVT: gIsSelectingRfInterface=%d, sIsDisabling=%d", __FUNCTION__, gIsSelectingRfInterface, sIsDisabling);
      if (sIsDisabling || !sIsNfaEnabled)
        break;

      NfcTag::getInstance().setActivationState();
      if (gIsSelectingRfInterface) {
        NfcTagManager::doConnectStatus(true);
        break;
      }

      NfcTagManager::doResetPresenceCheck();
      if (isPeerToPeer(eventData->activated)) {
        sP2pActive = true;
        ALOGD("%s: NFA_ACTIVATED_EVT; is p2p", __FUNCTION__);
        // Disable RF field events in case of p2p.
        UINT8  nfa_disable_rf_events[] = { 0x00 };
        ALOGD("%s: Disabling RF field events", __FUNCTION__);
        status = NFA_SetConfig(NCI_PARAM_ID_RF_FIELD_INFO, sizeof(nfa_disable_rf_events),
                   &nfa_disable_rf_events[0]);
        ALOGD("%s: Disabled RF field events, status = 0x%x", __FUNCTION__, status);

        // For the SE, consider the field to be on while p2p is active.
        SecureElement::getInstance().notifyRfFieldEvent(true);
      } else if (pn544InteropIsBusy() == false) {
        NfcTag::getInstance().connectionEventHandler(connEvent, eventData);

        // We know it is not activating for P2P.  If it activated in
        // listen mode then it is likely for an SE transaction.
        // Send the RF Event.
        if (isListenMode(eventData->activated)) {
          sSeRfActive = true;
          SecureElement::getInstance().notifyListenModeState(true);
        }
      }
      break;
    // NFC link/protocol deactivated.
    case NFA_DEACTIVATED_EVT:
      ALOGD("%s: NFA_DEACTIVATED_EVT   Type: %u, gIsTagDeactivating: %d", __FUNCTION__, eventData->deactivated.type,gIsTagDeactivating);
      NfcTag::getInstance().setDeactivationState(eventData->deactivated);
      if (eventData->deactivated.type != NFA_DEACTIVATE_TYPE_SLEEP) {
        NfcTagManager::doResetPresenceCheck();
        NfcTag::getInstance().connectionEventHandler(connEvent, eventData);
        NfcTagManager::doAbortWaits();
        NfcTag::getInstance().abort();
      } else if (gIsTagDeactivating) {
        NfcTagManager::doDeactivateStatus(0);
      }

      // If RF is activated for what we think is a Secure Element transaction
      // and it is deactivated to either IDLE or DISCOVERY mode, notify w/event.
      if ((eventData->deactivated.type == NFA_DEACTIVATE_TYPE_IDLE) ||
          (eventData->deactivated.type == NFA_DEACTIVATE_TYPE_DISCOVERY)) {
        if (sSeRfActive) {
          sSeRfActive = false;
          if (!sIsDisabling && sIsNfaEnabled) {
            SecureElement::getInstance().notifyListenModeState(false);
          }
        } else if (sP2pActive) {
          sP2pActive = false;
          // Make sure RF field events are re-enabled.
          ALOGD("%s: NFA_DEACTIVATED_EVT; is p2p", __FUNCTION__);
          // Disable RF field events in case of p2p.
          UINT8 nfa_enable_rf_events[] = { 0x01 };

          if (!sIsDisabling && sIsNfaEnabled) {
            status = NFA_SetConfig(NCI_PARAM_ID_RF_FIELD_INFO, sizeof(nfa_enable_rf_events),
                                   &nfa_enable_rf_events[0]);
            ALOGD("%s: Enabled RF field events, status = 0x%x", __FUNCTION__, status);

            // Consider the field to be off at this point
            SecureElement::getInstance().notifyRfFieldEvent(false);
          }
        }
      }
      break;
    // TLV Detection complete.
    case NFA_TLV_DETECT_EVT:
      status = eventData->tlv_detect.status;
      ALOGD("%s: NFA_TLV_DETECT_EVT: status = %d, protocol = %d, num_tlvs = %d, num_bytes = %d",
        __FUNCTION__, status, eventData->tlv_detect.protocol,
        eventData->tlv_detect.num_tlvs, eventData->tlv_detect.num_bytes);
      if (status != NFA_STATUS_OK) {
        ALOGE("%s: NFA_TLV_DETECT_EVT error: status = 0x%X", __FUNCTION__, status);
      }
      break;
    // NDEF Detection complete.
    case NFA_NDEF_DETECT_EVT:
      // If status is failure, it means the tag does not contain any or valid NDEF data.
      // Pass the failure status to the NFC Service.
      status = eventData->ndef_detect.status;
      ALOGD("%s: NFA_NDEF_DETECT_EVT: status = 0x%X, protocol = %u, "
            "max_size = %lu, cur_size = %lu, flags = 0x%X", __FUNCTION__,
            status,
            eventData->ndef_detect.protocol, eventData->ndef_detect.max_size,
            eventData->ndef_detect.cur_size, eventData->ndef_detect.flags);
      NfcTag::getInstance().connectionEventHandler(connEvent, eventData);
      NfcTagManager::doCheckNdefResult(status,
        eventData->ndef_detect.max_size, eventData->ndef_detect.cur_size,
        eventData->ndef_detect.flags);
      break;
    // Data message received (for non-NDEF reads).
    case NFA_DATA_EVT:
      ALOGD("%s: NFA_DATA_EVT:  len = %d", __FUNCTION__, eventData->data.len);
      NfcTagManager::doTransceiveComplete(eventData->data.p_data,
                                          eventData->data.len);
      break;
    case NFA_RW_INTF_ERROR_EVT:
        ALOGD("%s: NFC_RW_INTF_ERROR_EVT", __FUNCTION__);
      NfcTagManager::notifyRfTimeout();
      break;
    // Select completed.
    case NFA_SELECT_CPLT_EVT:
      status = eventData->status;
      ALOGD("%s: NFA_SELECT_CPLT_EVT: status = 0x%X", __FUNCTION__, status);
      if (status != NFA_STATUS_OK) {
        ALOGE("%s: NFA_SELECT_CPLT_EVT error: status = 0x%X", __FUNCTION__, status);
      }
      break;
    // NDEF-read or tag-specific-read completed.
    case NFA_READ_CPLT_EVT:
      ALOGD("%s: NFA_READ_CPLT_EVT: status = 0x%X", __FUNCTION__, eventData->status);
      NfcTagManager::doReadCompleted(eventData->status);
      NfcTag::getInstance().connectionEventHandler(connEvent, eventData);
      break;
    // Write completed.
    case NFA_WRITE_CPLT_EVT:
      ALOGD("%s: NFA_WRITE_CPLT_EVT: status = 0x%X", __FUNCTION__, eventData->status);
      NfcTagManager::doWriteStatus(eventData->status == NFA_STATUS_OK);
      break;
    // Tag set as Read only.
    case NFA_SET_TAG_RO_EVT:
      ALOGD("%s: NFA_SET_TAG_RO_EVT: status = 0x%X", __FUNCTION__, eventData->status);
      NfcTagManager::doMakeReadonlyResult(eventData->status);
      break;
    // NDEF write started.
    case NFA_CE_NDEF_WRITE_START_EVT:
      ALOGD("%s: NFA_CE_NDEF_WRITE_START_EVT: status: 0x%X", __FUNCTION__, eventData->status);

      if (eventData->status != NFA_STATUS_OK)
        ALOGE("%s: NFA_CE_NDEF_WRITE_START_EVT error: status = 0x%X", __FUNCTION__, eventData->status);
      break;
    // NDEF write completed.
    case NFA_CE_NDEF_WRITE_CPLT_EVT:
      ALOGD("%s: FA_CE_NDEF_WRITE_CPLT_EVT: len = %lu", __FUNCTION__, eventData->ndef_write_cplt.len);
      break;
    // LLCP link is activated.
    case NFA_LLCP_ACTIVATED_EVT:
      ALOGD("%s: NFA_LLCP_ACTIVATED_EVT: is_initiator: %d  remote_wks: %d, remote_lsc: %d, remote_link_miu: %d, local_link_miu: %d",
           __FUNCTION__,
           eventData->llcp_activated.is_initiator,
           eventData->llcp_activated.remote_wks,
           eventData->llcp_activated.remote_lsc,
           eventData->llcp_activated.remote_link_miu,
           eventData->llcp_activated.local_link_miu);

      PeerToPeer::getInstance().llcpActivatedHandler(eventData->llcp_activated);
      break;
    // LLCP link is deactivated.
    case NFA_LLCP_DEACTIVATED_EVT:
      ALOGD("%s: NFA_LLCP_DEACTIVATED_EVT", __FUNCTION__);
      PeerToPeer::getInstance().llcpDeactivatedHandler(eventData->llcp_deactivated);
      break;
    // Received first packet over llcp.
    case NFA_LLCP_FIRST_PACKET_RECEIVED_EVT:
      ALOGD("%s: NFA_LLCP_FIRST_PACKET_RECEIVED_EVT", __FUNCTION__);
      PeerToPeer::getInstance().llcpFirstPacketHandler();
      break;

    case NFA_PRESENCE_CHECK_EVT:
      ALOGD("%s: NFA_PRESENCE_CHECK_EVT", __FUNCTION__);
      NfcTagManager::doPresenceCheckResult(eventData->status);
      break;

    case NFA_FORMAT_CPLT_EVT:
      ALOGD("%s: NFA_FORMAT_CPLT_EVT: status=0x%X", __FUNCTION__, eventData->status);
      NfcTagManager::formatStatus(eventData->status == NFA_STATUS_OK);
      break;

    case NFA_I93_CMD_CPLT_EVT:
      ALOGD("%s: NFA_I93_CMD_CPLT_EVT: status=0x%X", __FUNCTION__, eventData->status);
      break;

    case NFA_CE_UICC_LISTEN_CONFIGURED_EVT :
      ALOGD("%s: NFA_CE_UICC_LISTEN_CONFIGURED_EVT : status=0x%X", __FUNCTION__, eventData->status);
      SecureElement::getInstance().connectionEventHandler(connEvent, eventData);
      break;

    case NFA_SET_P2P_LISTEN_TECH_EVT:
      ALOGD("%s: NFA_SET_P2P_LISTEN_TECH_EVT", __FUNCTION__);
      PeerToPeer::getInstance().connectionEventHandler(connEvent, eventData);
      break;

    default:
      ALOGE("%s: unknown event ????", __FUNCTION__);
      break;
  }
}

void startRfDiscovery(bool isStart)
{
  tNFA_STATUS status = NFA_STATUS_FAILED;

  ALOGD("%s: is start=%d", __FUNCTION__, isStart);
  SyncEventGuard guard(sNfaEnableDisablePollingEvent);
  status  = isStart ? NFA_StartRfDiscovery() : NFA_StopRfDiscovery();
  if (status == NFA_STATUS_OK) {
    sNfaEnableDisablePollingEvent.wait(); // Wait for NFA_RF_DISCOVERY_xxxx_EVT.
    sRfEnabled = isStart;
  } else {
    ALOGE("%s: NFA_StartRfDiscovery/NFA_StopRfDiscovery fail; error=0x%X", __FUNCTION__, status);
  }
}

void doStartupConfig()
{
  tNFA_STATUS stat = NFA_STATUS_FAILED;

  // If polling for Active mode, set the ordering so that we choose Active over Passive mode first.
  if (gNat.tech_mask & (NFA_TECHNOLOGY_MASK_A_ACTIVE | NFA_TECHNOLOGY_MASK_F_ACTIVE)) {
    UINT8  act_mode_order_param[] = { 0x01 };
    SyncEventGuard guard(sNfaSetConfigEvent);

    stat = NFA_SetConfig(NCI_PARAM_ID_ACT_ORDER, sizeof(act_mode_order_param), &act_mode_order_param[0]);
    if (stat == NFA_STATUS_OK)
      sNfaSetConfigEvent.wait();
    else
      ALOGE("%s: NFA_SetConfig fail; error = 0x%X", __FUNCTION__, stat);
  }
}

bool nfcManager_isNfcActive()
{
  return sIsNfaEnabled;
}

bool startStopPolling(bool isStartPolling)
{
  ALOGD("%s: enter; isStart=%u", __FUNCTION__, isStartPolling);
  tNFA_STATUS stat = NFA_STATUS_FAILED;

  startRfDiscovery(false);
  if (isStartPolling) {
    tNFA_TECHNOLOGY_MASK tech_mask = DEFAULT_TECH_MASK;
    unsigned long num = 0;
    if (GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
      tech_mask = num;

    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    ALOGD("%s: enable polling", __FUNCTION__);
    stat = NFA_EnablePolling(tech_mask);
    if (stat == NFA_STATUS_OK) {
      ALOGD("%s: wait for enable event", __FUNCTION__);
      sNfaEnableDisablePollingEvent.wait(); // Wait for NFA_POLL_ENABLED_EVT.
    } else {
      ALOGE ("%s: NFA_EnablePolling fail, error=0x%X", __FUNCTION__, stat);
    }
  } else {
    SyncEventGuard guard(sNfaEnableDisablePollingEvent);
    ALOGD("%s: disable polling", __FUNCTION__);
    stat = NFA_DisablePolling();
    if (stat == NFA_STATUS_OK) {
      sNfaEnableDisablePollingEvent.wait(); // Wait for NFA_POLL_DISABLED_EVT.
    } else {
      ALOGE("%s: NFA_DisablePolling fail, error=0x%X", __FUNCTION__, stat);
    }
  }
  startRfDiscovery(true);
  ALOGD("%s: exit", __FUNCTION__);
  return stat == NFA_STATUS_OK;
}

static bool isPeerToPeer(tNFA_ACTIVATED& activated)
{
  return activated.activate_ntf.protocol == NFA_PROTOCOL_NFC_DEP;
}

static bool isListenMode(tNFA_ACTIVATED& activated)
{
  return ((NFC_DISCOVERY_TYPE_LISTEN_A == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_B == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_F == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_ISO15693 == activated.activate_ntf.rf_tech_param.mode)
      || (NFC_DISCOVERY_TYPE_LISTEN_B_PRIME == activated.activate_ntf.rf_tech_param.mode));
}
