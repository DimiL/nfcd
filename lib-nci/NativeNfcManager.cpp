
#include "NativeNfcManager.h"

#include "NfcAdaptation.h"
#include "SyncEvent.h"
#include "PeerToPeer.h"
#include "PowerSwitch.h"
#include "NfcTag.h"

extern "C"
{
    #include "nfa_api.h"
    #include "nfa_p2p_api.h"
    #include "rw_api.h"
    #include "nfa_ee_api.h"
    #include "nfc_brcm_defs.h"
    #include "ce_api.h"
}

#define LOG_TAG "nfcd"
#include <cutils/log.h>

/*****************************************************************************
**
** public variables and functions
**
*****************************************************************************/
void                    doStartupConfig ();
void                    startRfDiscovery (bool isStart);

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

#define CONFIG_UPDATE_TECH_MASK     (1 << 1)
#define DEFAULT_TECH_MASK           (NFA_TECHNOLOGY_MASK_A \
                                     | NFA_TECHNOLOGY_MASK_B \
                                     | NFA_TECHNOLOGY_MASK_F \
                                     | NFA_TECHNOLOGY_MASK_ISO15693 \
                                     | NFA_TECHNOLOGY_MASK_B_PRIME \
                                     | NFA_TECHNOLOGY_MASK_A_ACTIVE \
                                     | NFA_TECHNOLOGY_MASK_F_ACTIVE \
                                     | NFA_TECHNOLOGY_MASK_KOVIO)


static void nfaConnectionCallback (UINT8 event, tNFA_CONN_EVT_DATA *eventData);
static void nfaDeviceManagementCallback (UINT8 event, tNFA_DM_CBACK_DATA *eventData);

NativeNfcManager::NativeNfcManager() :
  mNativeP2pDevice(NULL),
  mNativeNfcTag(NULL)
{
    initializeNativeStructure();
}

NativeNfcManager::~NativeNfcManager()
{
    // Dimi : TODO, use MACRO
    if (mNativeP2pDevice != NULL)    delete mNativeP2pDevice;
    if (mNativeNfcTag != NULL)       delete mNativeNfcTag;
}

void NativeNfcManager::initializeNativeStructure()
{
    mNativeP2pDevice = new NativeP2pDevice();
    mNativeNfcTag = new NativeNfcTag();
}

void* NativeNfcManager::getNativeStruct(const char* name)
{
    if (0 == strcmp(name, "NativeP2pDevice"))
        return reinterpret_cast<void*>(mNativeP2pDevice);
    else if (0 == strcmp(name, "NativeNfcTag"))
        return reinterpret_cast<void*>(mNativeNfcTag);

    return NULL;
}

bool NativeNfcManager::initialize()
{
    return doInitialize();
}

bool NativeNfcManager::doInitialize()
{
    tNFA_STATUS stat = NFA_STATUS_OK;

    // 1. Initialize PowerSwitch
    PowerSwitch::getInstance ().initialize (PowerSwitch::FULL_POWER);

    // 2. start GKI, NCI task, NFC task
    NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
    theInstance.Initialize();

    {
        unsigned long num = 5;

        SyncEventGuard guard (sNfaEnableEvent);
        tHAL_NFC_ENTRY* halFuncEntries = theInstance.GetHalEntryFuncs ();
        NFA_Init (halFuncEntries);

        stat = NFA_Enable (nfaDeviceManagementCallback, nfaConnectionCallback);
        if (stat == NFA_STATUS_OK)
        {
            //num = initializeGlobalAppLogLevel ();
            CE_SetTraceLevel (num);
            LLCP_SetTraceLevel (num);
            NFC_SetTraceLevel (num);
            RW_SetTraceLevel (num);
            NFA_SetTraceLevel (num);
            NFA_P2pSetTraceLevel (num);
            
            sNfaEnableEvent.wait(); //wait for NFA command to finish
        }
        else 
        {
            ALOGD("NFA Enable Fail");
        }
    }

    if (stat == NFA_STATUS_OK)
    {
        if (sIsNfaEnabled)
        {
            // Dimi : Remove temporarily, implement in the future
            // SecureElement::getInstance().initialize (getNative(e, o));
            //nativeNfcTag_registerNdefTypeHandler ();
            NfcTag::getInstance().initialize (this);

            PeerToPeer::getInstance().initialize (this);
            PeerToPeer::getInstance().handleNfcOnOff (true);

            /////////////////////////////////////////////////////////////////////////////////
            // Add extra configuration here (work-arounds, etc.)

            /*
            struct nfc_jni_native_data *nat = getNative(e, o);

            if ( nat )
            {
                if (GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
                    nat->tech_mask = num;
                else
                    nat->tech_mask = DEFAULT_TECH_MASK;

                ALOGD ("%s: tag polling tech mask=0x%X", __FUNCTION__, nat->tech_mask);
            }
            */

            // if this value exists, set polling interval.
            /*
            if (GetNumValue(NAME_NFA_DM_DISC_DURATION_POLL, &num, sizeof(num)))
                NFA_SetRfDiscoveryDuration(num);
            */

            // Do custom NFCA startup configuration.
            doStartupConfig();
            goto TheEnd;
        }
    }

    if (sIsNfaEnabled)
        stat = NFA_Disable (FALSE /* ungraceful */);

    theInstance.Finalize();

TheEnd:
    if (sIsNfaEnabled) {
        //PowerSwitch::getInstance ().setLevel (PowerSwitch::LOW_POWER);
    }

    return sIsNfaEnabled ? true : false;
}

void NativeNfcManager::enableDiscovery()
{
    tNFA_TECHNOLOGY_MASK tech_mask = DEFAULT_TECH_MASK;
    /*
    struct nfc_jni_native_data *nat = getNative(e, o);

    if (nat)
        tech_mask = (tNFA_TECHNOLOGY_MASK)nat->tech_mask;

    ALOGD ("%s: enter; tech_mask = %02x", __FUNCTION__, tech_mask);
    */

    if (sDiscoveryEnabled)
    {
        ALOGE ("%s: already polling", __FUNCTION__);
        return;
    }

    tNFA_STATUS stat = NFA_STATUS_OK;

    //ALOGD ("%s: sIsSecElemSelected=%u", __FUNCTION__, sIsSecElemSelected);

    PowerSwitch::getInstance ().setLevel (PowerSwitch::FULL_POWER);

    if (sRfEnabled) {
        // Stop RF discovery to reconfigure
        startRfDiscovery(false);
    }
    
    {
        SyncEventGuard guard (sNfaEnableDisablePollingEvent);
        stat = NFA_EnablePolling (tech_mask);
        if (stat == NFA_STATUS_OK)
        {
            ALOGD ("%s: wait for enable event", __FUNCTION__);
            sDiscoveryEnabled = true;
            sNfaEnableDisablePollingEvent.wait (); //wait for NFA_POLL_ENABLED_EVT
            ALOGD ("%s: got enabled event", __FUNCTION__);
        }
        else
        {
            ALOGE ("%s: fail enable discovery; error=0x%X", __FUNCTION__, stat);
        }
    }    

    // Start P2P listening if tag polling was enabled or the mask was 0.
    if (sDiscoveryEnabled || (tech_mask == 0))
    {
        ALOGD ("%s: Enable p2pListening", __FUNCTION__);
        PeerToPeer::getInstance().enableP2pListening (true);

        // Dimi : Remove SE related temporarily
        //if NFC service has deselected the sec elem, then apply default routes
        //if (!sIsSecElemSelected)
        //    stat = SecureElement::getInstance().routeToDefault ();
    }

    // Actually start discovery.
    startRfDiscovery (true);

    PowerSwitch::getInstance ().setModeOn (PowerSwitch::DISCOVERY);

    ALOGD ("%s: exit", __FUNCTION__);
}

/*******************************************************************************
**
** Function:        handleRfDiscoveryEvent
**
** Description:     Handle RF-discovery events from the stack.
**                  discoveredDevice: Discovered device.
**
** Returns:         None
**
*******************************************************************************/
static void handleRfDiscoveryEvent (tNFC_RESULT_DEVT* discoveredDevice)
{
    if (discoveredDevice->more)
    {
        //there is more discovery notification coming
        return;
    }

    bool isP2p = NfcTag::getInstance ().isP2pDiscovered ();
    if (isP2p)
    {
        //select the peer that supports P2P
        NfcTag::getInstance ().selectP2p();
    }
    else
    {
        //select the first of multiple tags that is discovered
        NfcTag::getInstance ().selectFirstTag();
    }
}

/*******************************************************************************
**
** Function:        nfaDeviceManagementCallback
**
** Description:     Receive device management events from stack.
**                  dmEvent: Device-management event ID.
**                  eventData: Data associated with event ID.
**
** Returns:         None
**
*******************************************************************************/
void nfaDeviceManagementCallback (UINT8 dmEvent, tNFA_DM_CBACK_DATA* eventData)
{
    ALOGD("nfaDeviceManagementCallback >>");
    ALOGD("%s: enter; event=0x%X", __FUNCTION__, dmEvent);

    switch (dmEvent)
    {
    case NFA_DM_ENABLE_EVT: /* Result of NFA_Enable */
        {
            SyncEventGuard guard (sNfaEnableEvent);
            ALOGD ("%s: NFA_DM_ENABLE_EVT; status=0x%X",
                    __FUNCTION__, eventData->status);
            sIsNfaEnabled = eventData->status == NFA_STATUS_OK;
            sIsDisabling = false;
            sNfaEnableEvent.notifyOne ();
        }
        break;

    case NFA_DM_DISABLE_EVT: /* Result of NFA_Disable */
        {
            ALOGD ("%s: NFA_DM_DISABLE_EVT", __FUNCTION__);
        }
        break;

    case NFA_DM_SET_CONFIG_EVT: //result of NFA_SetConfig
        ALOGD ("%s: NFA_DM_SET_CONFIG_EVT", __FUNCTION__);
        {
            SyncEventGuard guard (sNfaSetConfigEvent);
            sNfaSetConfigEvent.notifyOne();
        }
        break;

    case NFA_DM_GET_CONFIG_EVT: /* Result of NFA_GetConfig */
        ALOGD ("%s: NFA_DM_GET_CONFIG_EVT", __FUNCTION__);
        break;

    case NFA_DM_RF_FIELD_EVT:
        ALOGD ("%s: NFA_DM_RF_FIELD_EVT; status=0x%X; field status=%u", __FUNCTION__,
              eventData->rf_field.status, eventData->rf_field.rf_field_status);

        if (sIsDisabling || !sIsNfaEnabled)
            break;

        if (!sP2pActive && eventData->rf_field.status == NFA_STATUS_OK)
        {
            // Dimi : Remove SE function temporarily
            /*
            SecureElement::getInstance().notifyRfFieldEvent (
                    eventData->rf_field.rf_field_status == NFA_DM_RF_FIELD_ON);
            */
        }
        break;

    case NFA_DM_NFCC_TRANSPORT_ERR_EVT:
    case NFA_DM_NFCC_TIMEOUT_EVT:
        {
            if (dmEvent == NFA_DM_NFCC_TIMEOUT_EVT)
                ALOGD ("%s: NFA_DM_NFCC_TIMEOUT_EVT; abort all outstanding operations", __FUNCTION__);
            else
                ALOGD ("%s: NFA_DM_NFCC_TRANSPORT_ERR_EVT; abort all outstanding operations", __FUNCTION__);
        }
        break;

    case NFA_DM_PWR_MODE_CHANGE_EVT:
        break;
    default:
        ALOGD ("%s: unhandled event", __FUNCTION__);
        break;
    }
}

/*******************************************************************************
**
** Function:        nfaConnectionCallback
**
** Description:     Receive connection-related events from stack.
**                  connEvent: Event code.
**                  eventData: Event data.
**
** Returns:         None
**
*******************************************************************************/
static void nfaConnectionCallback (UINT8 connEvent, tNFA_CONN_EVT_DATA* eventData)
{
    tNFA_STATUS status = NFA_STATUS_FAILED;
    ALOGD ("%s: enter; event=0x%X", __FUNCTION__, connEvent);

    switch (connEvent)
    {
    case NFA_POLL_ENABLED_EVT: // whether polling successfully started
        {
            ALOGD("%s: NFA_POLL_ENABLED_EVT: status = %u", __FUNCTION__, eventData->status);

            SyncEventGuard guard (sNfaEnableDisablePollingEvent);
            sNfaEnableDisablePollingEvent.notifyOne ();
        }
        break;

    case NFA_POLL_DISABLED_EVT: // Listening/Polling stopped
        {
            ALOGD("%s: NFA_POLL_DISABLED_EVT: status = %u", __FUNCTION__, eventData->status);

            SyncEventGuard guard (sNfaEnableDisablePollingEvent);
            sNfaEnableDisablePollingEvent.notifyOne ();
        }
        break;

    case NFA_RF_DISCOVERY_STARTED_EVT: // RF Discovery started
        {
            ALOGD("%s: NFA_RF_DISCOVERY_STARTED_EVT: status = %u", __FUNCTION__, eventData->status);

            SyncEventGuard guard (sNfaEnableDisablePollingEvent);
            sNfaEnableDisablePollingEvent.notifyOne ();
        }
        break;

    case NFA_RF_DISCOVERY_STOPPED_EVT: // RF Discovery stopped event
        {
            ALOGD("%s: NFA_RF_DISCOVERY_STOPPED_EVT: status = %u", __FUNCTION__, eventData->status);
 
            SyncEventGuard guard (sNfaEnableDisablePollingEvent);
            sNfaEnableDisablePollingEvent.notifyOne ();
        }
        break;

    case NFA_DISC_RESULT_EVT: // NFC link/protocol discovery notificaiton
        status = eventData->disc_result.status;
        ALOGD("%s: NFA_DISC_RESULT_EVT: status = %d", __FUNCTION__, status);
        if (status != NFA_STATUS_OK)
        {
            ALOGE("%s: NFA_DISC_RESULT_EVT error: status = %d", __FUNCTION__, status);
        }
        else
        {
            NfcTag::getInstance().connectionEventHandler(connEvent, eventData);
            handleRfDiscoveryEvent(&eventData->disc_result.discovery_ntf);
        }
        break;

    case NFA_SELECT_RESULT_EVT: // NFC link/protocol discovery select response
        //ALOGD("%s: NFA_SELECT_RESULT_EVT: status = %d, gIsSelectingRfInterface = %d, sIsDisabling=%d", __FUNCTION__, eventData->status, gIsSelectingRfInterface, sIsDisabling);
        break;

    case NFA_DEACTIVATE_FAIL_EVT:
        ALOGD("%s: NFA_DEACTIVATE_FAIL_EVT: status = %d", __FUNCTION__, eventData->status);
        break;

    case NFA_ACTIVATED_EVT: // NFC link/protocol activated
        //ALOGD("%s: NFA_ACTIVATED_EVT: gIsSelectingRfInterface=%d, sIsDisabling=%d", __FUNCTION__, gIsSelectingRfInterface, sIsDisabling);
        break;

    case NFA_DEACTIVATED_EVT: // NFC link/protocol deactivated
        //ALOGD("%s: NFA_DEACTIVATED_EVT   Type: %u, gIsTagDeactivating: %d", __FUNCTION__, eventData->deactivated.type,gIsTagDeactivating);
        break;

    case NFA_TLV_DETECT_EVT: // TLV Detection complete
        status = eventData->tlv_detect.status;
        ALOGD("%s: NFA_TLV_DETECT_EVT: status = %d, protocol = %d, num_tlvs = %d, num_bytes = %d",
             __FUNCTION__, status, eventData->tlv_detect.protocol,
             eventData->tlv_detect.num_tlvs, eventData->tlv_detect.num_bytes);
        if (status != NFA_STATUS_OK)
        {
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
        NfcTag::getInstance().connectionEventHandler (connEvent, eventData);
        // Dimi : To be implement, still think how to modify this part
        /*
        nativeNfcTag_doCheckNdefResult(status,
            eventData->ndef_detect.max_size, eventData->ndef_detect.cur_size,
            eventData->ndef_detect.flags);
        */
        break;

    case NFA_DATA_EVT: // Data message received (for non-NDEF reads)
        ALOGD("%s: NFA_DATA_EVT:  len = %d", __FUNCTION__, eventData->data.len);
        break;

    case NFA_SELECT_CPLT_EVT: // Select completed
        status = eventData->status;
        ALOGD("%s: NFA_SELECT_CPLT_EVT: status = %d", __FUNCTION__, status);
        if (status != NFA_STATUS_OK)
        {
            ALOGE("%s: NFA_SELECT_CPLT_EVT error: status = %d", __FUNCTION__, status);
        }
        break;

    case NFA_READ_CPLT_EVT: // NDEF-read or tag-specific-read completed
        ALOGD("%s: NFA_READ_CPLT_EVT: status = 0x%X", __FUNCTION__, eventData->status);
        break;

    case NFA_WRITE_CPLT_EVT: // Write completed
        ALOGD("%s: NFA_WRITE_CPLT_EVT: status = %d", __FUNCTION__, eventData->status);
        break;

    case NFA_SET_TAG_RO_EVT: // Tag set as Read only
        ALOGD("%s: NFA_SET_TAG_RO_EVT: status = %d", __FUNCTION__, eventData->status);
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

        PeerToPeer::getInstance().llcpActivatedHandler (eventData->llcp_activated);
        break;

    case NFA_LLCP_DEACTIVATED_EVT: // LLCP link is deactivated
        ALOGD("%s: NFA_LLCP_DEACTIVATED_EVT", __FUNCTION__);
        PeerToPeer::getInstance().llcpDeactivatedHandler (eventData->llcp_deactivated);
        break;

    // Dimi : Compile error, to be fixed
    /*
    case NFA_LLCP_FIRST_PACKET_RECEIVED_EVT: // Received first packet over llcp
        ALOGD("%s: NFA_LLCP_FIRST_PACKET_RECEIVED_EVT", __FUNCTION__);
        break;
    */

    case NFA_PRESENCE_CHECK_EVT:
        ALOGD("%s: NFA_PRESENCE_CHECK_EVT", __FUNCTION__);
        break;

    case NFA_FORMAT_CPLT_EVT:
        ALOGD("%s: NFA_FORMAT_CPLT_EVT: status=0x%X", __FUNCTION__, eventData->status);
        break;

    case NFA_I93_CMD_CPLT_EVT:
        ALOGD("%s: NFA_I93_CMD_CPLT_EVT: status=0x%X", __FUNCTION__, eventData->status);
        break;

    case NFA_CE_UICC_LISTEN_CONFIGURED_EVT :
        ALOGD("%s: NFA_CE_UICC_LISTEN_CONFIGURED_EVT : status=0x%X", __FUNCTION__, eventData->status);
        break;

    case NFA_SET_P2P_LISTEN_TECH_EVT:
        ALOGD("%s: NFA_SET_P2P_LISTEN_TECH_EVT", __FUNCTION__);
        PeerToPeer::getInstance().connectionEventHandler (connEvent, eventData);
        break;

    default:
        ALOGE("%s: unknown event ????", __FUNCTION__);
        break;
    }
}

/*******************************************************************************
**
** Function:        startRfDiscovery
**
** Description:     Ask stack to start polling and listening for devices.
**                  isStart: Whether to start.
**
** Returns:         None
**
*******************************************************************************/
void startRfDiscovery(bool isStart)
{
    tNFA_STATUS status = NFA_STATUS_FAILED;

    ALOGD ("%s: is start=%d", __FUNCTION__, isStart);
    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    status  = isStart ? NFA_StartRfDiscovery () : NFA_StopRfDiscovery ();
    if (status == NFA_STATUS_OK)
    {
        sNfaEnableDisablePollingEvent.wait (); //wait for NFA_RF_DISCOVERY_xxxx_EVT
        sRfEnabled = isStart;
    }
    else
    {
        ALOGE ("%s: Failed to start/stop RF discovery; error=0x%X", __FUNCTION__, status);
    }
}

/*******************************************************************************
**
** Function:        doStartupConfig
**
** Description:     Configure the NFC controller.
**
** Returns:         None
**
*******************************************************************************/
void doStartupConfig()
{
    // Dimi : To be fixed, use correct nat
    unsigned long num = 0;
    // struct nfc_jni_native_data *nat = getNative(0, 0);
    tNFA_STATUS stat = NFA_STATUS_FAILED;

    // If polling for Active mode, set the ordering so that we choose Active over Passive mode first.
    // if (nat && (nat->tech_mask & (NFA_TECHNOLOGY_MASK_A_ACTIVE | NFA_TECHNOLOGY_MASK_F_ACTIVE)))
    if (true)
    {
        UINT8  act_mode_order_param[] = { 0x01 };
        SyncEventGuard guard (sNfaSetConfigEvent);
        stat = NFA_SetConfig(NCI_PARAM_ID_ACT_ORDER, sizeof(act_mode_order_param), &act_mode_order_param[0]);
        if (stat == NFA_STATUS_OK)
            sNfaSetConfigEvent.wait ();
    }
}


