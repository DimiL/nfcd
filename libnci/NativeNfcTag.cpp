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
