#include "NfcManager.h"

#include "OverrideLog.h"
#include "NfcAdaptation.h"
#include "SyncEvent.h"
#include "PeerToPeer.h"
#include "PowerSwitch.h"
#include "NfcTag.h"
#include "config.h"
#include "Pn544Interop.h"
#include "LlcpSocket.h"
#include "LlcpServiceSocket.h"
#include "NfcTagManager.h"

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
#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

extern bool gIsTagDeactivating;
extern bool gIsSelectingRfInterface;
/*****************************************************************************
**
** public variables and functions
**
*****************************************************************************/
nfc_data                gNat;
int                     gGeneralTransceiveTimeout = 1000;
void                    doStartupConfig();
void                    startRfDiscovery(bool isStart);

/*****************************************************************************
**
** private variables and functions
**
*****************************************************************************/
static SyncEvent            sNfaEnableEvent;  //event for NFA_Enable()
static SyncEvent            sNfaDisableEvent;  //event for NFA_Disable()
static SyncEvent            sNfaEnableDisablePollingEvent;  //event for NFA_EnablePolling(), NFA_DisablePolling()
static SyncEvent            sNfaSetConfigEvent;  // event for Set_Config....
static SyncEvent            sNfaGetConfigEvent;  // event for Get_Config....

static bool                 sIsNfaEnabled = false;
static bool                 sDiscoveryEnabled = false;  //is polling for tag?
static bool                 sIsDisabling = false;
static bool                 sRfEnabled = false; // whether RF discovery is enabled
static bool                 sP2pActive = false; // whether p2p was last active
static bool                 sAbortConnlessWait = false;

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
/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

NfcManager::NfcManager()
 : mP2pDevice(NULL)
 , mNfcTagManager(NULL)
{
  mP2pDevice = new P2pDevice();
  mNfcTagManager = new NfcTagManager();
}

NfcManager::~NfcManager()
{
  if (mP2pDevice != NULL)    delete mP2pDevice;
  if (mNfcTagManager != NULL)       delete mNfcTagManager;
}

void* NfcManager::queryInterface(const char* name)
{
  if (0 == strcmp(name, "P2pDevice"))
    return reinterpret_cast<void*>(mP2pDevice);
  else if (0 == strcmp(name, "NfcTagManager"))
    return reinterpret_cast<void*>(mNfcTagManager);

  return NULL;
}

bool NfcManager::doInitialize()
{
  tNFA_STATUS stat = NFA_STATUS_OK;
  unsigned long num = 5;

  // 1. Initialize PowerSwitch
  PowerSwitch::getInstance().initialize(PowerSwitch::FULL_POWER);

  // 2. start GKI, NCI task, NFC task
  NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
  theInstance.Initialize();

  {
    SyncEventGuard guard (sNfaEnableEvent);
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
            
      sNfaEnableEvent.wait(); //wait for NFA command to finish
    } else {
      ALOGE("%s: NFA Enable Fail", __FUNCTION__);
    }
  }

  if (stat == NFA_STATUS_OK) {
    if (sIsNfaEnabled) {
      // TODO : Implement SE
      NfcTagManager::doRegisterNdefTypeHandler();
      NfcTag::getInstance().initialize(this);

      PeerToPeer::getInstance().initialize(this);
      PeerToPeer::getInstance().handleNfcOnOff(true);

      /////////////////////////////////////////////////////////////////////////////////
      // Add extra configuration here (work-arounds, etc.)
      {
        if (GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
          gNat.tech_mask = num;
        else
          gNat.tech_mask = DEFAULT_TECH_MASK;

        ALOGD("%s: tag polling tech mask=0x%X", __FUNCTION__, gNat.tech_mask);
      }

      // if this value exists, set polling interval.
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

  return sIsNfaEnabled ? true : false;
}

bool NfcManager::doDeinitialize()
{
  ALOGD("%s: enter", __FUNCTION__);

  sIsDisabling = true;
  pn544InteropAbortNow();
  // TODO : Implement SE
  //SecureElement::getInstance().finalize();

  if (sIsNfaEnabled) {
    SyncEventGuard guard (sNfaDisableEvent);
    tNFA_STATUS stat = NFA_Disable(TRUE /* graceful */);
    if (stat == NFA_STATUS_OK) {
      ALOGD("%s: wait for completion", __FUNCTION__);
      sNfaDisableEvent.wait(); //wait for NFA command to finish
      PeerToPeer::getInstance().handleNfcOnOff(false);
    } else {
      ALOGE("%s: fail disable; error=0x%X", __FUNCTION__, stat);
    }
  }

  NfcTagManager::doAbortWaits();
  NfcTag::getInstance().abort();
  sAbortConnlessWait = true;
  // TODO : Implement LLCP
  sIsNfaEnabled = false;
  sDiscoveryEnabled = false;
  sIsDisabling = false;
  // TODO : Implement SE
  // sIsSecElemSelected = false;

  {
    //unblock NFA_EnablePolling() and NFA_DisablePolling()
    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    sNfaEnableDisablePollingEvent.notifyOne();
  }

  NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
  theInstance.Finalize();

  ALOGD("%s: exit", __FUNCTION__);
  return true;
}

void NfcManager::enableDiscovery()
{
  tNFA_TECHNOLOGY_MASK tech_mask = DEFAULT_TECH_MASK;

  tech_mask = (tNFA_TECHNOLOGY_MASK)gNat.tech_mask;

  if (sDiscoveryEnabled) {
    ALOGE("%s: already polling", __FUNCTION__);
    return;
  }

  tNFA_STATUS stat = NFA_STATUS_OK;

  //ALOGD("%s: sIsSecElemSelected=%u", __FUNCTION__, sIsSecElemSelected);

  PowerSwitch::getInstance().setLevel(PowerSwitch::FULL_POWER);

  if (sRfEnabled) {
    // Stop RF discovery to reconfigure
    startRfDiscovery(false);
  }
    
  {
    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    stat = NFA_EnablePolling(tech_mask);
    if (stat == NFA_STATUS_OK) {
      ALOGD("%s: wait for enable event", __FUNCTION__);
      sDiscoveryEnabled = true;
      sNfaEnableDisablePollingEvent.wait(); //wait for NFA_POLL_ENABLED_EVT
      ALOGD("%s: got enabled event", __FUNCTION__);
    } else {
      ALOGE("%s: fail enable discovery; error=0x%X", __FUNCTION__, stat);
    }
  }    

  // Start P2P listening if tag polling was enabled or the mask was 0.
  if (sDiscoveryEnabled || (tech_mask == 0)) {
    ALOGD("%s: Enable p2pListening", __FUNCTION__);
    PeerToPeer::getInstance().enableP2pListening(true);

    // TODO : Implement SE
  }

  // Actually start discovery.
  startRfDiscovery(true);

  PowerSwitch::getInstance().setModeOn(PowerSwitch::DISCOVERY);

  ALOGD("%s: exit", __FUNCTION__);
}

void NfcManager::disableDiscovery()
{
  tNFA_STATUS status = NFA_STATUS_OK;
  ALOGD("%s: enter;", __FUNCTION__);

  pn544InteropAbortNow();
  if (sDiscoveryEnabled == false) {
    ALOGD("%s: already disabled", __FUNCTION__);
    goto TheEnd;
  }

  // Stop RF Discovery.
  startRfDiscovery(false);

  if (sDiscoveryEnabled) {
    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    status = NFA_DisablePolling();
    if (status == NFA_STATUS_OK) {
      sDiscoveryEnabled = false;
      sNfaEnableDisablePollingEvent.wait(); //wait for NFA_POLL_DISABLED_EVT
    } else {
      ALOGE("%s: Failed to disable polling; error=0x%X", __FUNCTION__, status);
    }
  }

  PeerToPeer::getInstance().enableP2pListening(false);

  //if nothing is active after this, then tell the controller to power down
  if (! PowerSwitch::getInstance().setModeOff(PowerSwitch::DISCOVERY))
    PowerSwitch::getInstance().setLevel(PowerSwitch::LOW_POWER);

  // We may have had RF field notifications that did not cause
  // any activate/deactive events. For example, caused by wireless
  // charging orbs. Those may cause us to go to sleep while the last
  // field event was indicating a field. To prevent sticking in that
  // state, always reset the rf field status when we disable discovery.
  // TODO : Implement SE
  // SecureElement::getInstance().resetRfFieldStatus();
TheEnd:
  ALOGD("%s: exit", __FUNCTION__);
}

bool NfcManager::doCheckLlcp()
{
  ALOGD("%s: enter;", __FUNCTION__);
  return true;
}

bool NfcManager::doActivateLlcp()
{
  ALOGD("%s: enter;", __FUNCTION__);
  return true;
}

ILlcpSocket* NfcManager::createLlcpSocket(int sap, int miu, int rw, int linearBufferLength)
{
  ALOGD("%s: enter; sap=%d; miu=%d; rw=%d; buffer len=%d", __FUNCTION__, sap, miu, rw, linearBufferLength);

  unsigned int handle = PeerToPeer::getInstance().getNewHandle();
  bool stat = PeerToPeer::getInstance().createClient(handle, miu, rw);

  LlcpSocket* pLlcpSocket = new LlcpSocket(handle, sap, miu, rw);
    
  ALOGD("%s: exit", __FUNCTION__); 
  return static_cast<ILlcpSocket*>(pLlcpSocket);
}

ILlcpServerSocket* NfcManager::createLlcpServerSocket(int sap, const char* sn, int miu, int rw, int linearBufferLength)
{
  ALOGD("%s: enter; sap=%d; sn =%s; miu=%d; rw=%d; buffer len=%d", __FUNCTION__, sap, sn, miu, rw, linearBufferLength);
  unsigned int handle = PeerToPeer::getInstance().getNewHandle();
  LlcpServiceSocket* pLlcpServiceSocket = new LlcpServiceSocket(handle, linearBufferLength, miu, rw);

  if (!PeerToPeer::getInstance().registerServer(handle, sn)) {
    ALOGE("%s: RegisterServer error", __FUNCTION__);
    return NULL;
  }
    
  ALOGD("%s: exit", __FUNCTION__);
  return static_cast<ILlcpServerSocket*>(pLlcpServiceSocket);
}

void NfcManager::setP2pInitiatorModes(int modes)
{

}

void NfcManager::setP2pTargetModes(int modes)
{
  ALOGD("%s: modes=0x%X", __FUNCTION__, modes);
  // Map in the right modes
  tNFA_TECHNOLOGY_MASK mask = 0;
  if (modes & 0x01) mask |= NFA_TECHNOLOGY_MASK_A;
  if (modes & 0x02) mask |= NFA_TECHNOLOGY_MASK_F;
  if (modes & 0x04) mask |= NFA_TECHNOLOGY_MASK_F;
  if (modes & 0x08) mask |= NFA_TECHNOLOGY_MASK_A_ACTIVE | NFA_TECHNOLOGY_MASK_F_ACTIVE;

  PeerToPeer::getInstance().setP2pListenMask(mask); 
  //this function is not called by the NFC service nor exposed by public API.
}

int NfcManager::getDefaultLlcpMiu()
{
  return NfcManager::DEFAULT_LLCP_MIU;
}

int NfcManager::getDefaultLlcpRwSize()
{
  return NfcManager::DEFAULT_LLCP_RWSIZE;
}

static void handleRfDiscoveryEvent(tNFC_RESULT_DEVT* discoveredDevice)
{
  if (discoveredDevice->more) {
    //there is more discovery notification coming
    return;
  }

  bool isP2p = NfcTag::getInstance().isP2pDiscovered();
  if (isP2p) {
    //select the peer that supports P2P
    NfcTag::getInstance().selectP2p();
  } else {
    //select the first of multiple tags that is discovered
    NfcTag::getInstance().selectFirstTag();
  }
}

void nfaDeviceManagementCallback(UINT8 dmEvent, tNFA_DM_CBACK_DATA* eventData)
{
  ALOGD("%s: enter; event=0x%X", __FUNCTION__, dmEvent);

  switch (dmEvent)
  {
  case NFA_DM_ENABLE_EVT: /* Result of NFA_Enable */
    {
      SyncEventGuard guard (sNfaEnableEvent);
      ALOGD("%s: NFA_DM_ENABLE_EVT; status=0x%X",__FUNCTION__, eventData->status);
      sIsNfaEnabled = eventData->status == NFA_STATUS_OK;
      sIsDisabling = false;
      sNfaEnableEvent.notifyOne();
    }
    break;

  case NFA_DM_DISABLE_EVT: /* Result of NFA_Disable */
    {
      SyncEventGuard guard (sNfaDisableEvent);
      ALOGD("%s: NFA_DM_DISABLE_EVT", __FUNCTION__);
      sIsNfaEnabled = false;
      sIsDisabling = false;
      sNfaDisableEvent.notifyOne();
    }
    break;

  case NFA_DM_SET_CONFIG_EVT: //result of NFA_SetConfig
    ALOGD("%s: NFA_DM_SET_CONFIG_EVT", __FUNCTION__);
    {
      SyncEventGuard guard (sNfaSetConfigEvent);
      sNfaSetConfigEvent.notifyOne();
    }
    break;

  case NFA_DM_GET_CONFIG_EVT: /* Result of NFA_GetConfig */
    ALOGD("%s: NFA_DM_GET_CONFIG_EVT", __FUNCTION__);
    {
      SyncEventGuard guard (sNfaGetConfigEvent);
      if (eventData->status == NFA_STATUS_OK &&
          eventData->get_config.tlv_size <= sizeof(sConfig)) {
        sCurrentConfigLen = eventData->get_config.tlv_size;
        memcpy(sConfig, eventData->get_config.param_tlvs, eventData->get_config.tlv_size);
      } else {
        ALOGE("%s: NFA_DM_GET_CONFIG failed", __FUNCTION__);
        sCurrentConfigLen = 0;
      }
      sNfaGetConfigEvent.notifyOne();
    }
    break;

  case NFA_DM_RF_FIELD_EVT:
    ALOGD("%s: NFA_DM_RF_FIELD_EVT; status=0x%X; field status=%u", __FUNCTION__,
            eventData->rf_field.status, eventData->rf_field.rf_field_status);

    if (sIsDisabling || !sIsNfaEnabled)
      break;

    if (!sP2pActive && eventData->rf_field.status == NFA_STATUS_OK) {
      // TODO : Implement SE
    }
    break;

  case NFA_DM_NFCC_TRANSPORT_ERR_EVT:
  case NFA_DM_NFCC_TIMEOUT_EVT:
    {
      if (dmEvent == NFA_DM_NFCC_TIMEOUT_EVT)
        ALOGD("%s: NFA_DM_NFCC_TIMEOUT_EVT; abort all outstanding operations", __FUNCTION__);
      else
        ALOGD("%s: NFA_DM_NFCC_TRANSPORT_ERR_EVT; abort all outstanding operations", __FUNCTION__);

      NfcTagManager::doAbortWaits();
      NfcTag::getInstance().abort();
      sAbortConnlessWait = true;
      // TODO : Implement LLCP
      {
        ALOGD("%s: aborting  sNfaEnableDisablePollingEvent", __FUNCTION__);
        SyncEventGuard guard (sNfaEnableDisablePollingEvent);
        sNfaEnableDisablePollingEvent.notifyOne();
      }
      {
        ALOGD("%s: aborting  sNfaEnableEvent", __FUNCTION__);
        SyncEventGuard guard (sNfaEnableEvent);
        sNfaEnableEvent.notifyOne();
      }
      {
        ALOGD("%s: aborting  sNfaDisableEvent", __FUNCTION__);
        SyncEventGuard guard (sNfaDisableEvent);
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

  switch (connEvent)
  {
  case NFA_POLL_ENABLED_EVT: // whether polling successfully started
    {
      ALOGD("%s: NFA_POLL_ENABLED_EVT: status = %u", __FUNCTION__, eventData->status);

      SyncEventGuard guard (sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.notifyOne();
    }
    break;

  case NFA_POLL_DISABLED_EVT: // Listening/Polling stopped
    {
      ALOGD("%s: NFA_POLL_DISABLED_EVT: status = %u", __FUNCTION__, eventData->status);

      SyncEventGuard guard (sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.notifyOne();
    }
    break;

  case NFA_RF_DISCOVERY_STARTED_EVT: // RF Discovery started
    {
      ALOGD("%s: NFA_RF_DISCOVERY_STARTED_EVT: status = %u", __FUNCTION__, eventData->status);

      SyncEventGuard guard (sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.notifyOne();
    }
    break;

  case NFA_RF_DISCOVERY_STOPPED_EVT: // RF Discovery stopped event
    {
      ALOGD("%s: NFA_RF_DISCOVERY_STOPPED_EVT: status = %u", __FUNCTION__, eventData->status);
 
      SyncEventGuard guard (sNfaEnableDisablePollingEvent);
      sNfaEnableDisablePollingEvent.notifyOne();
    }
    break;

  case NFA_DISC_RESULT_EVT: // NFC link/protocol discovery notificaiton
    status = eventData->disc_result.status;
    ALOGD("%s: NFA_DISC_RESULT_EVT: status = %d", __FUNCTION__, status);
    if (status != NFA_STATUS_OK) {
      ALOGE("%s: NFA_DISC_RESULT_EVT error: status = %d", __FUNCTION__, status);
    } else {
      NfcTag::getInstance().connectionEventHandler(connEvent, eventData);
      handleRfDiscoveryEvent(&eventData->disc_result.discovery_ntf);
    }
    break;

  case NFA_SELECT_RESULT_EVT: // NFC link/protocol discovery select response
    ALOGD("%s: NFA_SELECT_RESULT_EVT: status = %d, gIsSelectingRfInterface = %d, sIsDisabling=%d", __FUNCTION__, eventData->status, gIsSelectingRfInterface, sIsDisabling);
    break;

  case NFA_DEACTIVATE_FAIL_EVT:
    ALOGD("%s: NFA_DEACTIVATE_FAIL_EVT: status = %d", __FUNCTION__, eventData->status);
    break;

  case NFA_ACTIVATED_EVT: // NFC link/protocol activated
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
      // Disable RF field events in case of p2p
      UINT8  nfa_disable_rf_events[] = { 0x00 };
      ALOGD("%s: Disabling RF field events", __FUNCTION__);
      status = NFA_SetConfig(NCI_PARAM_ID_RF_FIELD_INFO, sizeof(nfa_disable_rf_events),
                 &nfa_disable_rf_events[0]);
      if (status == NFA_STATUS_OK) {
        ALOGD("%s: Disabled RF field events", __FUNCTION__);
      } else {
        ALOGE("%s: Failed to disable RF field events", __FUNCTION__);
      }
      // For the SE, consider the field to be on while p2p is active.
      // TODO : Implement SE
    } else if (pn544InteropIsBusy() == false) {
      NfcTag::getInstance().connectionEventHandler(connEvent, eventData);

      // We know it is not activating for P2P.  If it activated in
      // listen mode then it is likely for an SE transaction.
      // Send the RF Event.
      if (isListenMode(eventData->activated)) {
        // TODO : Implement SE
      }
    }

    break;

  case NFA_DEACTIVATED_EVT: // NFC link/protocol deactivated
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
    if ((eventData->deactivated.type == NFA_DEACTIVATE_TYPE_IDLE)
     || (eventData->deactivated.type == NFA_DEACTIVATE_TYPE_DISCOVERY)) {
      // TODO : Implement SE
    }

    break;

  case NFA_TLV_DETECT_EVT: // TLV Detection complete
    status = eventData->tlv_detect.status;
    ALOGD("%s: NFA_TLV_DETECT_EVT: status = %d, protocol = %d, num_tlvs = %d, num_bytes = %d",
      __FUNCTION__, status, eventData->tlv_detect.protocol,
      eventData->tlv_detect.num_tlvs, eventData->tlv_detect.num_bytes);
    if (status != NFA_STATUS_OK) {
      ALOGE("%s: NFA_TLV_DETECT_EVT error: status = %d", __FUNCTION__, status);
    }
    break;

  case NFA_NDEF_DETECT_EVT: // NDEF Detection complete;
    //if status is failure, it means the tag does not contain any or valid NDEF data;
    //pass the failure status to the NFC Service;
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

  case NFA_DATA_EVT: // Data message received (for non-NDEF reads)
    ALOGD("%s: NFA_DATA_EVT:  len = %d", __FUNCTION__, eventData->data.len);
    break;

  case NFA_SELECT_CPLT_EVT: // Select completed
    status = eventData->status;
    ALOGD("%s: NFA_SELECT_CPLT_EVT: status = %d", __FUNCTION__, status);
    if (status != NFA_STATUS_OK) {
      ALOGE("%s: NFA_SELECT_CPLT_EVT error: status = %d", __FUNCTION__, status);
    }
    break;

  case NFA_READ_CPLT_EVT: // NDEF-read or tag-specific-read completed
    ALOGD("%s: NFA_READ_CPLT_EVT: status = 0x%X", __FUNCTION__, eventData->status);
    NfcTagManager::doReadCompleted(eventData->status);
    NfcTag::getInstance().connectionEventHandler(connEvent, eventData);
    break;

  case NFA_WRITE_CPLT_EVT: // Write completed
    ALOGD("%s: NFA_WRITE_CPLT_EVT: status = %d", __FUNCTION__, eventData->status);
    NfcTagManager::doWriteStatus(eventData->status == NFA_STATUS_OK);
    break;

  case NFA_SET_TAG_RO_EVT: // Tag set as Read only
    ALOGD("%s: NFA_SET_TAG_RO_EVT: status = %d", __FUNCTION__, eventData->status);
    NfcTagManager::doMakeReadonlyResult(eventData->status);
    break;

  case NFA_CE_NDEF_WRITE_START_EVT: // NDEF write started
    ALOGD("%s: NFA_CE_NDEF_WRITE_START_EVT: status: %d", __FUNCTION__, eventData->status);

    if (eventData->status != NFA_STATUS_OK)
      ALOGE("%s: NFA_CE_NDEF_WRITE_START_EVT error: status = %d", __FUNCTION__, eventData->status);
    break;

  case NFA_CE_NDEF_WRITE_CPLT_EVT: // NDEF write completed
    ALOGD("%s: FA_CE_NDEF_WRITE_CPLT_EVT: len = %lu", __FUNCTION__, eventData->ndef_write_cplt.len);
    break;

  case NFA_LLCP_ACTIVATED_EVT: // LLCP link is activated
    ALOGD("%s: NFA_LLCP_ACTIVATED_EVT: is_initiator: %d  remote_wks: %d, remote_lsc: %d, remote_link_miu: %d, local_link_miu: %d",
           __FUNCTION__,
           eventData->llcp_activated.is_initiator,
           eventData->llcp_activated.remote_wks,
           eventData->llcp_activated.remote_lsc,
           eventData->llcp_activated.remote_link_miu,
           eventData->llcp_activated.local_link_miu);

    PeerToPeer::getInstance().llcpActivatedHandler(eventData->llcp_activated);
    break;

  case NFA_LLCP_DEACTIVATED_EVT: // LLCP link is deactivated
    ALOGD("%s: NFA_LLCP_DEACTIVATED_EVT", __FUNCTION__);
    PeerToPeer::getInstance().llcpDeactivatedHandler(eventData->llcp_deactivated);
    break;

  case NFA_LLCP_FIRST_PACKET_RECEIVED_EVT: // Received first packet over llcp
    ALOGD("%s: NFA_LLCP_FIRST_PACKET_RECEIVED_EVT", __FUNCTION__);
    PeerToPeer::getInstance().llcpFirstPacketHandler();
    break;

  case NFA_PRESENCE_CHECK_EVT:
    ALOGD("%s: NFA_PRESENCE_CHECK_EVT", __FUNCTION__);
    NfcTagManager::doPresenceCheckResult(eventData->status);
    break;

  case NFA_FORMAT_CPLT_EVT:
    ALOGD("%s: NFA_FORMAT_CPLT_EVT: status=0x%X", __FUNCTION__, eventData->status);
    ALOGE("%s: Unimplement function", __FUNCTION__);
    break;

  case NFA_I93_CMD_CPLT_EVT:
    ALOGD("%s: NFA_I93_CMD_CPLT_EVT: status=0x%X", __FUNCTION__, eventData->status);
    break;

  case NFA_CE_UICC_LISTEN_CONFIGURED_EVT :
    ALOGD("%s: NFA_CE_UICC_LISTEN_CONFIGURED_EVT : status=0x%X", __FUNCTION__, eventData->status);
    // TODO : Implement SE
    ALOGE("%s: Unimplement function", __FUNCTION__);
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
  SyncEventGuard guard (sNfaEnableDisablePollingEvent);
  status  = isStart ? NFA_StartRfDiscovery() : NFA_StopRfDiscovery();
  if (status == NFA_STATUS_OK) {
    sNfaEnableDisablePollingEvent.wait(); //wait for NFA_RF_DISCOVERY_xxxx_EVT
    sRfEnabled = isStart;
  } else {
    ALOGE("%s: Failed to start/stop RF discovery; error=0x%X", __FUNCTION__, status);
  }
}

void doStartupConfig()
{
  unsigned long num = 0;
  tNFA_STATUS stat = NFA_STATUS_FAILED;

  // If polling for Active mode, set the ordering so that we choose Active over Passive mode first.
  if (gNat.tech_mask & (NFA_TECHNOLOGY_MASK_A_ACTIVE | NFA_TECHNOLOGY_MASK_F_ACTIVE)) {
    UINT8  act_mode_order_param[] = { 0x01 };
    SyncEventGuard guard (sNfaSetConfigEvent);
    stat = NFA_SetConfig(NCI_PARAM_ID_ACT_ORDER, sizeof(act_mode_order_param), &act_mode_order_param[0]);
    if (stat == NFA_STATUS_OK)
      sNfaSetConfigEvent.wait();
  }
}

bool nfcManager_isNfcActive()
{
  return sIsNfaEnabled;
}

void startStopPolling(bool isStartPolling)
{
  ALOGD("%s: enter; isStart=%u", __FUNCTION__, isStartPolling);
  tNFA_STATUS stat = NFA_STATUS_FAILED;

  startRfDiscovery(false);
  if (isStartPolling) {
    tNFA_TECHNOLOGY_MASK tech_mask = DEFAULT_TECH_MASK;
    unsigned long num = 0;
    if (GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
      tech_mask = num;

    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    ALOGD("%s: enable polling", __FUNCTION__);
    stat = NFA_EnablePolling(tech_mask);
    if (stat == NFA_STATUS_OK) {
      ALOGD("%s: wait for enable event", __FUNCTION__);
      sNfaEnableDisablePollingEvent.wait(); //wait for NFA_POLL_ENABLED_EVT
    } else {
      ALOGE ("%s: fail enable polling; error=0x%X", __FUNCTION__, stat);
    }
  } else {
    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    ALOGD("%s: disable polling", __FUNCTION__);
    stat = NFA_DisablePolling();
    if (stat == NFA_STATUS_OK) {
      sNfaEnableDisablePollingEvent.wait(); //wait for NFA_POLL_DISABLED_EVT
    } else {
      ALOGE("%s: fail disable polling; error=0x%X", __FUNCTION__, stat);
    }
  }
  startRfDiscovery(true);
  ALOGD("%s: exit", __FUNCTION__);
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
