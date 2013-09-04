/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Communicate with a peer using NFC-DEP, LLCP, SNEP.
 */
#pragma once
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>
#include "SyncEvent.h"
#include "NfcManager.h"
#include <string>
extern "C"
{
    #include "nfa_p2p_api.h"
}

class P2pServer;
class P2pClient;
class NfaConn;
#define MAX_NFA_CONNS_PER_SERVER    5

class PeerToPeer
{
public:
    typedef unsigned int tJNI_HANDLE;

    PeerToPeer ();

    ~PeerToPeer ();

    static PeerToPeer& getInstance();

    void initialize (NfcManager* pNfcManager);

    void llcpActivatedHandler (tNFA_LLCP_ACTIVATED& activated);

    void llcpDeactivatedHandler (tNFA_LLCP_DEACTIVATED& deactivated);

    void llcpFirstPacketHandler();

    void connectionEventHandler (UINT8 event, tNFA_CONN_EVT_DATA* eventData);

    bool registerServer (tJNI_HANDLE jniHandle, const char* serviceName);

    bool deregisterServer (tJNI_HANDLE jniHandle);

    bool accept (tJNI_HANDLE serverJniHandle, tJNI_HANDLE connJniHandle, int maxInfoUnit, int recvWindow);

    bool createClient (tJNI_HANDLE jniHandle, UINT16 miu, UINT8 rw);

    bool connectConnOriented (tJNI_HANDLE jniHandle, const char* serviceName);

    bool connectConnOriented (tJNI_HANDLE jniHandle, UINT8 destinationSap);

    bool send (tJNI_HANDLE jniHandle, UINT8* buffer, UINT16 bufferLen);

    bool receive (tJNI_HANDLE jniHandle, UINT8* buffer, UINT16 bufferLen, UINT16& actualLen);

    bool disconnectConnOriented (tJNI_HANDLE jniHandle);

    UINT16 getRemoteMaxInfoUnit (tJNI_HANDLE jniHandle);

    UINT8 getRemoteRecvWindow (tJNI_HANDLE jniHandle);

    void setP2pListenMask (tNFA_TECHNOLOGY_MASK p2pListenMask);

    void enableP2pListening (bool isEnable);

    void handleNfcOnOff (bool isOn);

    tJNI_HANDLE getNewJniHandle ();

    static void nfaServerCallback  (tNFA_P2P_EVT p2pEvent, tNFA_P2P_EVT_DATA *eventData);

    static void nfaClientCallback  (tNFA_P2P_EVT p2pEvent, tNFA_P2P_EVT_DATA *eventData);

private:
    static const int sMax = 10;
    static PeerToPeer sP2p;

    // Variables below only accessed from a single thread
    UINT16          mRemoteWKS;                 // Peer's well known services
    bool            mIsP2pListening;            // If P2P listening is enabled or not
    tNFA_TECHNOLOGY_MASK    mP2pListenTechMask; // P2P Listen mask

    // Variable below is protected by mNewJniHandleMutex
    tJNI_HANDLE     mNextJniHandle;

    // Variables below protected by mMutex
    // A note on locking order: mMutex in PeerToPeer is *ALWAYS*
    // locked before any locks / guards in P2pServer / P2pClient
    Mutex                    mMutex;
    android::sp<P2pServer>   mServers [sMax];
    android::sp<P2pClient>   mClients [sMax];

    // Synchronization variables
    SyncEvent       mSetTechEvent;              // completion event for NFA_SetP2pListenTech()
    SyncEvent       mSnepDefaultServerStartStopEvent; // completion event for NFA_SnepStartDefaultServer(), NFA_SnepStopDefaultServer()
    SyncEvent       mSnepRegisterEvent;         // completion event for NFA_SnepRegisterClient()
    Mutex           mDisconnectMutex;           // synchronize the disconnect operation
    Mutex           mNewJniHandleMutex;         // synchronize the creation of a new JNI handle

    NfcManager*     mNfcManager;

    static void ndefTypeCallback   (tNFA_NDEF_EVT event, tNFA_NDEF_EVT_DATA *evetnData);

    android::sp<P2pServer>   findServerLocked (tNFA_HANDLE nfaP2pServerHandle);

    android::sp<P2pServer>   findServerLocked (tJNI_HANDLE jniHandle);

    android::sp<P2pServer>   findServerLocked (const char *serviceName);

    void        removeServer (tJNI_HANDLE jniHandle);

    void        removeConn (tJNI_HANDLE jniHandle);

    bool        createDataLinkConn (tJNI_HANDLE jniHandle, const char* serviceName, UINT8 destinationSap);

    android::sp<P2pClient>   findClient (tNFA_HANDLE nfaConnHandle);

    android::sp<P2pClient>   findClient (tJNI_HANDLE jniHandle);

    android::sp<P2pClient>   findClientCon (tNFA_HANDLE nfaConnHandle);

    android::sp<NfaConn>     findConnection (tNFA_HANDLE nfaConnHandle);

    android::sp<NfaConn>     findConnection (tJNI_HANDLE jniHandle);
};

class NfaConn : public android::RefBase
{
public:
    tNFA_HANDLE         mNfaConnHandle;         // NFA handle of the P2P connection
    PeerToPeer::tJNI_HANDLE         mJniHandle;             // JNI handle of the P2P connection
    UINT16              mMaxInfoUnit;
    UINT8               mRecvWindow;
    UINT16              mRemoteMaxInfoUnit;
    UINT8               mRemoteRecvWindow;
    SyncEvent           mReadEvent;             // event for reading
    SyncEvent           mCongEvent;             // event for congestion
    SyncEvent           mDisconnectingEvent;     // event for disconnecting

    NfaConn();
};

class P2pServer : public android::RefBase
{
public:
    static const std::string sSnepServiceName;

    tNFA_HANDLE     mNfaP2pServerHandle;    // NFA p2p handle of local server
    PeerToPeer::tJNI_HANDLE     mJniHandle;     // JNI Handle
    SyncEvent       mRegServerEvent;        // for NFA_P2pRegisterServer()
    SyncEvent       mConnRequestEvent;      // for accept()
    std::string     mServiceName;

    P2pServer (PeerToPeer::tJNI_HANDLE jniHandle, const char* serviceName);

    bool registerWithStack();

    bool accept (PeerToPeer::tJNI_HANDLE serverJniHandle, PeerToPeer::tJNI_HANDLE connJniHandle,
            int maxInfoUnit, int recvWindow);

    void unblockAll();

    android::sp<NfaConn> findServerConnection (tNFA_HANDLE nfaConnHandle);

    android::sp<NfaConn> findServerConnection (PeerToPeer::tJNI_HANDLE jniHandle);

    bool removeServerConnection(PeerToPeer::tJNI_HANDLE jniHandle);

private:
    Mutex           mMutex;
    // mServerConn is protected by mMutex
    android::sp<NfaConn>     mServerConn[MAX_NFA_CONNS_PER_SERVER];

    android::sp<NfaConn> allocateConnection (PeerToPeer::tJNI_HANDLE jniHandle);
};

class P2pClient : public android::RefBase
{
public:
    tNFA_HANDLE           mNfaP2pClientHandle;    // NFA p2p handle of client
    bool                  mIsConnecting;          // Set true while connecting
    android::sp<NfaConn>  mClientConn;
    SyncEvent             mRegisteringEvent;      // For client registration
    SyncEvent             mConnectingEvent;       // for NFA_P2pConnectByName or Sap()
    SyncEvent             mSnepEvent;             // To wait for SNEP completion

    P2pClient ();

    ~P2pClient ();

    void unblock();
};

