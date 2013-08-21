#include "NativeNfcTag.h"

#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include "OverrideLog.h"
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

/*****************************************************************************
**
** public variables and functions
**
*****************************************************************************/
bool    gIsTagDeactivating = false;    // flag for nfa callback indicating we are deactivating for RF interface switch
bool    gIsSelectingRfInterface = false; // flag for nfa callback indicating we are selecting for RF interface switch

/*****************************************************************************
**
** private variables and functions
**
*****************************************************************************/
// Pre-defined tag type values. These must match the values in
// framework Ndef.java for Google public NFC API.
#define NDEF_UNKNOWN_TYPE          -1
#define NDEF_TYPE1_TAG             1
#define NDEF_TYPE2_TAG             2
#define NDEF_TYPE3_TAG             3
#define NDEF_TYPE4_TAG             4
#define NDEF_MIFARE_CLASSIC_TAG    101

#define STATUS_CODE_TARGET_LOST    146  // this error code comes from the service

static uint32_t     sCheckNdefCurrentSize = 0;
static tNFA_STATUS  sCheckNdefStatus = 0; //whether tag already contains a NDEF message
static bool         sCheckNdefCapable = false; //whether tag has NDEF capability
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
static IntervalTimer sSwitchBackTimer; // timer used to tell us to switch back to ISO_DEP frame interface
static bool     	sWriteOk = false;
static bool     	sWriteWaitingForComplete = false;
static bool         sFormatOk = false;
static bool     	sConnectOk = false;
static bool     	sConnectWaitingForComplete = false;
static bool         sGotDeactivate = false;
static uint32_t     sCheckNdefMaxSize = 0;
static bool         sCheckNdefCardReadOnly = false;
static bool     	sCheckNdefWaitingForComplete = false;
static int          sCountTagAway = 0; //count the consecutive number of presence-check failures
static tNFA_STATUS  sMakeReadonlyStatus = NFA_STATUS_FAILED;
static bool     	sMakeReadonlyWaitingForComplete = false;

/*******************************************************************************
**
** Function:        ndefHandlerCallback
**
** Description:     Receive NDEF-message related events from stack.
**                  event: Event code.
**                  p_data: Event data.
**
** Returns:         None
**
*******************************************************************************/
static void ndefHandlerCallback (tNFA_NDEF_EVT event, tNFA_NDEF_EVT_DATA *eventData)
{
    ALOGD ("%s: event=%u, eventData=%p", __FUNCTION__, event, eventData);

    switch (event)
    {
    case NFA_NDEF_REGISTER_EVT:
        {
            tNFA_NDEF_REGISTER& ndef_reg = eventData->ndef_reg;
            ALOGD ("%s: NFA_NDEF_REGISTER_EVT; status=0x%X; h=0x%X", __FUNCTION__, ndef_reg.status, ndef_reg.ndef_type_handle);
            sNdefTypeHandlerHandle = ndef_reg.ndef_type_handle;
        }
        break;

    case NFA_NDEF_DATA_EVT:
        {
            ALOGD ("%s: NFA_NDEF_DATA_EVT; data_len = %lu", __FUNCTION__, eventData->ndef_data.len);
            sReadDataLen = eventData->ndef_data.len;
            sReadData = (uint8_t*) malloc (sReadDataLen);
            memcpy (sReadData, eventData->ndef_data.p_data, eventData->ndef_data.len);
        }
        break;

    default:
        ALOGE ("%s: Unknown event %u ????", __FUNCTION__, event);
        break;
    }
}

NativeNfcTag::NativeNfcTag()
{
}

NativeNfcTag::~NativeNfcTag()
{
}

/*******************************************************************************
**
** Function:        nativeNfcTag_abortWaits
**
** Description:     Unblock all thread synchronization objects.
**
** Returns:         None
**
*******************************************************************************/
void NativeNfcTag::nativeNfcTag_abortWaits ()
{
    ALOGD ("%s", __FUNCTION__);
    {
        SyncEventGuard g (sReadEvent);
        sReadEvent.notifyOne ();
    }
    sem_post (&sWriteSem);
    sem_post (&sFormatSem);
    {
        SyncEventGuard g (sTransceiveEvent);
        sTransceiveEvent.notifyOne ();
    }
    {
        SyncEventGuard g (sReconnectEvent);
        sReconnectEvent.notifyOne ();
    }

    sem_post (&sCheckNdefSem);
    sem_post (&sPresenceCheckSem);
    sem_post (&sMakeReadonlySem);
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doConnectStatus
**
** Description:     Receive the completion status of connect operation.
**                  isConnectOk: Status of the operation.
**
** Returns:         None
**
*******************************************************************************/
void NativeNfcTag::nativeNfcTag_doConnectStatus (bool isConnectOk)
{
    if (sConnectWaitingForComplete != false)
    {
        sConnectWaitingForComplete = false;
        sConnectOk = isConnectOk;
        SyncEventGuard g (sReconnectEvent);
        sReconnectEvent.notifyOne ();
    }
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doDeactivateStatus
**
** Description:     Receive the completion status of deactivate operation.
**
** Returns:         None
**
*******************************************************************************/
void NativeNfcTag::nativeNfcTag_doDeactivateStatus (int status)
{
    sGotDeactivate = (status == 0);

    SyncEventGuard g (sReconnectEvent);
    sReconnectEvent.notifyOne ();
}

/*******************************************************************************
**
** Function:        nativeNfcTag_resetPresenceCheck
**
** Description:     Reset variables related to presence-check.
**
** Returns:         None
**
*******************************************************************************/
void NativeNfcTag::nativeNfcTag_resetPresenceCheck ()
{
    sCountTagAway = 0;
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doCheckNdefResult
**
** Description:     Receive the result of checking whether the tag contains a NDEF
**                  message.  Called by the NFA_NDEF_DETECT_EVT.
**                  status: Status of the operation.
**                  maxSize: Maximum size of NDEF message.
**                  currentSize: Current size of NDEF message.
**                  flags: Indicate various states.
**
** Returns:         None
**
*******************************************************************************/
void NativeNfcTag::nativeNfcTag_doCheckNdefResult (tNFA_STATUS status, uint32_t maxSize, uint32_t currentSize, uint8_t flags)
{
    //this function's flags parameter is defined using the following macros
    //in nfc/include/rw_api.h;
    //#define RW_NDEF_FL_READ_ONLY  0x01    /* Tag is read only              */
    //#define RW_NDEF_FL_FORMATED   0x02    /* Tag formated for NDEF         */
    //#define RW_NDEF_FL_SUPPORTED  0x04    /* NDEF supported by the tag     */
    //#define RW_NDEF_FL_UNKNOWN    0x08    /* Unable to find if tag is ndef capable/formated/read only */
    //#define RW_NDEF_FL_FORMATABLE 0x10    /* Tag supports format operation */

    if (status == NFC_STATUS_BUSY)
    {
        ALOGE ("%s: stack is busy", __FUNCTION__);
        return;
    }

    if (!sCheckNdefWaitingForComplete)
    {
        ALOGE ("%s: not waiting", __FUNCTION__);
        return;
    }

    if (flags & RW_NDEF_FL_READ_ONLY)
        ALOGD ("%s: flag read-only", __FUNCTION__);
    if (flags & RW_NDEF_FL_FORMATED)
        ALOGD ("%s: flag formatted for ndef", __FUNCTION__);
    if (flags & RW_NDEF_FL_SUPPORTED)
        ALOGD ("%s: flag ndef supported", __FUNCTION__);
    if (flags & RW_NDEF_FL_UNKNOWN)
        ALOGD ("%s: flag all unknown", __FUNCTION__);
    if (flags & RW_NDEF_FL_FORMATABLE)
        ALOGD ("%s: flag formattable", __FUNCTION__);

    sCheckNdefWaitingForComplete = false;
    sCheckNdefStatus = status;
    sCheckNdefCapable = false; //assume tag is NOT ndef capable
    if (sCheckNdefStatus == NFA_STATUS_OK)
    {
        //NDEF content is on the tag
        sCheckNdefMaxSize = maxSize;
        sCheckNdefCurrentSize = currentSize;
        sCheckNdefCardReadOnly = flags & RW_NDEF_FL_READ_ONLY;
        sCheckNdefCapable = true;
    }
    else if (sCheckNdefStatus == NFA_STATUS_FAILED)
    {
        //no NDEF content on the tag
        sCheckNdefMaxSize = 0;
        sCheckNdefCurrentSize = 0;
        sCheckNdefCardReadOnly = flags & RW_NDEF_FL_READ_ONLY;
        if ((flags & RW_NDEF_FL_UNKNOWN) == 0) //if stack understands the tag
        {
            if (flags & RW_NDEF_FL_SUPPORTED) //if tag is ndef capable
                sCheckNdefCapable = true;
        }
    }
    else
    {
        ALOGE ("%s: unknown status=0x%X", __FUNCTION__, status);
        sCheckNdefMaxSize = 0;
        sCheckNdefCurrentSize = 0;
        sCheckNdefCardReadOnly = false;
    }
    sem_post (&sCheckNdefSem);
}


/*******************************************************************************
**
** Function:        nativeNfcTag_registerNdefTypeHandler
**
** Description:     Register a callback to receive NDEF message from the tag
**                  from the NFA_NDEF_DATA_EVT.
**
** Returns:         None
**
*******************************************************************************/
//register a callback to receive NDEF message from the tag
//from the NFA_NDEF_DATA_EVT;
void NativeNfcTag::nativeNfcTag_registerNdefTypeHandler ()
{
    ALOGD ("%s", __FUNCTION__);
    sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
    NFA_RegisterNDefTypeHandler (TRUE, NFA_TNF_DEFAULT, (UINT8 *) "", 0, ndefHandlerCallback);
}


/*******************************************************************************
**
** Function:        nativeNfcTag_deregisterNdefTypeHandler
**
** Description:     No longer need to receive NDEF message from the tag.
**
** Returns:         None
**
*******************************************************************************/
void NativeNfcTag::nativeNfcTag_deregisterNdefTypeHandler ()
{
    ALOGD ("%s", __FUNCTION__);
    NFA_DeregisterNDefTypeHandler (sNdefTypeHandlerHandle);
    sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
}
