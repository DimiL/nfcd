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
  static PeerToPeer& GetInstance();

  /**
   * Initialize member variables.
   *
   * @param  aNfcManager NFC manager class instance.
   * @return None.
   */
  void Initialize(NfcManager* aNfcManager);

  /**
   * Receive LLLCP-activated event from stack.
   *
   * @param  aActivated Event data.
   * @return            None.
   */
  void LlcpActivatedHandler(tNFA_LLCP_ACTIVATED& aActivated);

  /**
   * Receive LLLCP-deactivated event from stack.
   *
   * @param  aDeactivated Event data.
   * @return              None.
   */
  void LlcpDeactivatedHandler(tNFA_LLCP_DEACTIVATED& aDeactivated);

  void LlcpFirstPacketHandler();

  /**
   * Receive events from the stack.
   *
   * @param  aEvent     Event code.
   * @param  aEventData Event data.
   * @return            None.
   */
  void ConnectionEventHandler(uint8_t aEvent,
                              tNFA_CONN_EVT_DATA* aEventData);

  /**
   * Let a server start listening for peer's connection request.
   *
   * @param  aHandle      Connection handle.
   * @param  aServiceName Server's service name.
   * @return              True if ok.
   */
  bool RegisterServer(unsigned int aHandle,
                      const char* aServiceName);

  /**
   * Stop a P2pServer from listening for peer.
   *
   * @param  aHandle Connection handle.
   * @return         True if ok.
   */
  bool DeregisterServer(unsigned int aHandle);

  /**
   * Accept a peer's request to connect
   *
   * @param  aServerHandle Server's handle.
   * @param  aConnHandle   Connection handle.
   * @param  aMaxInfoUnit  Maximum information unit.
   * @param  aRecvWindow   Receive window size.
   * @return               True if ok.
   */
  bool Accept(unsigned int aServerHandle,
              unsigned int aConnHandle,
              int aMaxInfoUnit,
              int aRecvWindow);

  /**
   * Create a P2pClient object for a new out-bound connection.
   *
   * @param  aHandle Connection handle.
   * @param  aMiu    Maximum information unit.
   * @param  aRw     Receive window size.
   * @return         True if ok.
   */
  bool CreateClient(unsigned int aHandle,
                    uint16_t aMiu,
                    uint8_t aRw);

  /**
   * Estabish a connection-oriented connection to a peer.
   *
   * @param  aHandle      Connection handle.
   * @param  aServiceName Peer's service name.
   * @return              True if ok.
   */
  bool ConnectConnOriented(unsigned int aHandle,
                           const char* aServiceName);

  /**
   * Estabish a connection-oriented connection to a peer.
   *
   * @param  aHandle         Connection handle.
   * @param  aDestinationSap Peer's service access point.
   * @return                 True if ok.
   */
  bool ConnectConnOriented(unsigned int aHandle,
                           uint8_t aDestinationSap);

  /**
   * Send data to peer.
   *
   * @param  aHandle    Handle of connection.
   * @param  aBuffer    Buffer of data.
   * @param  aBufferLen Length of data.
   * @return            True if ok.
   */
  bool Send(unsigned int aHandle,
            uint8_t* aBuffer,
            uint16_t aBufferLen);

  /**
   * Receive data from peer.
   *
   * @param  aHandle    Handle of connection.
   * @param  aBuffer    Buffer to store data.
   * @param  aBufferLen Max length of buffer.
   * @param  aActualLen Actual length received.
   * @return            True if ok.
   */
  bool Receive(unsigned int aHandle,
               uint8_t* aBuffer,
               uint16_t aBufferLen,
               uint16_t& aActualLen);

  /**
   * Disconnect a connection-oriented connection with peer.
   *
   * @param  aHandle Handle of connection.
   * @return        True if ok.
   */
  bool DisconnectConnOriented(unsigned int aHandle);

  /**
   * Get peer's max information unit.
   *
   * @param  aHandle Handle of the connection.
   * @return         Peer's max information unit.
   */
  uint16_t GetRemoteMaxInfoUnit(unsigned int aHandle);

  /**
   * Get peer's receive window size.
   *
   * @param  aHandle Handle of the connection.
   * @return         Peer's receive window size.
   */
  uint8_t GetRemoteRecvWindow(unsigned int aHandle);

  /**
   * Sets the p2p listen technology mask.
   *
   * @param  aP2pListenMask The p2p listen mask to be set?
   * @return                None.
   */
  void SetP2pListenMask(tNFA_TECHNOLOGY_MASK aP2pListenMask);

  /**
   * Start/stop polling/listening to peer that supports P2P.
   *
   * @param  aIsEnable Is enable polling/listening?
   * @return           True if ok.
   */
  bool EnableP2pListening(bool aIsEnable);

  /**
   * Handle events related to turning NFC on/off by the user.
   *
   * @param  aIsOn Is NFC turning on?
   * @return       None.
   */
  void HandleNfcOnOff(bool aIsOn);

  /**
   * Get a new handle.
   *
   * @return A new handle
   */
  unsigned int GetNewHandle();

  /**
   * Receive LLCP-related events from the stack.
   *
   * @param  aP2pEvent  Event code.
   * @param  aEventData Event data.
   * @return            None.
   */
  static void NfaServerCallback(tNFA_P2P_EVT aP2pEvent,
                                tNFA_P2P_EVT_DATA* aEventData);

  /**
   * Receive LLCP-related events from the stack.
   *
   * @param  aP2pEvent  Event code.
   * @param  aEventData Event data.
   * @return            None.
   */
  static void NfaClientCallback(tNFA_P2P_EVT aP2pEvent,
                                tNFA_P2P_EVT_DATA* aEventData);

private:
  static const int sMax = 10;
  static PeerToPeer sP2p;

  // Variables below only accessed from a single thread.
  uint16_t              mRemoteWKS;         // Peer's well known services.
  bool                  mIsP2pListening;    // If P2P listening is enabled or not.
  tNFA_TECHNOLOGY_MASK  mP2pListenTechMask; // P2P Listen mask.

  // Variable below is protected by mNewHandleMutex.
  unsigned int mNextHandle;

  // Variables below protected by mMutex.
  // A note on locking order: mMutex in PeerToPeer is *ALWAYS*.
  // locked before any locks / guards in P2pServer / P2pClient.
  Mutex                    mMutex;
  android::sp<P2pServer>   mServers[sMax];
  android::sp<P2pClient>   mClients[sMax];

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

  static void NdefTypeCallback(tNFA_NDEF_EVT aEvent,
                               tNFA_NDEF_EVT_DATA* aEvetnData);

  android::sp<P2pServer> FindServerLocked(tNFA_HANDLE aNfaP2pServerHandle);
  android::sp<P2pServer> FindServerLocked(unsigned int aHandle);
  android::sp<P2pServer> FindServerLocked(const char* aServiceName);

  void RemoveServer(unsigned int aHandle);
  void RemoveConn(unsigned int aHandle);
  bool CreateDataLinkConn(unsigned int aHandle,
                          const char* aServiceName,
                          uint8_t aDestinationSap);

  android::sp<P2pClient> FindClient(tNFA_HANDLE aNfaConnHandle);
  android::sp<P2pClient> FindClient(unsigned int aHandle);
  android::sp<P2pClient> FindClientCon(tNFA_HANDLE aNfaConnHandle);
  android::sp<NfaConn>   FindConnection(tNFA_HANDLE aNfaConnHandle);
  android::sp<NfaConn>   FindConnection(unsigned int aHandle);
};

class NfaConn
  : public android::RefBase
{
public:
  tNFA_HANDLE         mNfaConnHandle;      // NFA handle of the P2P connection.
  unsigned int        mHandle;             // Handle of the P2P connection.
  uint16_t            mMaxInfoUnit;
  uint8_t             mRecvWindow;
  uint16_t            mRemoteMaxInfoUnit;
  uint8_t             mRemoteRecvWindow;
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

  P2pServer(unsigned int aHandle,
            const char* aServiceName);

  bool RegisterWithStack();
  bool Accept(unsigned int aServerHandle,
              unsigned int aConnHandle,
              int aMaxInfoUnit,
              int aRecvWindow);
  void UnblockAll();

  android::sp<NfaConn> FindServerConnection(tNFA_HANDLE aNfaConnHandle);
  android::sp<NfaConn> FindServerConnection(unsigned int aHandle);

  bool RemoveServerConnection(unsigned int aHandle);

private:
  Mutex           mMutex;
  // mServerConn is protected by mMutex.
  android::sp<NfaConn> mServerConn[MAX_NFA_CONNS_PER_SERVER];

  android::sp<NfaConn> AllocateConnection(unsigned int aHandle);
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

  void Unblock();
};
