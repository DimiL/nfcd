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

/**
 *  Communicate with a peer using NFC-DEP, LLCP, SNEP.
 */
#include "PeerToPeer.h"

#include "NfcDebug.h"
#include "NfcManager.h"
#include "NfcNciUtil.h"
#include "llcp_defs.h"
#include "config.h"
#include "IP2pDevice.h"
#include "NfcTagManager.h"

using namespace android;

/**
 * Some older PN544-based solutions would only send the first SYMM back
 * (as an initiator) after the full LTO (750ms). But our connect timer
 * starts immediately, and hence we may timeout if the timer is set to
 * 1000 ms. Worse, this causes us to immediately connect to the NPP
 * socket, causing concurrency issues in that stack. Increase the default
 * timeout to 2000 ms, giving us enough time to complete the first connect.
 */
#define LLCP_DATA_LINK_TIMEOUT    2000

PeerToPeer PeerToPeer::sP2p;
const std::string P2pServer::sSnepServiceName("urn:nfc:sn:snep");

PeerToPeer::PeerToPeer()
 : mRemoteWKS(0)
 , mIsP2pListening(false)
 , mP2pListenTechMask(NFA_TECHNOLOGY_MASK_A
                    | NFA_TECHNOLOGY_MASK_F
                    | NFA_TECHNOLOGY_MASK_A_ACTIVE
                    | NFA_TECHNOLOGY_MASK_F_ACTIVE)
 , mNextHandle(1)
 , mNfcManager(NULL)
{
  memset(mServers, 0, sizeof(mServers));
  memset(mClients, 0, sizeof(mClients));
}

PeerToPeer::~PeerToPeer()
{
}

PeerToPeer& PeerToPeer::GetInstance()
{
  return sP2p;
}

void PeerToPeer::Initialize(NfcManager* aNfcManager)
{
  unsigned long num = 0;

  mNfcManager = aNfcManager;

  if (GetNumValue("P2P_LISTEN_TECH_MASK", &num, sizeof(num)))
    mP2pListenTechMask = num;
}

sp<P2pServer> PeerToPeer::FindServerLocked(tNFA_HANDLE aNfaP2pServerHandle)
{
  for (int i = 0; i < sMax; i++) {
    if ((mServers[i] != NULL)
      && (mServers[i]->mNfaP2pServerHandle == aNfaP2pServerHandle) ) {
      return mServers[i];
    }
  }

  // If here, not found.
  return NULL;
}

sp<P2pServer> PeerToPeer::FindServerLocked(unsigned int aHandle)
{
  for (int i = 0; i < sMax; i++) {
    if (( mServers[i] != NULL)
      && (mServers[i]->mHandle == aHandle) ) {
      return mServers[i];
    }
  }

  // If here, not found.
  return NULL;
}

sp<P2pServer> PeerToPeer::FindServerLocked(const char* aServiceName)
{
  for (int i = 0; i < sMax; i++) {
    if ((mServers[i] != NULL) &&
        (mServers[i]->mServiceName.compare(aServiceName) == 0) )
      return mServers[i];
  }

  // If here, not found.
  return NULL;
}

bool PeerToPeer::RegisterServer(unsigned int aHandle,
                                const char* aServiceName)
{
  NCI_DEBUG("enter; service name: %s  handle: %u", aServiceName, aHandle);
  sp<P2pServer> pSrv = NULL;

  mMutex.Lock();
  // Check if already registered.
  if ((pSrv = FindServerLocked(aServiceName)) != NULL) {
    NCI_DEBUG("service name=%s  already registered, handle: 0x%04x",
              aServiceName, pSrv->mNfaP2pServerHandle);

    // Update handle.
    pSrv->mHandle = aHandle;
    mMutex.Unlock();
    return true;
  }

  for (int i = 0; i < sMax; i++) {
    if (mServers[i] == NULL) {
      pSrv = mServers[i] = new P2pServer(aHandle, aServiceName);
      NCI_DEBUG("added new p2p server  index: %d  handle: %u  name: %s", i, aHandle, aServiceName);
      break;
    }
  }
  mMutex.Unlock();

  if (pSrv == NULL) {
    NCI_ERROR("service name=%s  no free entry", aServiceName);
    return false;
  }

  if (pSrv->RegisterWithStack()) {
    NCI_DEBUG("got new p2p server h=0x%X", pSrv->mNfaP2pServerHandle);
    return true;
  } else {
    NCI_ERROR("invalid server handle");
    RemoveServer(aHandle);
    return false;
  }
}

void PeerToPeer::RemoveServer(unsigned int aHandle)
{
  AutoMutex mutex(mMutex);

  for (int i = 0; i < sMax; i++) {
    if ((mServers[i] != NULL) && (mServers[i]->mHandle == aHandle) ) {
      NCI_DEBUG("server handle: %u;  nfa_handle: 0x%04x; name: %s; index=%d",
                aHandle, mServers[i]->mNfaP2pServerHandle, mServers[i]->mServiceName.c_str(), i);
      mServers[i] = NULL;
      return;
    }
  }
  NCI_ERROR("unknown server handle: %u", aHandle);
}

void PeerToPeer::LlcpActivatedHandler(tNFA_LLCP_ACTIVATED& aActivated)
{
  NCI_DEBUG("enter");

  IP2pDevice* pIP2pDevice =
    reinterpret_cast<IP2pDevice*>(mNfcManager->QueryInterface(INTERFACE_P2P_DEVICE));

  if (pIP2pDevice == NULL) {
    NCI_ERROR("cannot get p2p device class");
    return;
  }

  // No longer need to receive NDEF message from a tag.
  NfcTagManager::DoDeregisterNdefTypeHandler();

  if (aActivated.is_initiator == true) {
    NCI_DEBUG("p2p initiator");
    pIP2pDevice->GetMode() = NfcDepEndpoint::MODE_P2P_INITIATOR;
  } else {
    NCI_DEBUG("p2p target");
    pIP2pDevice->GetMode() = NfcDepEndpoint::MODE_P2P_TARGET;
  }

  pIP2pDevice->GetHandle() = 0x1234;

  mNfcManager->NotifyLlcpLinkActivated(pIP2pDevice);

  NCI_DEBUG("exit");
}

void PeerToPeer::LlcpDeactivatedHandler(tNFA_LLCP_DEACTIVATED& /*deactivated*/)
{
  NCI_DEBUG("enter");

  IP2pDevice* pIP2pDevice =
    reinterpret_cast<IP2pDevice*>(mNfcManager->QueryInterface(INTERFACE_P2P_DEVICE));

  if (pIP2pDevice == NULL) {
    NCI_ERROR("cannot get p2p device class");
    return;
  }

  mNfcManager->NotifyLlcpLinkDeactivated(pIP2pDevice);

  NfcTagManager::DoRegisterNdefTypeHandler();
  NCI_DEBUG("exit");
}

void PeerToPeer::LlcpFirstPacketHandler()
{
  NCI_DEBUG("enter");

  mNfcManager->NotifyLlcpLinkFirstPacketReceived();

  NCI_DEBUG("exit");
}

bool PeerToPeer::Accept(unsigned int aServerHandle,
                        unsigned int aConnHandle,
                        int aMaxInfoUnit,
                        int aRecvWindow)
{
  sp<P2pServer> pSrv = NULL;

  NCI_DEBUG("enter; server handle: %u; conn handle: %u; maxInfoUnit: %d; recvWindow: %d",
            aServerHandle, aConnHandle, aMaxInfoUnit, aRecvWindow);

  mMutex.Lock();
  if ((pSrv = FindServerLocked(aServerHandle)) == NULL) {
    NCI_ERROR("unknown server handle: %u", aServerHandle);
    mMutex.Unlock();
    return false;
  }
  mMutex.Unlock();

  return pSrv->Accept(aServerHandle, aConnHandle, aMaxInfoUnit, aRecvWindow);
}

bool PeerToPeer::DeregisterServer(unsigned int aHandle)
{
  NCI_DEBUG("enter; handle: %u", aHandle);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  sp<P2pServer> pSrv = NULL;

  mMutex.Lock();
  if ((pSrv = FindServerLocked(aHandle)) == NULL) {
    NCI_ERROR("unknown service handle: %u", aHandle);
    mMutex.Unlock();
    return false;
  }
  mMutex.Unlock();

  {
    // Server does not call NFA_P2pDisconnect(), so unblock the accept().
    SyncEventGuard guard(pSrv->mConnRequestEvent);
    pSrv->mConnRequestEvent.NotifyOne();
  }

  nfaStat = NFA_P2pDeregister(pSrv->mNfaP2pServerHandle);
  if (nfaStat != NFA_STATUS_OK) {
    NCI_ERROR("deregister error=0x%X", nfaStat);
  }

  RemoveServer(aHandle);

  NCI_DEBUG("exit");
  return true;
}

bool PeerToPeer::CreateClient(unsigned int aHandle,
                              uint16_t aMiu,
                              uint8_t aRw)
{
  int i = 0;
  NCI_DEBUG("enter: h: %u  miu: %u  rw: %u", aHandle, aMiu, aRw);

  mMutex.Lock();
  sp<P2pClient> client = NULL;
  for (i = 0; i < sMax; i++) {
    if (mClients[i] == NULL) {
      mClients[i] = client = new P2pClient();

      mClients[i]->mClientConn->mHandle = aHandle;
      mClients[i]->mClientConn->mMaxInfoUnit = aMiu;
      mClients[i]->mClientConn->mRecvWindow = aRw;
      break;
    }
  }
  mMutex.Unlock();

  if (client == NULL) {
    NCI_ERROR("fail");
    return false;
  }

  NCI_DEBUG("pClient: 0x%p  assigned for client handle: %u", client.get(), aHandle);

  {
    SyncEventGuard guard(mClients[i]->mRegisteringEvent);
    NFA_P2pRegisterClient(NFA_P2P_DLINK_TYPE, NfaClientCallback);
    mClients[i]->mRegisteringEvent.Wait(); // Wait for NFA_P2P_REG_CLIENT_EVT.
  }

  if (mClients[i]->mNfaP2pClientHandle != NFA_HANDLE_INVALID) {
    NCI_DEBUG("exit; new client handle: %u   NFA Handle: 0x%04x",
              aHandle, client->mClientConn->mNfaConnHandle);
    return true;
  } else {
    NCI_ERROR("FAILED; new client handle: %u   NFA Handle: 0x%04x",
              aHandle, client->mClientConn->mNfaConnHandle);
    RemoveConn(aHandle);
    return false;
  }
}

void PeerToPeer::RemoveConn(unsigned int aHandle)
{
  AutoMutex mutex(mMutex);
  // If the connection is a for a client, delete the client itself.
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL) && (mClients[i]->mClientConn->mHandle == aHandle)) {
      if (mClients[i]->mNfaP2pClientHandle != NFA_HANDLE_INVALID) {
        NFA_P2pDeregister(mClients[i]->mNfaP2pClientHandle);
      }

      mClients[i] = NULL;
      NCI_DEBUG("deleted client handle: %u  index: %u", aHandle, i);
      return;
    }
  }

  // If the connection is for a server, just delete the connection.
  for (int i = 0; i < sMax; i++) {
    if (mServers[i] != NULL) {
      if (mServers[i]->RemoveServerConnection(aHandle)) {
        return;
      }
    }
  }

  NCI_ERROR("could not find handle: %u", aHandle);
}

bool PeerToPeer::ConnectConnOriented(unsigned int aHandle,
                                     const char* aServiceName)
{
  NCI_DEBUG("enter; h: %u  service name=%s", aHandle, aServiceName);
  bool stat = CreateDataLinkConn(aHandle, aServiceName, 0);
  NCI_DEBUG("exit; h: %u  stat: %u", aHandle, stat);
  return stat;
}

bool PeerToPeer::ConnectConnOriented(unsigned int aHandle,
                                     uint8_t aDestinationSap)
{
  NCI_DEBUG("enter; h: %u  dest sap: 0x%X", aHandle, aDestinationSap);
  bool stat = CreateDataLinkConn(aHandle, NULL, aDestinationSap);
  NCI_DEBUG("exit; h: %u  stat: %u", aHandle, stat);
  return stat;
}

bool PeerToPeer::CreateDataLinkConn(unsigned int aHandle,
                                    const char* aServiceName,
                                    uint8_t aDestinationSap)
{
  NCI_DEBUG("enter");
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  sp<P2pClient> pClient = NULL;

  if ((pClient = FindClient(aHandle)) == NULL) {
    NCI_ERROR("can't find client, handle: %u", aHandle);
    return false;
  }

  {
    SyncEventGuard guard(pClient->mConnectingEvent);
    pClient->mIsConnecting = true;

    if (aServiceName) {
      nfaStat = NFA_P2pConnectByName(pClient->mNfaP2pClientHandle,
                  const_cast<char*>(aServiceName), pClient->mClientConn->mMaxInfoUnit,
                  pClient->mClientConn->mRecvWindow);
    } else if (aDestinationSap) {
      nfaStat = NFA_P2pConnectBySap(pClient->mNfaP2pClientHandle, aDestinationSap,
                  pClient->mClientConn->mMaxInfoUnit, pClient->mClientConn->mRecvWindow);
    }

    if (nfaStat == NFA_STATUS_OK) {
      NCI_DEBUG("wait for connected event  mConnectingEvent: 0x%p", pClient.get());
      pClient->mConnectingEvent.Wait();
    }
  }

  if (nfaStat == NFA_STATUS_OK) {
    if (pClient->mClientConn->mNfaConnHandle == NFA_HANDLE_INVALID) {
      RemoveConn(aHandle);
      nfaStat = NFA_STATUS_FAILED;
    } else {
      pClient->mIsConnecting = false;
    }
  } else {
    RemoveConn(aHandle);
    NCI_ERROR("fail; error=0x%X", nfaStat);
  }

  NCI_DEBUG("exit");
  return nfaStat == NFA_STATUS_OK;
}

sp<P2pClient> PeerToPeer::FindClient(tNFA_HANDLE aNfaConnHandle)
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL) &&
        (mClients[i]->mNfaP2pClientHandle == aNfaConnHandle))
      return mClients[i];
  }
  return NULL;
}

sp<P2pClient> PeerToPeer::FindClient(unsigned int aHandle)
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL) &&
        (mClients[i]->mClientConn->mHandle == aHandle))
      return mClients[i];
  }
  return NULL;
}

sp<P2pClient> PeerToPeer::FindClientCon(tNFA_HANDLE aNfaConnHandle)
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL) &&
        (mClients[i]->mClientConn->mNfaConnHandle == aNfaConnHandle))
      return mClients[i];
  }
  return NULL;
}

sp<NfaConn> PeerToPeer::FindConnection(tNFA_HANDLE aNfaConnHandle)
{
  AutoMutex mutex(mMutex);
  // First, look through all the client control blocks.
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL)
      &&(mClients[i]->mClientConn->mNfaConnHandle == aNfaConnHandle)) {
      return mClients[i]->mClientConn;
    }
  }

  // Not found yet. Look through all the server control blocks.
  for (int i = 0; i < sMax; i++) {
    if (mServers[i] != NULL) {
      sp<NfaConn> conn = mServers[i]->FindServerConnection(aNfaConnHandle);
      if (conn != NULL) {
        return conn;
      }
    }
  }

  // Not found...
  return NULL;
}

sp<NfaConn> PeerToPeer::FindConnection(unsigned int aHandle)
{
  AutoMutex mutex(mMutex);
  // First, look through all the client control blocks.
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL)
      && (mClients[i]->mClientConn->mHandle == aHandle)) {
      return mClients[i]->mClientConn;
    }
  }

  // Not found yet. Look through all the server control blocks.
  for (int i = 0; i < sMax; i++) {
    if (mServers[i] != NULL) {
      sp<NfaConn> conn = mServers[i]->FindServerConnection(aHandle);
      if (conn != NULL) {
        return conn;
      }
    }
  }

  // Not found...
  return NULL;
}

bool PeerToPeer::Send(unsigned int aHandle,
                      uint8_t* aBuffer,
                      uint16_t aBufferLen)
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  sp<NfaConn> pConn = NULL;

  if ((pConn = FindConnection(aHandle)) == NULL) {
    NCI_ERROR("can't find connection handle: %u", aHandle);
    return false;
  }

  while (true) {
    SyncEventGuard guard(pConn->mCongEvent);
    nfaStat = NFA_P2pSendData(pConn->mNfaConnHandle, aBufferLen, aBuffer);
    if (nfaStat == NFA_STATUS_CONGESTED) {
      pConn->mCongEvent.Wait(); // Wait for NFA_P2P_CONGEST_EVT.
    } else {
      break;
    }

    if (pConn->mNfaConnHandle == NFA_HANDLE_INVALID) { // Peer already disconnected.
      NCI_DEBUG("peer disconnected");
      return false;
    }
  }

  if (nfaStat == NFA_STATUS_OK) {
    NCI_DEBUG("exit OK; handle: %u  NFA Handle: 0x%04x", aHandle, pConn->mNfaConnHandle);
  } else {
    NCI_ERROR("Data not sent; handle: %u  NFA Handle: 0x%04x  error: 0x%04x",
              aHandle, pConn->mNfaConnHandle, nfaStat);
  }

  return nfaStat == NFA_STATUS_OK;
}

bool PeerToPeer::Receive(unsigned int aHandle,
                         uint8_t* aBuffer,
                         uint16_t aBufferLen,
                         uint16_t& aActualLen)
{
  NCI_DEBUG("enter; handle: %u  bufferLen: %u", aHandle, aBufferLen);
  sp<NfaConn> pConn = NULL;
  tNFA_STATUS stat = NFA_STATUS_FAILED;
  long unsigned int actualDataLen = 0;
  BOOLEAN isMoreData = TRUE;
  bool retVal = false;

  if ((pConn = FindConnection(aHandle)) == NULL) {
    NCI_ERROR("can't find connection handle: %u", aHandle);
    return false;
  }

  NCI_DEBUG("handle: %u  nfaHandle: 0x%04X  buf len=%u",
            pConn->mHandle, pConn->mNfaConnHandle, aBufferLen);

  while (pConn->mNfaConnHandle != NFA_HANDLE_INVALID) {
    // NFA_P2pReadData() is synchronous.
    stat = NFA_P2pReadData(
      pConn->mNfaConnHandle, aBufferLen, &actualDataLen, aBuffer, &isMoreData);
    if ((stat == NFA_STATUS_OK) && (actualDataLen > 0)) { // Received some data.
      aActualLen = (uint16_t)actualDataLen;
      retVal = true;
      break;
    }
    NCI_DEBUG("waiting for data...");
    {
      SyncEventGuard guard(pConn->mReadEvent);
      pConn->mReadEvent.Wait();
    }
  } // while.

  NCI_DEBUG("exit; nfa h: 0x%X  ok: %u  actual len: %u",
            pConn->mNfaConnHandle, retVal, aActualLen);
  return retVal;
}

bool PeerToPeer::DisconnectConnOriented(unsigned int aHandle)
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  sp<P2pClient> pClient = NULL;
  sp<NfaConn> pConn = NULL;

  NCI_DEBUG("enter; handle: %u", aHandle);

  if ((pConn = FindConnection(aHandle)) == NULL) {
    NCI_ERROR("can't find connection handle: %u", aHandle);
    return false;
  }

  // If this is a client, he may not be connected yet, so unblock him just in case.
  if (((pClient = FindClient(aHandle)) != NULL) && (pClient->mIsConnecting) ) {
    SyncEventGuard guard(pClient->mConnectingEvent);
    pClient->mConnectingEvent.NotifyOne();
    return true;
  }

  {
    SyncEventGuard guard1(pConn->mCongEvent);
    pConn->mCongEvent.NotifyOne(); // Unblock send() if congested.
  }
  {
    SyncEventGuard guard2(pConn->mReadEvent);
    pConn->mReadEvent.NotifyOne(); // Unblock receive().
  }

  if (pConn->mNfaConnHandle != NFA_HANDLE_INVALID) {
    NCI_DEBUG("try disconn nfa h=0x%04X", pConn->mNfaConnHandle);
    SyncEventGuard guard(pConn->mDisconnectingEvent);
    nfaStat = NFA_P2pDisconnect(pConn->mNfaConnHandle, FALSE);

    if (nfaStat != NFA_STATUS_OK) {
      NCI_ERROR("fail p2p disconnect");
    } else {
      pConn->mDisconnectingEvent.Wait();
    }
  }

  mDisconnectMutex.Lock();
  RemoveConn(aHandle);
  mDisconnectMutex.Unlock();

  NCI_DEBUG("exit; handle: %u", aHandle);
  return nfaStat == NFA_STATUS_OK;
}

uint16_t PeerToPeer::GetRemoteMaxInfoUnit(unsigned int aHandle)
{
  sp<NfaConn> pConn = NULL;

  if ((pConn = FindConnection(aHandle)) == NULL) {
    NCI_ERROR("can't find client  handle: %u", aHandle);
    return 0;
  }
  NCI_DEBUG("handle: %u   MIU: %u", aHandle, pConn->mRemoteMaxInfoUnit);
  return pConn->mRemoteMaxInfoUnit;
}

uint8_t PeerToPeer::GetRemoteRecvWindow(unsigned int aHandle)
{
  NCI_DEBUG("client handle: %u", aHandle);
  sp<NfaConn> pConn = NULL;

  if ((pConn = FindConnection(aHandle)) == NULL) {
    NCI_ERROR("can't find client");
    return 0;
  }
  return pConn->mRemoteRecvWindow;
}

void PeerToPeer::SetP2pListenMask(tNFA_TECHNOLOGY_MASK aP2pListenMask)
{
  mP2pListenTechMask = aP2pListenMask;
}

bool PeerToPeer::EnableP2pListening(bool aIsEnable)
{
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;

  NCI_DEBUG("enter isEnable: %u  mIsP2pListening: %u", aIsEnable, mIsP2pListening);

  // If request to enable P2P listening, and we were not already listening.
  if ((aIsEnable == true) && (mIsP2pListening == false) && (mP2pListenTechMask != 0) ) {
    SyncEventGuard guard(mSetTechEvent);
    if ((nfaStat = NFA_SetP2pListenTech(mP2pListenTechMask)) == NFA_STATUS_OK) {
      mSetTechEvent.Wait();
      mIsP2pListening = true;
    }
    else {
      NCI_ERROR("fail enable listen; error=0x%X", nfaStat);
      return false;
    }
  } else if ((aIsEnable == false) && (mIsP2pListening == true) ) {
    SyncEventGuard guard(mSetTechEvent);
    // Request to disable P2P listening, check if it was enabled.
    if ((nfaStat = NFA_SetP2pListenTech(0)) == NFA_STATUS_OK) {
      mSetTechEvent.Wait();
      mIsP2pListening = false;
    } else {
      NCI_ERROR("fail disable listen; error=0x%X", nfaStat);
      return false;
    }
  }
  NCI_DEBUG("exit; mIsP2pListening: %u", mIsP2pListening);
  return true;
}

void PeerToPeer::HandleNfcOnOff(bool aIsOn)
{
  NCI_DEBUG("enter; is on=%u", aIsOn);

  mIsP2pListening = false;            // In both cases, P2P will not be listening.

  AutoMutex mutex(mMutex);
  if (aIsOn) {
    // Start with no clients or servers.
    memset(mServers, 0, sizeof(mServers));
    memset(mClients, 0, sizeof(mClients));
  } else {
    // Disconnect through all the clients.
    for (int i = 0; i < sMax; i++) {
      if (mClients[i] != NULL) {
        if (mClients[i]->mClientConn->mNfaConnHandle == NFA_HANDLE_INVALID) {
          SyncEventGuard guard(mClients[i]->mConnectingEvent);
          mClients[i]->mConnectingEvent.NotifyOne();
        } else {
          mClients[i]->mClientConn->mNfaConnHandle = NFA_HANDLE_INVALID;
          {
            SyncEventGuard guard1(mClients[i]->mClientConn->mCongEvent);
            mClients[i]->mClientConn->mCongEvent.NotifyOne(); // Unblock send().
          }
          {
            SyncEventGuard guard2(mClients[i]->mClientConn->mReadEvent);
            mClients[i]->mClientConn->mReadEvent.NotifyOne(); // Unblock receive().
          }
        }
      }
    } // Loop.

    // Now look through all the server control blocks.
    for (int i = 0; i < sMax; i++) {
      if (mServers[i] != NULL) {
        mServers[i]->UnblockAll();
      }
    } // Loop.

  }
  NCI_DEBUG("exit");
}

void PeerToPeer::NfaServerCallback(tNFA_P2P_EVT aP2pEvent,
                                   tNFA_P2P_EVT_DATA* aEventData)
{
  sp<P2pServer>   pSrv = NULL;
  sp<NfaConn>     pConn = NULL;

  NCI_DEBUG("enter; event=0x%X", aP2pEvent);

  switch (aP2pEvent) {
    case NFA_P2P_REG_SERVER_EVT:  // NFA_P2pRegisterServer() has started to listen.
      NCI_DEBUG("NFA_P2P_REG_SERVER_EVT; handle: 0x%04x; service sap=0x%02x  name: %s",
                aEventData->reg_server.server_handle,
                aEventData->reg_server.server_sap,
                aEventData->reg_server.service_name);

      sP2p.mMutex.Lock();
      pSrv = sP2p.FindServerLocked(aEventData->reg_server.service_name);
      sP2p.mMutex.Unlock();
      if (pSrv == NULL) {
        NCI_ERROR("NFA_P2P_REG_SERVER_EVT for unknown service: %s",
                  aEventData->reg_server.service_name);
      } else {
        SyncEventGuard guard(pSrv->mRegServerEvent);
        pSrv->mNfaP2pServerHandle = aEventData->reg_server.server_handle;
        pSrv->mRegServerEvent.NotifyOne(); // Unblock registerServer().
      }
      break;
    case NFA_P2P_ACTIVATED_EVT: // Remote device has activated.
      NCI_DEBUG("NFA_P2P_ACTIVATED_EVT; handle: 0x%04x",
                aEventData->activated.handle);
      break;
    case NFA_P2P_DEACTIVATED_EVT:
      NCI_DEBUG("NFA_P2P_DEACTIVATED_EVT; handle: 0x%04x", aEventData->activated.handle);
      break;
    case NFA_P2P_CONN_REQ_EVT:
      NCI_DEBUG("NFA_P2P_CONN_REQ_EVT; nfa server h=0x%04x; nfa conn h=0x%04x; remote sap=0x%02x",
                aEventData->conn_req.server_handle,
                aEventData->conn_req.conn_handle,
                aEventData->conn_req.remote_sap);

      sP2p.mMutex.Lock();
      pSrv = sP2p.FindServerLocked(aEventData->conn_req.server_handle);
      sP2p.mMutex.Unlock();
      if (pSrv == NULL) {
        NCI_ERROR("NFA_P2P_CONN_REQ_EVT; unknown server h");
        return;
      }
      NCI_DEBUG("NFA_P2P_CONN_REQ_EVT; server h=%u", pSrv->mHandle);

      // Look for a connection block that is waiting (handle invalid).
      if ((pConn = pSrv->FindServerConnection((tNFA_HANDLE)NFA_HANDLE_INVALID)) == NULL) {
        NCI_ERROR("NFA_P2P_CONN_REQ_EVT; server not listening");
      } else {
        SyncEventGuard guard(pSrv->mConnRequestEvent);
        pConn->mNfaConnHandle = aEventData->conn_req.conn_handle;
        pConn->mRemoteMaxInfoUnit = aEventData->conn_req.remote_miu;
        pConn->mRemoteRecvWindow = aEventData->conn_req.remote_rw;
        NCI_DEBUG("NFA_P2P_CONN_REQ_EVT; server h=%u; conn h=%u; notify conn req",
                  pSrv->mHandle, pConn->mHandle);
        pSrv->mConnRequestEvent.NotifyOne(); // Unblock accept().
      }
      break;
    case NFA_P2P_CONNECTED_EVT:
      NCI_DEBUG("NFA_P2P_CONNECTED_EVT; h=0x%x  remote sap=0x%X",
                aEventData->connected.client_handle,
                aEventData->connected.remote_sap);
      break;
    case NFA_P2P_DISC_EVT:
      NCI_DEBUG("NFA_P2P_DISC_EVT; h=0x%04x; reason=0x%X",
                aEventData->disc.handle,
                aEventData->disc.reason);
      // Look for the connection block.
      if ((pConn = sP2p.FindConnection(aEventData->disc.handle)) == NULL) {
        NCI_ERROR("NFA_P2P_DISC_EVT: can't find conn for NFA handle: 0x%04x",
                  aEventData->disc.handle);
      } else {
        sP2p.mDisconnectMutex.Lock();
        pConn->mNfaConnHandle = NFA_HANDLE_INVALID;
        {
          NCI_DEBUG("NFA_P2P_DISC_EVT; try guard disconn event");
          SyncEventGuard guard3(pConn->mDisconnectingEvent);
          pConn->mDisconnectingEvent.NotifyOne();
          NCI_DEBUG("NFA_P2P_DISC_EVT; notified disconn event");
        }
        {
          NCI_DEBUG("NFA_P2P_DISC_EVT; try guard congest event");
          SyncEventGuard guard1(pConn->mCongEvent);
          pConn->mCongEvent.NotifyOne(); // Unblock write (if congested).
          NCI_DEBUG("NFA_P2P_DISC_EVT; notified congest event");
        }
        {
          NCI_DEBUG("NFA_P2P_DISC_EVT; try guard read event");
          SyncEventGuard guard2(pConn->mReadEvent);
          pConn->mReadEvent.NotifyOne(); // Unblock receive().
          NCI_DEBUG("NFA_P2P_DISC_EVT; notified read event");
        }
        sP2p.mDisconnectMutex.Unlock();
      }
      break;
    case NFA_P2P_DATA_EVT:
      // Look for the connection block.
      if ((pConn = sP2p.FindConnection(aEventData->data.handle)) == NULL) {
        NCI_ERROR("NFA_P2P_DATA_EVT: can't find conn for NFA handle: 0x%04x",
                  aEventData->data.handle);
      } else {
        NCI_DEBUG("NFA_P2P_DATA_EVT; h=0x%X; remote sap=0x%X",
                  aEventData->data.handle,
                  aEventData->data.remote_sap);
        SyncEventGuard guard(pConn->mReadEvent);
        pConn->mReadEvent.NotifyOne();
      }
      break;
    case NFA_P2P_CONGEST_EVT:
      // Look for the connection block.
      if ((pConn = sP2p.FindConnection(aEventData->congest.handle)) == NULL) {
        NCI_ERROR("NFA_P2P_CONGEST_EVT: can't find conn for NFA handle: 0x%04x",
                  aEventData->congest.handle);
      } else {
        NCI_DEBUG("NFA_P2P_CONGEST_EVT; nfa handle: 0x%04x  congested: %u",
                  aEventData->congest.handle, aEventData->congest.is_congested);
        if (aEventData->congest.is_congested == FALSE) {
          SyncEventGuard guard(pConn->mCongEvent);
          pConn->mCongEvent.NotifyOne();
        }
      }
      break;
    default:
      NCI_ERROR("unknown event 0x%X ????", aP2pEvent);
      break;
  }
  NCI_DEBUG("exit");
}

void PeerToPeer::NfaClientCallback(tNFA_P2P_EVT aP2pEvent,
                                   tNFA_P2P_EVT_DATA* aEventData)
{
  sp<NfaConn> pConn = NULL;
  sp<P2pClient> pClient = NULL;

  NCI_DEBUG("enter; event=%u", aP2pEvent);

  switch (aP2pEvent) {
    case NFA_P2P_REG_CLIENT_EVT:
      // Look for a client that is trying to register.
      if ((pClient = sP2p.FindClient((tNFA_HANDLE)NFA_HANDLE_INVALID)) == NULL) {
        NCI_ERROR("NFA_P2P_REG_CLIENT_EVT: can't find waiting client");
      } else {
        NCI_DEBUG("NFA_P2P_REG_CLIENT_EVT; Conn Handle: 0x%04x, pClient: 0x%p",
                  aEventData->reg_client.client_handle, pClient.get());

        SyncEventGuard guard(pClient->mRegisteringEvent);
        pClient->mNfaP2pClientHandle = aEventData->reg_client.client_handle;
        pClient->mRegisteringEvent.NotifyOne();
      }
      break;

    case NFA_P2P_CONNECTED_EVT:
      // Look for the client that is trying to connect.
      if ((pClient = sP2p.FindClient(aEventData->connected.client_handle)) == NULL) {
        NCI_ERROR("NFA_P2P_CONNECTED_EVT: can't find client: 0x%04x",
                  aEventData->connected.client_handle);
      } else {
        NCI_DEBUG("NFA_P2P_CONNECTED_EVT; client_handle=0x%04x  conn_handle: 0x%04x  remote sap=0x%X  pClient: 0x%p",
                  aEventData->connected.client_handle,
                  aEventData->connected.conn_handle,
                  aEventData->connected.remote_sap,
                  pClient.get());

        SyncEventGuard guard(pClient->mConnectingEvent);
        pClient->mClientConn->mNfaConnHandle = aEventData->connected.conn_handle;
        pClient->mClientConn->mRemoteMaxInfoUnit = aEventData->connected.remote_miu;
        pClient->mClientConn->mRemoteRecvWindow = aEventData->connected.remote_rw;
        pClient->mConnectingEvent.NotifyOne(); // Unblock createDataLinkConn().
      }
      break;

    case NFA_P2P_DISC_EVT:
      NCI_DEBUG("NFA_P2P_DISC_EVT; h=0x%04x; reason=0x%X", aEventData->disc.handle, aEventData->disc.reason);
      // Look for the connection block.
      if ((pConn = sP2p.FindConnection(aEventData->disc.handle)) == NULL) {
        // If no connection, may be a client that is trying to connect.
        if ((pClient = sP2p.FindClient(aEventData->disc.handle)) == NULL) {
          NCI_ERROR("NFA_P2P_DISC_EVT: can't find client for NFA handle: 0x%04x",
                    aEventData->disc.handle);
          return;
        }
        // Unblock createDataLinkConn().
        SyncEventGuard guard(pClient->mConnectingEvent);
        pClient->mConnectingEvent.NotifyOne();
      } else {
        sP2p.mDisconnectMutex.Lock();
        pConn->mNfaConnHandle = NFA_HANDLE_INVALID;
        {
          NCI_DEBUG("NFA_P2P_DISC_EVT; try guard disconn event");
          SyncEventGuard guard3(pConn->mDisconnectingEvent);
          pConn->mDisconnectingEvent.NotifyOne();
          NCI_DEBUG("NFA_P2P_DISC_EVT; notified disconn event");
        }
        {
          NCI_DEBUG("NFA_P2P_DISC_EVT; try guard congest event");
          SyncEventGuard guard1(pConn->mCongEvent);
          pConn->mCongEvent.NotifyOne(); // Unblock write (if congested).
          NCI_DEBUG("NFA_P2P_DISC_EVT; notified congest event");
        }
        {
          NCI_DEBUG("NFA_P2P_DISC_EVT; try guard read event");
          SyncEventGuard guard2(pConn->mReadEvent);
          pConn->mReadEvent.NotifyOne(); // Unblock receive().
          NCI_DEBUG("NFA_P2P_DISC_EVT; notified read event");
        }
        sP2p.mDisconnectMutex.Unlock();
      }
      break;

    case NFA_P2P_DATA_EVT:
      // Look for the connection block.
      if ((pConn = sP2p.FindConnection(aEventData->data.handle)) == NULL) {
        NCI_ERROR("NFA_P2P_DATA_EVT: can't find conn for NFA handle: 0x%04x",
                  aEventData->data.handle);
      } else {
        NCI_DEBUG("NFA_P2P_DATA_EVT; h=0x%X; remote sap=0x%X",
                  aEventData->data.handle, aEventData->data.remote_sap);
        SyncEventGuard guard(pConn->mReadEvent);
        pConn->mReadEvent.NotifyOne();
      }
      break;

    case NFA_P2P_CONGEST_EVT:
      // Look for the connection block.
      if ((pConn = sP2p.FindConnection(aEventData->congest.handle)) == NULL) {
        NCI_ERROR("NFA_P2P_CONGEST_EVT: can't find conn for NFA handle: 0x%04x",
                  aEventData->congest.handle);
      } else {
        SyncEventGuard guard(pConn->mCongEvent);
        pConn->mCongEvent.NotifyOne();
      }
      break;

    default:
      NCI_ERROR("unknown event 0x%X ????", aP2pEvent);
      break;
  }
}

void PeerToPeer::ConnectionEventHandler(uint8_t aEvent,
                                        tNFA_CONN_EVT_DATA* /*eventData*/)
{
  switch (aEvent) {
    case NFA_SET_P2P_LISTEN_TECH_EVT: {
      SyncEventGuard guard(mSetTechEvent);
      mSetTechEvent.NotifyOne(); // Unblock NFA_SetP2pListenTech().
      break;
    }
  }
}

unsigned int PeerToPeer::GetNewHandle()
{
  unsigned int newHandle = 0;

  mNewHandleMutex.Lock();
  newHandle = mNextHandle++;
  mNewHandleMutex.Unlock();
  return newHandle;
}

P2pServer::P2pServer(unsigned int aHandle,
                     const char* aServiceName)
 : mNfaP2pServerHandle(NFA_HANDLE_INVALID)
 , mHandle(aHandle)
{
  mServiceName.assign(aServiceName);

  memset(mServerConn, 0, sizeof(mServerConn));
}

bool P2pServer::RegisterWithStack()
{
  NCI_DEBUG("enter; service name: %s  handle: %u", mServiceName.c_str(), mHandle);
  tNFA_STATUS stat = NFA_STATUS_OK;
  uint8_t serverSap = NFA_P2P_ANY_SAP;

  /**
   * default values for all LLCP parameters:
   * - Local Link MIU (LLCP_MIU)
   * - Option parameter (LLCP_OPT_VALUE)
   * - Response Waiting Time Index (LLCP_WAITING_TIME)
   * - Local Link Timeout (LLCP_LTO_VALUE)
   * - Inactivity Timeout as initiator role (LLCP_INIT_INACTIVITY_TIMEOUT)
   * - Inactivity Timeout as target role (LLCP_TARGET_INACTIVITY_TIMEOUT)
   * - Delay SYMM response (LLCP_DELAY_RESP_TIME)
   * - Data link connection timeout (LLCP_DATA_LINK_CONNECTION_TOUT)
   * - Delay timeout to send first PDU as initiator (LLCP_DELAY_TIME_TO_SEND_FIRST_PDU)
   */
  stat = NFA_P2pSetLLCPConfig(LLCP_MAX_MIU,
         LLCP_OPT_VALUE,
         LLCP_WAITING_TIME,
         LLCP_LTO_VALUE,
         0, // Use 0 for infinite timeout for symmetry procedure when acting as initiator.
         0, // Use 0 for infinite timeout for symmetry procedure when acting as target.
         LLCP_DELAY_RESP_TIME,
         LLCP_DATA_LINK_TIMEOUT,
         LLCP_DELAY_TIME_TO_SEND_FIRST_PDU);
  if (stat != NFA_STATUS_OK)
    NCI_ERROR("fail set LLCP config; error=0x%X", stat);

  if (sSnepServiceName.compare(mServiceName) == 0)
    serverSap = LLCP_SAP_SNEP; // LLCP_SAP_SNEP == 4.

  {
    SyncEventGuard guard(mRegServerEvent);
    stat = NFA_P2pRegisterServer(serverSap,
                                 NFA_P2P_DLINK_TYPE,
                                 const_cast<char*>(mServiceName.c_str()),
                                 PeerToPeer::NfaServerCallback);
    if (stat != NFA_STATUS_OK) {
      NCI_ERROR("fail register p2p server; error=0x%X", stat);
      return false;
    }
    NCI_DEBUG("wait for listen-completion event");
    // Wait for NFA_P2P_REG_SERVER_EVT.
    mRegServerEvent.Wait();
  }

  return (mNfaP2pServerHandle != NFA_HANDLE_INVALID);
}

bool P2pServer::Accept(unsigned int aServerHandle,
                       unsigned int aConnHandle,
                       int aMaxInfoUnit,
                       int aRecvWindow)
{
  tNFA_STATUS nfaStat = NFA_STATUS_OK;

  sp<NfaConn> connection = AllocateConnection(aConnHandle);
  if (connection == NULL) {
    NCI_ERROR("failed to allocate new server connection");
    return false;
  }

  {
    // Wait for NFA_P2P_CONN_REQ_EVT or NFA_NDEF_DATA_EVT when remote device requests connection.
    SyncEventGuard guard(mConnRequestEvent);
    NCI_DEBUG("serverHandle: %u; connHandle: %u; wait for incoming connection",
              aServerHandle, aConnHandle);
    mConnRequestEvent.Wait();
    NCI_DEBUG("serverHandle: %u; connHandle: %u; nfa conn h: 0x%X; got incoming connection",
              aServerHandle, aConnHandle, connection->mNfaConnHandle);
  }

  if (connection->mNfaConnHandle == NFA_HANDLE_INVALID) {
    RemoveServerConnection(aConnHandle);
    NCI_DEBUG("no handle assigned");
    return false;
  }

  NCI_DEBUG("serverHandle: %u; connHandle: %u; nfa conn h: 0x%X; try accept",
            aServerHandle, aConnHandle, connection->mNfaConnHandle);
  nfaStat = NFA_P2pAcceptConn(connection->mNfaConnHandle, aMaxInfoUnit, aRecvWindow);

  if (nfaStat != NFA_STATUS_OK) {
    NCI_ERROR("fail to accept remote; error=0x%X", nfaStat);
    return false;
  }

  NCI_DEBUG("exit; serverHandle: %u; connHandle: %u; nfa conn h: 0x%X",
            aServerHandle, aConnHandle, connection->mNfaConnHandle);
  return true;
}

void P2pServer::UnblockAll()
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < MAX_NFA_CONNS_PER_SERVER; i++) {
    if (mServerConn[i] != NULL) {
      mServerConn[i]->mNfaConnHandle = NFA_HANDLE_INVALID;
      {
        SyncEventGuard guard1(mServerConn[i]->mCongEvent);
        mServerConn[i]->mCongEvent.NotifyOne(); // Unblock write (if congested).
      }
      {
        SyncEventGuard guard2(mServerConn[i]->mReadEvent);
        mServerConn[i]->mReadEvent.NotifyOne(); // Unblock receive().
      }
    }
  }
}

sp<NfaConn> P2pServer::AllocateConnection(unsigned int aHandle)
{
  AutoMutex mutex(mMutex);
  // First, find a free connection block to handle the connection.
  for (int i = 0; i < MAX_NFA_CONNS_PER_SERVER; i++) {
    if (mServerConn[i] == NULL) {
      mServerConn[i] = new NfaConn;
      mServerConn[i]->mHandle = aHandle;
      return mServerConn[i];
    }
  }

  return NULL;
}

sp<NfaConn> P2pServer::FindServerConnection(tNFA_HANDLE aNfaConnHandle)
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < MAX_NFA_CONNS_PER_SERVER; i++) {
    if ((mServerConn[i] != NULL) &&
        (mServerConn[i]->mNfaConnHandle == aNfaConnHandle) )
      return mServerConn[i];
  }

  // If here, not found.
  return NULL;
}

sp<NfaConn> P2pServer::FindServerConnection(unsigned int aHandle)
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < MAX_NFA_CONNS_PER_SERVER; i++) {
    if ((mServerConn[i] != NULL) && (mServerConn[i]->mHandle == aHandle))
      return (mServerConn[i]);
  }

  // If here, not found.
  return (NULL);
}

bool P2pServer::RemoveServerConnection(unsigned int aHandle)
{
  int i = 0;

  AutoMutex mutex(mMutex);
  for (i = 0; i < MAX_NFA_CONNS_PER_SERVER; i++) {
    if ((mServerConn[i] != NULL) && (mServerConn[i]->mHandle == aHandle) ) {
      mServerConn[i] = NULL;
      return true;
    }
  }

  // If here, not found.
  return false;
}

P2pClient::P2pClient()
 : mNfaP2pClientHandle(NFA_HANDLE_INVALID)
 , mIsConnecting(false)
{
  mClientConn = new NfaConn();
}

P2pClient::~P2pClient()
{
}

NfaConn::NfaConn()
 : mNfaConnHandle(NFA_HANDLE_INVALID)
 , mHandle(0)
 , mMaxInfoUnit(0)
 , mRecvWindow(0)
 , mRemoteMaxInfoUnit(0)
 , mRemoteRecvWindow(0)
{
}
