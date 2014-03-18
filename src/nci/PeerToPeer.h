/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <utils/RefBase.h>
#include <utils/StrongPointer.h>
#include <string>

#include "SyncEvent.h"

extern "C"
{
  #include "nfa_p2p_api.h"
}

class NfcManager;
class P2pServer;
class P2pClient;
class NfaConn;

#define MAX_NFA_CONNS_PER_SERVER    5

/**
 * Communicate with a peer using NFC-DEP, LLCP, SNEP.
 */
class PeerToPeer
{
public:
  PeerToPeer();
  ~PeerToPeer();

  /**
   * Get the singleton PeerToPeer object.
   *
   * @return Singleton PeerToPeer object.
   */
  static PeerToPeer& getInstance();

  /**
   * Initialize member variables.
   *
   * @return None.
   */
  void initialize(NfcManager* pNfcManager);

  /**
   * Receive LLLCP-activated event from stack.
   *
   * @param activated Event data.
   * @return          None.
   */
  void llcpActivatedHandler(tNFA_LLCP_ACTIVATED& activated);

  /**
   * Receive LLLCP-deactivated event from stack.
   *
   * @param deactivated Event data.
   * @return            None.
   */
  void llcpDeactivatedHandler(tNFA_LLCP_DEACTIVATED& deactivated);

  void llcpFirstPacketHandler();

  /**
   * Receive events from the stack.
   *
   * @param  event     Event code.
   * @param  eventData Event data.
   * @return           None.
   */
  void connectionEventHandler(UINT8 event, tNFA_CONN_EVT_DATA* eventData);

  /**
   * Let a server start listening for peer's connection request.
   *
   * @param  handle      Connection handle.
   * @param  serviceName Server's service name.
   * @return             True if ok.
   */
  bool registerServer(unsigned int handle, const char* serviceName);

  /**
   * Stop a P2pServer from listening for peer.
   *
   * @param  handle Connection handle.
   * @return        True if ok.
   */
  bool deregisterServer(unsigned int handle);

  /**
   * Accept a peer's request to connect
   *
   * @param  serverHandle Server's handle.
   * @param  connHandle   Connection handle.
   * @param  maxInfoUnit  Maximum information unit.
   * @param  recvWindow   Receive window size.
   * @return              True if ok.
   */
  bool accept(unsigned int serverHandle, unsigned int connHandle, int maxInfoUnit, int recvWindow);

  /**
   * Create a P2pClient object for a new out-bound connection.
   *
   * @param  handle Connection handle.
   * @param  miu    Maximum information unit.
   * @param  rw     Receive window size.
   * @return        True if ok.
   */
  bool createClient(unsigned int handle, UINT16 miu, UINT8 rw);

  /**
   * Estabish a connection-oriented connection to a peer.
   *
   * @param  handle      Connection handle.
   * @param  serviceName Peer's service name.
   * @return             True if ok.
   */
  bool connectConnOriented(unsigned int handle, const char* serviceName);

  /**
   * Estabish a connection-oriented connection to a peer.
   *
   * @param  handle         Connection handle.
   * @param  destinationSap Peer's service access point.
   * @return                True if ok.
   */
  bool connectConnOriented(unsigned int handle, UINT8 destinationSap);

  /**
   * Send data to peer.
   *
   * @param  handle    Handle of connection.
   * @param  buffer    Buffer of data.
   * @param  bufferLen Length of data.
   * @return           True if ok.
   */
  bool send(unsigned int handle, UINT8* buffer, UINT16 bufferLen);

  /**
   * Receive data from peer.
   *
   * @param  handle    Handle of connection.
   * @param  buffer    Buffer to store data.
   * @param  bufferLen Max length of buffer.
   * @param  actualLen Actual length received. 
   * @return           True if ok.
   */
  bool receive(unsigned int handle, UINT8* buffer, UINT16 bufferLen, UINT16& actualLen);

  /**
   * Disconnect a connection-oriented connection with peer.
   *
   * @param  handle Handle of connection.  
   * @return        True if ok.
   */
  bool disconnectConnOriented(unsigned int handle);

  /**
   * Get peer's max information unit.
   *
   * @param  handle Handle of the connection.
   * @return        Peer's max information unit.
   */
  UINT16 getRemoteMaxInfoUnit(unsigned int handle);

  /**
   * Get peer's receive window size.
   *
   * @param  handle Handle of the connection.
   * @return        Peer's receive window size.
   */
  UINT8 getRemoteRecvWindow(unsigned int handle);

  /**
   * Sets the p2p listen technology mask.
   *
   * @param  p2pListenMask The p2p listen mask to be set?
   * @return               None.
   */
  void setP2pListenMask(tNFA_TECHNOLOGY_MASK p2pListenMask);

  /**
   * Start/stop polling/listening to peer that supports P2P.
   *
   * @param  isEnable Is enable polling/listening?
   * @return          None.
   */
  void enableP2pListening(bool isEnable);

  /**
   * Handle events related to turning NFC on/off by the user.
   *
   * @param  isOn Is NFC turning on?
   * @return      None.
   */
  void handleNfcOnOff(bool isOn);

  /**
   * Get a new handle.
   *
   * @return A new handle
   */
  unsigned int getNewHandle();

  /**
   * Receive LLCP-related events from the stack.
   *
   * @param  p2pEvent  Event code.
   * @param  eventData Event data.
   * @return           None.
   */
  static void nfaServerCallback(tNFA_P2P_EVT p2pEvent, tNFA_P2P_EVT_DATA *eventData);

  /**
   * Receive LLCP-related events from the stack.
   *
   * @param  p2pEvent  Event code.
   * @param  eventData Event data.
   * @return           None.
   */
  static void nfaClientCallback(tNFA_P2P_EVT p2pEvent, tNFA_P2P_EVT_DATA *eventData);

private:
  static const int  sMax = 10;
  static PeerToPeer sP2p;

  // Variables below only accessed from a single thread.
  UINT16          		mRemoteWKS;         // Peer's well known services.
  bool            		mIsP2pListening;    // If P2P listening is enabled or not.
  tNFA_TECHNOLOGY_MASK  mP2pListenTechMask; // P2P Listen mask.

  // Variable below is protected by mNewHandleMutex.
  unsigned int     mNextHandle;

  // Variables below protected by mMutex.
  // A note on locking order: mMutex in PeerToPeer is *ALWAYS*.
  // locked before any locks / guards in P2pServer / P2pClient.
  Mutex                    mMutex;
  android::sp<P2pServer>   mServers [sMax];
  android::sp<P2pClient>   mClients [sMax];

  // Synchronization variables.
  // Completion event for NFA_SetP2pListenTech().
  SyncEvent       mSetTechEvent;
  // Completion event for NFA_SnepStartDefaultServer(), NFA_SnepStopDefaultServer().
  SyncEvent       mSnepDefaultServerStartStopEvent;
  // Completion event for NFA_SnepRegisterClient().
  SyncEvent       mSnepRegisterEvent;
  // Synchronize the disconnect operation.
  Mutex           mDisconnectMutex;
  // Synchronize the creation of a new handle.
  Mutex           mNewHandleMutex;

  NfcManager*     mNfcManager;

  static void ndefTypeCallback   (tNFA_NDEF_EVT event, tNFA_NDEF_EVT_DATA *evetnData);

  android::sp<P2pServer>   findServerLocked(tNFA_HANDLE nfaP2pServerHandle);
  android::sp<P2pServer>   findServerLocked(unsigned int handle);
  android::sp<P2pServer>   findServerLocked(const char *serviceName);

  void        removeServer(unsigned int handle);
  void        removeConn(unsigned int handle);
  bool        createDataLinkConn(unsigned int handle, const char* serviceName, UINT8 destinationSap);

  android::sp<P2pClient>   findClient(tNFA_HANDLE nfaConnHandle);
  android::sp<P2pClient>   findClient(unsigned int handle);
  android::sp<P2pClient>   findClientCon(tNFA_HANDLE nfaConnHandle);
  android::sp<NfaConn>     findConnection(tNFA_HANDLE nfaConnHandle);
  android::sp<NfaConn>     findConnection(unsigned int handle);
};

class NfaConn
  : public android::RefBase
{
public:
  tNFA_HANDLE         mNfaConnHandle;      // NFA handle of the P2P connection.
  unsigned int        mHandle;             // Handle of the P2P connection.
  UINT16              mMaxInfoUnit;
  UINT8               mRecvWindow;
  UINT16              mRemoteMaxInfoUnit;
  UINT8               mRemoteRecvWindow;
  SyncEvent           mReadEvent;          // Event for reading.
  SyncEvent           mCongEvent;          // Event for congestion.
  SyncEvent           mDisconnectingEvent; // Event for disconnecting.

  NfaConn();
};

class P2pServer
  : public android::RefBase
{
public:
  static const std::string sSnepServiceName;

  tNFA_HANDLE     mNfaP2pServerHandle;  // NFA p2p handle of local server
  unsigned int    mHandle;              // Handle
  SyncEvent       mRegServerEvent;      // For NFA_P2pRegisterServer()
  SyncEvent       mConnRequestEvent;    // For accept()
  std::string     mServiceName;

  P2pServer(unsigned int handle, const char* serviceName);

  bool registerWithStack();
  bool accept(unsigned int serverHandle, unsigned int connHandle,
            int maxInfoUnit, int recvWindow);
  void unblockAll();

  android::sp<NfaConn> findServerConnection(tNFA_HANDLE nfaConnHandle);
  android::sp<NfaConn> findServerConnection(unsigned int handle);

  bool removeServerConnection(unsigned int handle);

private:
  Mutex           mMutex;
  // mServerConn is protected by mMutex.
  android::sp<NfaConn>     mServerConn[MAX_NFA_CONNS_PER_SERVER];

  android::sp<NfaConn> allocateConnection(unsigned int handle);
};

class P2pClient
  : public android::RefBase
{
public:
  tNFA_HANDLE           mNfaP2pClientHandle;    // NFA p2p handle of client.
  bool                  mIsConnecting;          // Set true while connecting.
  android::sp<NfaConn>  mClientConn;
  SyncEvent             mRegisteringEvent;      // For client registration.
  SyncEvent             mConnectingEvent;       // For NFA_P2pConnectByName or Sap().
  SyncEvent             mSnepEvent;             // To wait for SNEP completion.

  P2pClient();
  ~P2pClient();

  void unblock();
};
