/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Communicate with a peer using NFC-DEP, LLCP, SNEP.
 */
#include "PeerToPeer.h"

#include "NfcManager.h"
#include "NfcUtil.h"
#include "llcp_defs.h"
#include "config.h"
#include "IP2pDevice.h"
#include "NfcTagManager.h"

#undef LOG_TAG
#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

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

PeerToPeer& PeerToPeer::getInstance()
{
  return sP2p;
}

void PeerToPeer::initialize(NfcManager* pNfcManager)
{
  unsigned long num = 0;

  mNfcManager = pNfcManager;

  if (GetNumValue("P2P_LISTEN_TECH_MASK", &num, sizeof(num)))
    mP2pListenTechMask = num;
}

sp<P2pServer> PeerToPeer::findServerLocked(tNFA_HANDLE nfaP2pServerHandle)
{
  for (int i = 0; i < sMax; i++) {
    if ((mServers[i] != NULL)
      && (mServers[i]->mNfaP2pServerHandle == nfaP2pServerHandle) ) {
      return mServers [i];
    }
  }

  // If here, not found.
  return NULL;
}

sp<P2pServer> PeerToPeer::findServerLocked(unsigned int handle)
{
  for (int i = 0; i < sMax; i++) {
    if (( mServers[i] != NULL)
      && (mServers[i]->mHandle == handle) ) {
      return mServers [i];
    }
  }

  // If here, not found.
  return NULL;
}

sp<P2pServer> PeerToPeer::findServerLocked(const char *serviceName)
{
  for (int i = 0; i < sMax; i++) {
    if ((mServers[i] != NULL) && (mServers[i]->mServiceName.compare(serviceName) == 0) )
      return mServers [i];
  }

  // If here, not found.
  return NULL;
}

bool PeerToPeer::registerServer(unsigned int handle, const char *serviceName)
{
  static const char fn [] = "PeerToPeer::registerServer";
  ALOGD("%s: enter; service name: %s  handle: %u", fn, serviceName, handle);
  sp<P2pServer>   pSrv = NULL;

  mMutex.lock();
  // Check if already registered.
  if ((pSrv = findServerLocked(serviceName)) != NULL) {
    ALOGD("%s: service name=%s  already registered, handle: 0x%04x", fn, serviceName, pSrv->mNfaP2pServerHandle);
    // Update handle.
    pSrv->mHandle = handle;
    mMutex.unlock();
    return true;
  }

  for (int ii = 0; ii < sMax; ii++) {
    if (mServers[ii] == NULL) {
      pSrv = mServers[ii] = new P2pServer(handle, serviceName);
      ALOGD("%s: added new p2p server  index: %d  handle: %u  name: %s", fn, ii, handle, serviceName);
      break;
    }
  }
  mMutex.unlock();

  if (pSrv == NULL) {
    ALOGE("%s: service name=%s  no free entry", fn, serviceName);
    return false;
  }

  if (pSrv->registerWithStack()) {
    ALOGD("%s: got new p2p server h=0x%X", fn, pSrv->mNfaP2pServerHandle);
    return true;
  } else {
    ALOGE("%s: invalid server handle", fn);
    removeServer(handle);
    return false;
  }
}

void PeerToPeer::removeServer(unsigned int handle)
{
  static const char fn [] = "PeerToPeer::removeServer";

  AutoMutex mutex(mMutex);

  for (int i = 0; i < sMax; i++) {
    if ((mServers[i] != NULL) && (mServers[i]->mHandle == handle) ) {
      ALOGD("%s: server handle: %u;  nfa_handle: 0x%04x; name: %s; index=%d",
        fn, handle, mServers[i]->mNfaP2pServerHandle, mServers[i]->mServiceName.c_str(), i);
      mServers[i] = NULL;
      return;
    }
  }
  ALOGE("%s: unknown server handle: %u", fn, handle);
}

void PeerToPeer::llcpActivatedHandler(tNFA_LLCP_ACTIVATED& activated)
{
  static const char fn [] = "PeerToPeer::llcpActivatedHandler";
  ALOGD("%s: enter", fn);

  IP2pDevice* pIP2pDevice = 
    reinterpret_cast<IP2pDevice*>(mNfcManager->queryInterface(INTERFACE_P2P_DEVICE));

  if (pIP2pDevice == NULL) {
    ALOGE("%s : cannot get p2p device class", fn);
    return;
  }
    
  // No longer need to receive NDEF message from a tag.
  NfcTagManager::doDeregisterNdefTypeHandler();

  if (activated.is_initiator == true) {
    ALOGD("%s: p2p initiator", fn);
    pIP2pDevice->getMode() = NfcDepEndpoint::MODE_P2P_INITIATOR;
  } else {
    ALOGD("%s: p2p target", fn);
    pIP2pDevice->getMode() = NfcDepEndpoint::MODE_P2P_TARGET;
  }

  pIP2pDevice->getHandle() = 0x1234;

  mNfcManager->notifyLlcpLinkActivated(pIP2pDevice);

  ALOGD("%s: exit", fn);
}

void PeerToPeer::llcpDeactivatedHandler(tNFA_LLCP_DEACTIVATED& /*deactivated*/)
{
  static const char fn [] = "PeerToPeer::llcpDeactivatedHandler";
  ALOGD("%s: enter", fn);

  IP2pDevice* pIP2pDevice =
    reinterpret_cast<IP2pDevice*>(mNfcManager->queryInterface(INTERFACE_P2P_DEVICE));

  if (pIP2pDevice == NULL) {
    ALOGE("%s : cannot get p2p device class", fn);
    return;
  }

  mNfcManager->notifyLlcpLinkDeactivated(pIP2pDevice);

  NfcTagManager::doRegisterNdefTypeHandler();
  ALOGD("%s: exit", fn);
}

void PeerToPeer::llcpFirstPacketHandler()
{
  static const char fn [] = "PeerToPeer::llcpFirstPacketHandler";
  ALOGD("%s: enter", fn);

  mNfcManager->notifyLlcpLinkFirstPacketReceived();

  ALOGD("%s: exit", fn);
}

bool PeerToPeer::accept(unsigned int serverHandle, unsigned int connHandle, int maxInfoUnit, int recvWindow)
{
  static const char fn [] = "PeerToPeer::accept";
  sp<P2pServer> pSrv = NULL;

  ALOGD("%s: enter; server handle: %u; conn handle: %u; maxInfoUnit: %d; recvWindow: %d", fn,
    serverHandle, connHandle, maxInfoUnit, recvWindow);

  mMutex.lock();
  if ((pSrv = findServerLocked(serverHandle)) == NULL) {
    ALOGE("%s: unknown server handle: %u", fn, serverHandle);
    mMutex.unlock();
    return false;
  }
  mMutex.unlock();

  return pSrv->accept(serverHandle, connHandle, maxInfoUnit, recvWindow);
}

bool PeerToPeer::deregisterServer(unsigned int handle)
{
  static const char fn [] = "PeerToPeer::deregisterServer";
  ALOGD("%s: enter; handle: %u", fn, handle);
  tNFA_STATUS     nfaStat = NFA_STATUS_FAILED;
  sp<P2pServer>   pSrv = NULL;

  mMutex.lock();
  if ((pSrv = findServerLocked(handle)) == NULL) {
    ALOGE("%s: unknown service handle: %u", fn, handle);
    mMutex.unlock();
    return false;
  }
  mMutex.unlock();

  {
    // Server does not call NFA_P2pDisconnect(), so unblock the accept().
    SyncEventGuard guard(pSrv->mConnRequestEvent);
    pSrv->mConnRequestEvent.notifyOne();
  }

  nfaStat = NFA_P2pDeregister(pSrv->mNfaP2pServerHandle);
  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("%s: deregister error=0x%X", fn, nfaStat);
  }

  removeServer(handle);

  ALOGD("%s: exit", fn);
  return true;
}

bool PeerToPeer::createClient(unsigned int handle, UINT16 miu, UINT8 rw)
{
  static const char fn [] = "PeerToPeer::createClient";
  int i = 0;
  ALOGD("%s: enter: h: %u  miu: %u  rw: %u", fn, handle, miu, rw);

  mMutex.lock();
  sp<P2pClient> client = NULL;
  for (i = 0; i < sMax; i++) {
    if (mClients[i] == NULL) {
      mClients[i] = client = new P2pClient();

      mClients[i]->mClientConn->mHandle   = handle;
      mClients[i]->mClientConn->mMaxInfoUnit = miu;
      mClients[i]->mClientConn->mRecvWindow  = rw;
      break;
    }
  }
  mMutex.unlock();

  if (client == NULL) {
    ALOGE("%s: fail", fn);
    return false;
  }

  ALOGD("%s: pClient: 0x%p  assigned for client handle: %u", fn, client.get(), handle);

  {
    SyncEventGuard guard(mClients[i]->mRegisteringEvent);
    NFA_P2pRegisterClient(NFA_P2P_DLINK_TYPE, nfaClientCallback);
    mClients[i]->mRegisteringEvent.wait(); // Wait for NFA_P2P_REG_CLIENT_EVT.
  }

  if (mClients[i]->mNfaP2pClientHandle != NFA_HANDLE_INVALID) {
    ALOGD("%s: exit; new client handle: %u   NFA Handle: 0x%04x", fn, handle, client->mClientConn->mNfaConnHandle);
    return true;
  } else {
    ALOGE("%s: FAILED; new client handle: %u   NFA Handle: 0x%04x", fn, handle, client->mClientConn->mNfaConnHandle);
    removeConn(handle);
    return false;
  }
}

void PeerToPeer::removeConn(unsigned int handle)
{
  static const char fn[] = "PeerToPeer::removeConn";

  AutoMutex mutex(mMutex);
  // If the connection is a for a client, delete the client itself.
  for (int ii = 0; ii < sMax; ii++) {
    if ((mClients[ii] != NULL) && (mClients[ii]->mClientConn->mHandle == handle)) {
      if (mClients[ii]->mNfaP2pClientHandle != NFA_HANDLE_INVALID)
        NFA_P2pDeregister(mClients[ii]->mNfaP2pClientHandle);

      mClients[ii] = NULL;
      ALOGD("%s: deleted client handle: %u  index: %u", fn, handle, ii);
      return;
    }
  }

  // If the connection is for a server, just delete the connection.
  for (int ii = 0; ii < sMax; ii++) {
    if (mServers[ii] != NULL) {
      if (mServers[ii]->removeServerConnection(handle)) {
        return;
      }
    }
  }

  ALOGE("%s: could not find handle: %u", fn, handle);
}

bool PeerToPeer::connectConnOriented(unsigned int handle, const char* serviceName)
{
  static const char fn [] = "PeerToPeer::connectConnOriented";
  ALOGD("%s: enter; h: %u  service name=%s", fn, handle, serviceName);
  bool stat = createDataLinkConn(handle, serviceName, 0);
  ALOGD("%s: exit; h: %u  stat: %u", fn, handle, stat);
  return stat;
}

bool PeerToPeer::connectConnOriented(unsigned int handle, UINT8 destinationSap)
{
  static const char fn [] = "PeerToPeer::connectConnOriented";
  ALOGD("%s: enter; h: %u  dest sap: 0x%X", fn, handle, destinationSap);
  bool stat = createDataLinkConn(handle, NULL, destinationSap);
  ALOGD("%s: exit; h: %u  stat: %u", fn, handle, stat);
  return stat;
}

bool PeerToPeer::createDataLinkConn(unsigned int handle, const char* serviceName, UINT8 destinationSap)
{
  static const char fn [] = "PeerToPeer::createDataLinkConn";
  ALOGD("%s: enter", fn);
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  sp<P2pClient>   pClient = NULL;

  if ((pClient = findClient(handle)) == NULL) {
    ALOGE("%s: can't find client, handle: %u", fn, handle);
    return false;
  }

  {
    SyncEventGuard guard(pClient->mConnectingEvent);
    pClient->mIsConnecting = true;

    if (serviceName)
      nfaStat = NFA_P2pConnectByName(pClient->mNfaP2pClientHandle,
                  const_cast<char*>(serviceName), pClient->mClientConn->mMaxInfoUnit,
                  pClient->mClientConn->mRecvWindow);
    else if (destinationSap)
      nfaStat = NFA_P2pConnectBySap(pClient->mNfaP2pClientHandle, destinationSap,
                  pClient->mClientConn->mMaxInfoUnit, pClient->mClientConn->mRecvWindow);

    if (nfaStat == NFA_STATUS_OK) {
      ALOGD("%s: wait for connected event  mConnectingEvent: 0x%p", fn, pClient.get());
      pClient->mConnectingEvent.wait();
    }
  }

  if (nfaStat == NFA_STATUS_OK) {
    if (pClient->mClientConn->mNfaConnHandle == NFA_HANDLE_INVALID) {
      removeConn(handle);
      nfaStat = NFA_STATUS_FAILED;
    } else {
      pClient->mIsConnecting = false;
    }
  } else {
    removeConn(handle);
    ALOGE("%s: fail; error=0x%X", fn, nfaStat);
  }

  ALOGD("%s: exit", fn);
  return nfaStat == NFA_STATUS_OK;
}

sp<P2pClient> PeerToPeer::findClient(tNFA_HANDLE nfaConnHandle)
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL) && (mClients[i]->mNfaP2pClientHandle == nfaConnHandle))
      return mClients[i];
  }
  return NULL;
}

sp<P2pClient> PeerToPeer::findClient(unsigned int handle)
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL) && (mClients[i]->mClientConn->mHandle == handle))
      return mClients[i];
  }
  return NULL;
}

sp<P2pClient> PeerToPeer::findClientCon(tNFA_HANDLE nfaConnHandle)
{
  AutoMutex mutex(mMutex);
  for (int i = 0; i < sMax; i++) {
    if ((mClients[i] != NULL) && (mClients[i]->mClientConn->mNfaConnHandle == nfaConnHandle))
      return mClients[i];
  }
  return NULL;
}

sp<NfaConn> PeerToPeer::findConnection(tNFA_HANDLE nfaConnHandle)
{
  AutoMutex mutex(mMutex);
  // First, look through all the client control blocks.
  for (int ii = 0; ii < sMax; ii++) {
    if ((mClients[ii] != NULL)
      &&(mClients[ii]->mClientConn->mNfaConnHandle == nfaConnHandle) ) {
      return mClients[ii]->mClientConn;
    }
  }

  // Not found yet. Look through all the server control blocks.
  for (int ii = 0; ii < sMax; ii++) {
    if (mServers[ii] != NULL) {
      sp<NfaConn> conn = mServers[ii]->findServerConnection(nfaConnHandle);
      if (conn != NULL) {
        return conn;
      }
    }
  }

  // Not found...
  return NULL;
}

sp<NfaConn> PeerToPeer::findConnection(unsigned int handle)
{
  AutoMutex mutex(mMutex);
  // First, look through all the client control blocks.
  for (int ii = 0; ii < sMax; ii++) {
    if ((mClients[ii] != NULL)
      && (mClients[ii]->mClientConn->mHandle == handle) ) {
      return mClients[ii]->mClientConn;
    }
  }

  // Not found yet. Look through all the server control blocks.
  for (int ii = 0; ii < sMax; ii++) {
    if (mServers[ii] != NULL) {
      sp<NfaConn> conn = mServers[ii]->findServerConnection(handle);
      if (conn != NULL) {
        return conn;
      }
    }
  }

  // Not found...
  return NULL;
}

bool PeerToPeer::send(unsigned int handle, UINT8 *buffer, UINT16 bufferLen)
{
  static const char fn [] = "PeerToPeer::send";
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  sp<NfaConn>     pConn =  NULL;

  if ((pConn = findConnection(handle)) == NULL) {
    ALOGE("%s: can't find connection handle: %u", fn, handle);
    return false;
  }

  ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: send data; handle: %u  nfaHandle: 0x%04X",
          fn, pConn->mHandle, pConn->mNfaConnHandle);

  while (true) {
    SyncEventGuard guard(pConn->mCongEvent);
    nfaStat = NFA_P2pSendData(pConn->mNfaConnHandle, bufferLen, buffer);
    if (nfaStat == NFA_STATUS_CONGESTED)
      pConn->mCongEvent.wait(); // Wait for NFA_P2P_CONGEST_EVT.
    else
      break;

    if (pConn->mNfaConnHandle == NFA_HANDLE_INVALID) { // Peer already disconnected.
      ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: peer disconnected", fn);
      return false;
    }
  }

  if (nfaStat == NFA_STATUS_OK)
    ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: exit OK; handle: %u  NFA Handle: 0x%04x", fn, handle, pConn->mNfaConnHandle);
  else
    ALOGE("%s: Data not sent; handle: %u  NFA Handle: 0x%04x  error: 0x%04x",
      fn, handle, pConn->mNfaConnHandle, nfaStat);

  return nfaStat == NFA_STATUS_OK;
}

bool PeerToPeer::receive(unsigned int handle, UINT8* buffer, UINT16 bufferLen, UINT16& actualLen)
{
  static const char fn [] = "PeerToPeer::receive";
  ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: enter; handle: %u  bufferLen: %u", fn, handle, bufferLen);
  sp<NfaConn> pConn = NULL;
  tNFA_STATUS stat = NFA_STATUS_FAILED;
  UINT32 actualDataLen2 = 0;
  BOOLEAN isMoreData = TRUE;
  bool retVal = false;

  if ((pConn = findConnection(handle)) == NULL) {
    ALOGE("%s: can't find connection handle: %u", fn, handle);
    return false;
  }

  ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: handle: %u  nfaHandle: 0x%04X  buf len=%u", fn, pConn->mHandle, pConn->mNfaConnHandle, bufferLen);

  while (pConn->mNfaConnHandle != NFA_HANDLE_INVALID) {
    // NFA_P2pReadData() is synchronous.
    stat = NFA_P2pReadData(pConn->mNfaConnHandle, bufferLen, &actualDataLen2, buffer, &isMoreData);
    if ((stat == NFA_STATUS_OK) && (actualDataLen2 > 0)) { // Received some data.
      actualLen = (UINT16) actualDataLen2;
      retVal = true;
      break;
    }
    ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: waiting for data...", fn);
    {
      SyncEventGuard guard(pConn->mReadEvent);
      pConn->mReadEvent.wait();
    }
  } // while.

  ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: exit; nfa h: 0x%X  ok: %u  actual len: %u", fn, pConn->mNfaConnHandle, retVal, actualLen);
  return retVal;
}

bool PeerToPeer::disconnectConnOriented(unsigned int handle)
{
  static const char fn [] = "PeerToPeer::disconnectConnOriented";
  tNFA_STATUS nfaStat = NFA_STATUS_FAILED;
  sp<P2pClient>   pClient = NULL;
  sp<NfaConn>     pConn = NULL;

  ALOGD("%s: enter; handle: %u", fn, handle);

  if ((pConn = findConnection(handle)) == NULL) {
    ALOGE("%s: can't find connection handle: %u", fn, handle);
    return false;
  }

  // If this is a client, he may not be connected yet, so unblock him just in case.
  if (((pClient = findClient(handle)) != NULL) && (pClient->mIsConnecting) ) {
    SyncEventGuard guard(pClient->mConnectingEvent);
    pClient->mConnectingEvent.notifyOne();
    return true;
  }

  {
    SyncEventGuard guard1(pConn->mCongEvent);
    pConn->mCongEvent.notifyOne(); // Unblock send() if congested.
  }
  {
    SyncEventGuard guard2(pConn->mReadEvent);
    pConn->mReadEvent.notifyOne(); // Unblock receive().
  }

  if (pConn->mNfaConnHandle != NFA_HANDLE_INVALID) {
    ALOGD("%s: try disconn nfa h=0x%04X", fn, pConn->mNfaConnHandle);
    SyncEventGuard guard (pConn->mDisconnectingEvent);
    nfaStat = NFA_P2pDisconnect(pConn->mNfaConnHandle, FALSE);

    if (nfaStat != NFA_STATUS_OK)
      ALOGE("%s: fail p2p disconnect", fn);
    else
      pConn->mDisconnectingEvent.wait();
  }

  mDisconnectMutex.lock();
  removeConn(handle);
  mDisconnectMutex.unlock();

  ALOGD("%s: exit; handle: %u", fn, handle);
  return nfaStat == NFA_STATUS_OK;
}

UINT16 PeerToPeer::getRemoteMaxInfoUnit(unsigned int handle)
{
  static const char fn [] = "PeerToPeer::getRemoteMaxInfoUnit";
  sp<NfaConn> pConn = NULL;

  if ((pConn = findConnection(handle)) == NULL) {
    ALOGE("%s: can't find client  handle: %u", fn, handle);
    return 0;
  }
  ALOGD("%s: handle: %u   MIU: %u", fn, handle, pConn->mRemoteMaxInfoUnit);
  return pConn->mRemoteMaxInfoUnit;
}

UINT8 PeerToPeer::getRemoteRecvWindow(unsigned int handle)
{
  static const char fn [] = "PeerToPeer::getRemoteRecvWindow";
  ALOGD("%s: client handle: %u", fn, handle);
  sp<NfaConn> pConn = NULL;

  if ((pConn = findConnection(handle)) == NULL) {
    ALOGE("%s: can't find client", fn);
    return 0;
  }
  return pConn->mRemoteRecvWindow;
}

void PeerToPeer::setP2pListenMask(tNFA_TECHNOLOGY_MASK p2pListenMask) 
{
  mP2pListenTechMask = p2pListenMask;
}

void PeerToPeer::enableP2pListening(bool isEnable)
{
  static const char    fn []   = "PeerToPeer::enableP2pListening";
  tNFA_STATUS          nfaStat = NFA_STATUS_FAILED;

  ALOGD("%s: enter isEnable: %u  mIsP2pListening: %u", fn, isEnable, mIsP2pListening);

  // If request to enable P2P listening, and we were not already listening.
  if ((isEnable == true) && (mIsP2pListening == false) && (mP2pListenTechMask != 0) ) {
    SyncEventGuard guard(mSetTechEvent);
    if ((nfaStat = NFA_SetP2pListenTech(mP2pListenTechMask)) == NFA_STATUS_OK) {
      mSetTechEvent.wait();
      mIsP2pListening = true;
    }
    else
      ALOGE("%s: fail enable listen; error=0x%X", fn, nfaStat);
  } else if ((isEnable == false) && (mIsP2pListening == true) ) {
    SyncEventGuard guard(mSetTechEvent);
    // Request to disable P2P listening, check if it was enabled.
    if ((nfaStat = NFA_SetP2pListenTech(0)) == NFA_STATUS_OK) {
      mSetTechEvent.wait();
      mIsP2pListening = false;
    } else {
      ALOGE("%s: fail disable listen; error=0x%X", fn, nfaStat);
    }
  }
  ALOGD("%s: exit; mIsP2pListening: %u", fn, mIsP2pListening);
}

void PeerToPeer::handleNfcOnOff(bool isOn)
{
  static const char fn [] = "PeerToPeer::handleNfcOnOff";
  ALOGD("%s: enter; is on=%u", fn, isOn);

  mIsP2pListening = false;            // In both cases, P2P will not be listening.

  AutoMutex mutex(mMutex);
  if (isOn) {
    // Start with no clients or servers.
    memset(mServers, 0, sizeof(mServers));
    memset(mClients, 0, sizeof(mClients));
  } else {
    // Disconnect through all the clients.
    for (int ii = 0; ii < sMax; ii++) {
      if (mClients[ii] != NULL) {
        if (mClients[ii]->mClientConn->mNfaConnHandle == NFA_HANDLE_INVALID) {
          SyncEventGuard guard(mClients[ii]->mConnectingEvent);
          mClients[ii]->mConnectingEvent.notifyOne();
        } else {
          mClients[ii]->mClientConn->mNfaConnHandle = NFA_HANDLE_INVALID;
          {
            SyncEventGuard guard1(mClients[ii]->mClientConn->mCongEvent);
            mClients[ii]->mClientConn->mCongEvent.notifyOne(); // Unblock send().
          }
          {
            SyncEventGuard guard2(mClients[ii]->mClientConn->mReadEvent);
            mClients[ii]->mClientConn->mReadEvent.notifyOne(); // Unblock receive().
          }
        }
      }
    } // Loop.

    // Now look through all the server control blocks.
    for (int ii = 0; ii < sMax; ii++) {
      if (mServers[ii] != NULL) {
        mServers[ii]->unblockAll();
      }
    } // Loop.

  }
  ALOGD("%s: exit", fn);
}

void PeerToPeer::nfaServerCallback(tNFA_P2P_EVT p2pEvent, tNFA_P2P_EVT_DATA* eventData)
{
  static const char fn [] = "PeerToPeer::nfaServerCallback";
  sp<P2pServer>   pSrv = NULL;
  sp<NfaConn>     pConn = NULL;

  ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: enter; event=0x%X", fn, p2pEvent);

  switch (p2pEvent) {
    case NFA_P2P_REG_SERVER_EVT:  // NFA_P2pRegisterServer() has started to listen.
      ALOGD("%s: NFA_P2P_REG_SERVER_EVT; handle: 0x%04x; service sap=0x%02x  name: %s", fn,
        eventData->reg_server.server_handle, eventData->reg_server.server_sap, eventData->reg_server.service_name);

      sP2p.mMutex.lock();
      pSrv = sP2p.findServerLocked(eventData->reg_server.service_name);
      sP2p.mMutex.unlock();
      if (pSrv == NULL) {
        ALOGE("%s: NFA_P2P_REG_SERVER_EVT for unknown service: %s", fn, eventData->reg_server.service_name);
      } else {
        SyncEventGuard guard(pSrv->mRegServerEvent);
        pSrv->mNfaP2pServerHandle = eventData->reg_server.server_handle;
        pSrv->mRegServerEvent.notifyOne(); // Unblock registerServer().
      }
      break;

    case NFA_P2P_ACTIVATED_EVT: // Remote device has activated.
      ALOGD("%s: NFA_P2P_ACTIVATED_EVT; handle: 0x%04x", fn, eventData->activated.handle);
      break;

    case NFA_P2P_DEACTIVATED_EVT:
      ALOGD("%s: NFA_P2P_DEACTIVATED_EVT; handle: 0x%04x", fn, eventData->activated.handle);
      break;

    case NFA_P2P_CONN_REQ_EVT:
      ALOGD("%s: NFA_P2P_CONN_REQ_EVT; nfa server h=0x%04x; nfa conn h=0x%04x; remote sap=0x%02x", fn,
        eventData->conn_req.server_handle, eventData->conn_req.conn_handle, eventData->conn_req.remote_sap);

      sP2p.mMutex.lock();
      pSrv = sP2p.findServerLocked(eventData->conn_req.server_handle);
      sP2p.mMutex.unlock();
      if (pSrv == NULL) {
        ALOGE("%s: NFA_P2P_CONN_REQ_EVT; unknown server h", fn);
        return;
      }
      ALOGD("%s: NFA_P2P_CONN_REQ_EVT; server h=%u", fn, pSrv->mHandle);

      // Look for a connection block that is waiting (handle invalid).
      if ((pConn = pSrv->findServerConnection((tNFA_HANDLE) NFA_HANDLE_INVALID)) == NULL) {
        ALOGE("%s: NFA_P2P_CONN_REQ_EVT; server not listening", fn);
      } else {
        SyncEventGuard guard(pSrv->mConnRequestEvent);
        pConn->mNfaConnHandle = eventData->conn_req.conn_handle;
        pConn->mRemoteMaxInfoUnit = eventData->conn_req.remote_miu;
        pConn->mRemoteRecvWindow = eventData->conn_req.remote_rw;
        ALOGD("%s: NFA_P2P_CONN_REQ_EVT; server h=%u; conn h=%u; notify conn req", fn, pSrv->mHandle, pConn->mHandle);
        pSrv->mConnRequestEvent.notifyOne(); // Unblock accept().
      }
      break;

    case NFA_P2P_CONNECTED_EVT:
      ALOGD("%s: NFA_P2P_CONNECTED_EVT; h=0x%x  remote sap=0x%X", fn,
      eventData->connected.client_handle, eventData->connected.remote_sap);
      break;

    case NFA_P2P_DISC_EVT:
      ALOGD("%s: NFA_P2P_DISC_EVT; h=0x%04x; reason=0x%X", fn, eventData->disc.handle, eventData->disc.reason);
      // Look for the connection block.
      if ((pConn = sP2p.findConnection(eventData->disc.handle)) == NULL) {
        ALOGE("%s: NFA_P2P_DISC_EVT: can't find conn for NFA handle: 0x%04x", fn, eventData->disc.handle);
      } else {
        sP2p.mDisconnectMutex.lock();
        pConn->mNfaConnHandle = NFA_HANDLE_INVALID;
        {
          ALOGD("%s: NFA_P2P_DISC_EVT; try guard disconn event", fn);
          SyncEventGuard guard3(pConn->mDisconnectingEvent);
          pConn->mDisconnectingEvent.notifyOne();
          ALOGD("%s: NFA_P2P_DISC_EVT; notified disconn event", fn);
        }
        {
          ALOGD("%s: NFA_P2P_DISC_EVT; try guard congest event", fn);
          SyncEventGuard guard1(pConn->mCongEvent);
          pConn->mCongEvent.notifyOne(); // Unblock write (if congested).
          ALOGD("%s: NFA_P2P_DISC_EVT; notified congest event", fn);
        }
        {
          ALOGD("%s: NFA_P2P_DISC_EVT; try guard read event", fn);
          SyncEventGuard guard2(pConn->mReadEvent);
          pConn->mReadEvent.notifyOne(); // Unblock receive().
          ALOGD("%s: NFA_P2P_DISC_EVT; notified read event", fn);
        }
        sP2p.mDisconnectMutex.unlock();
      }
      break;

    case NFA_P2P_DATA_EVT:
      // Look for the connection block.
      if ((pConn = sP2p.findConnection(eventData->data.handle)) == NULL) {
        ALOGE("%s: NFA_P2P_DATA_EVT: can't find conn for NFA handle: 0x%04x", fn, eventData->data.handle);
      } else {
        ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: NFA_P2P_DATA_EVT; h=0x%X; remote sap=0x%X", fn,
                    eventData->data.handle, eventData->data.remote_sap);
        SyncEventGuard guard(pConn->mReadEvent);
        pConn->mReadEvent.notifyOne();
      }
      break;

    case NFA_P2P_CONGEST_EVT:
      // Look for the connection block.
      if ((pConn = sP2p.findConnection(eventData->congest.handle)) == NULL) {
        ALOGE("%s: NFA_P2P_CONGEST_EVT: can't find conn for NFA handle: 0x%04x", fn, eventData->congest.handle);
      } else {
        ALOGD("%s: NFA_P2P_CONGEST_EVT; nfa handle: 0x%04x  congested: %u", fn,
          eventData->congest.handle, eventData->congest.is_congested);
        if (eventData->congest.is_congested == FALSE) {
          SyncEventGuard guard(pConn->mCongEvent);
          pConn->mCongEvent.notifyOne();
        }
      }
      break;

    default:
      ALOGE("%s: unknown event 0x%X ????", fn, p2pEvent);
      break;
    }
    ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: exit", fn);
}

void PeerToPeer::nfaClientCallback(tNFA_P2P_EVT p2pEvent, tNFA_P2P_EVT_DATA* eventData)
{
  static const char fn [] = "PeerToPeer::nfaClientCallback";
  sp<NfaConn>     pConn = NULL;
  sp<P2pClient>   pClient = NULL;

  ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: enter; event=%u", fn, p2pEvent);

  switch (p2pEvent) {
    case NFA_P2P_REG_CLIENT_EVT:
      // Look for a client that is trying to register.
      if ((pClient = sP2p.findClient((tNFA_HANDLE)NFA_HANDLE_INVALID)) == NULL) {
        ALOGE("%s: NFA_P2P_REG_CLIENT_EVT: can't find waiting client", fn);
      } else {
        ALOGD("%s: NFA_P2P_REG_CLIENT_EVT; Conn Handle: 0x%04x, pClient: 0x%p", fn, eventData->reg_client.client_handle, pClient.get());

        SyncEventGuard guard(pClient->mRegisteringEvent);
        pClient->mNfaP2pClientHandle = eventData->reg_client.client_handle;
        pClient->mRegisteringEvent.notifyOne();
      }
      break;

    case NFA_P2P_ACTIVATED_EVT:
      // Look for a client that is trying to register.
      if ((pClient = sP2p.findClient(eventData->activated.handle)) == NULL) {
        ALOGE("%s: NFA_P2P_ACTIVATED_EVT: can't find client", fn);
      } else {
        ALOGD("%s: NFA_P2P_ACTIVATED_EVT; Conn Handle: 0x%04x, pClient: 0x%p", fn, eventData->activated.handle, pClient.get());
      }
      break;

    case NFA_P2P_DEACTIVATED_EVT:
      ALOGD("%s: NFA_P2P_DEACTIVATED_EVT: conn handle: 0x%X", fn, eventData->deactivated.handle);
      break;

    case NFA_P2P_CONNECTED_EVT:
      // Look for the client that is trying to connect.
      if ((pClient = sP2p.findClient(eventData->connected.client_handle)) == NULL) {
        ALOGE("%s: NFA_P2P_CONNECTED_EVT: can't find client: 0x%04x", fn, eventData->connected.client_handle);
      } else {
        ALOGD("%s: NFA_P2P_CONNECTED_EVT; client_handle=0x%04x  conn_handle: 0x%04x  remote sap=0x%X  pClient: 0x%p", fn,
        eventData->connected.client_handle, eventData->connected.conn_handle, eventData->connected.remote_sap, pClient.get());

        SyncEventGuard guard(pClient->mConnectingEvent);
        pClient->mClientConn->mNfaConnHandle     = eventData->connected.conn_handle;
        pClient->mClientConn->mRemoteMaxInfoUnit = eventData->connected.remote_miu;
        pClient->mClientConn->mRemoteRecvWindow  = eventData->connected.remote_rw;
        pClient->mConnectingEvent.notifyOne(); // Unblock createDataLinkConn().
      }
      break;

    case NFA_P2P_DISC_EVT:
      ALOGD("%s: NFA_P2P_DISC_EVT; h=0x%04x; reason=0x%X", fn, eventData->disc.handle, eventData->disc.reason);
      // Look for the connection block.
      if ((pConn = sP2p.findConnection(eventData->disc.handle)) == NULL) {
        // If no connection, may be a client that is trying to connect.
        if ((pClient = sP2p.findClient(eventData->disc.handle)) == NULL) {
          ALOGE("%s: NFA_P2P_DISC_EVT: can't find client for NFA handle: 0x%04x", fn, eventData->disc.handle);
          return;
        }
        // Unblock createDataLinkConn().
        SyncEventGuard guard(pClient->mConnectingEvent);
        pClient->mConnectingEvent.notifyOne();
      } else {
        sP2p.mDisconnectMutex.lock();
        pConn->mNfaConnHandle = NFA_HANDLE_INVALID;
        {
          ALOGD("%s: NFA_P2P_DISC_EVT; try guard disconn event", fn);
          SyncEventGuard guard3(pConn->mDisconnectingEvent);
          pConn->mDisconnectingEvent.notifyOne();
          ALOGD("%s: NFA_P2P_DISC_EVT; notified disconn event", fn);
        }
        {
          ALOGD("%s: NFA_P2P_DISC_EVT; try guard congest event", fn);
          SyncEventGuard guard1(pConn->mCongEvent);
          pConn->mCongEvent.notifyOne(); // Unblock write (if congested).
          ALOGD("%s: NFA_P2P_DISC_EVT; notified congest event", fn);
        }
        {
          ALOGD("%s: NFA_P2P_DISC_EVT; try guard read event", fn);
          SyncEventGuard guard2(pConn->mReadEvent);
          pConn->mReadEvent.notifyOne(); // Unblock receive().
          ALOGD("%s: NFA_P2P_DISC_EVT; notified read event", fn);
        }
        sP2p.mDisconnectMutex.unlock();
      }
      break;

    case NFA_P2P_DATA_EVT:
      // Look for the connection block.
      if ((pConn = sP2p.findConnection(eventData->data.handle)) == NULL) {
        ALOGE("%s: NFA_P2P_DATA_EVT: can't find conn for NFA handle: 0x%04x", fn, eventData->data.handle);
      } else {
        ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: NFA_P2P_DATA_EVT; h=0x%X; remote sap=0x%X", fn,
          eventData->data.handle, eventData->data.remote_sap);
        SyncEventGuard guard(pConn->mReadEvent);
        pConn->mReadEvent.notifyOne();
      }
      break;

    case NFA_P2P_CONGEST_EVT:
      // Look for the connection block.
      if ((pConn = sP2p.findConnection(eventData->congest.handle)) == NULL) {
        ALOGE("%s: NFA_P2P_CONGEST_EVT: can't find conn for NFA handle: 0x%04x", fn, eventData->congest.handle);
      } else {
        ALOGD_IF((appl_trace_level>=BT_TRACE_LEVEL_DEBUG), "%s: NFA_P2P_CONGEST_EVT; nfa handle: 0x%04x  congested: %u", fn,
        eventData->congest.handle, eventData->congest.is_congested);

        SyncEventGuard guard(pConn->mCongEvent);
        pConn->mCongEvent.notifyOne();
      }
      break;

    default:
      ALOGE("%s: unknown event 0x%X ????", fn, p2pEvent);
      break;
    }
}

void PeerToPeer::connectionEventHandler(UINT8 event, tNFA_CONN_EVT_DATA* /*eventData*/)
{
  switch (event) {
    case NFA_SET_P2P_LISTEN_TECH_EVT: {
      SyncEventGuard guard(mSetTechEvent);
      mSetTechEvent.notifyOne(); // Unblock NFA_SetP2pListenTech().
      break;
    }
  }
}

unsigned int PeerToPeer::getNewHandle()
{
  unsigned int newHandle = 0;

  mNewHandleMutex.lock();
  newHandle = mNextHandle++;
  mNewHandleMutex.unlock();
  return newHandle;
}

P2pServer::P2pServer(unsigned int handle, const char* serviceName)
 : mNfaP2pServerHandle(NFA_HANDLE_INVALID)
 , mHandle(handle)
{
  mServiceName.assign(serviceName);

  memset(mServerConn, 0, sizeof(mServerConn));
}

bool P2pServer::registerWithStack()
{
  static const char fn [] = "P2pServer::registerWithStack";
  ALOGD("%s: enter; service name: %s  handle: %u", fn, mServiceName.c_str(), mHandle);
  tNFA_STATUS     stat  = NFA_STATUS_OK;
  UINT8           serverSap = NFA_P2P_ANY_SAP;

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
    ALOGE("%s: fail set LLCP config; error=0x%X", fn, stat);

  if (sSnepServiceName.compare(mServiceName) == 0)
    serverSap = LLCP_SAP_SNEP; // LLCP_SAP_SNEP == 4.

  {
    SyncEventGuard guard(mRegServerEvent);
    stat = NFA_P2pRegisterServer(serverSap, NFA_P2P_DLINK_TYPE, const_cast<char*>(mServiceName.c_str()),
             PeerToPeer::nfaServerCallback);
    if (stat != NFA_STATUS_OK) {
      ALOGE("%s: fail register p2p server; error=0x%X", fn, stat);
      return false;
    }
    ALOGD("%s: wait for listen-completion event", fn);
    // Wait for NFA_P2P_REG_SERVER_EVT.
    mRegServerEvent.wait();
  }

  return (mNfaP2pServerHandle != NFA_HANDLE_INVALID);
}

bool P2pServer::accept(unsigned int serverHandle, unsigned int connHandle,
        int maxInfoUnit, int recvWindow)
{
  static const char fn [] = "P2pServer::accept";
  tNFA_STATUS     nfaStat  = NFA_STATUS_OK;

  sp<NfaConn> connection = allocateConnection(connHandle);
  if (connection == NULL) {
    ALOGE("%s: failed to allocate new server connection", fn);
    return false;
  }

  {
    // Wait for NFA_P2P_CONN_REQ_EVT or NFA_NDEF_DATA_EVT when remote device requests connection.
    SyncEventGuard guard(mConnRequestEvent);
    ALOGD("%s: serverHandle: %u; connHandle: %u; wait for incoming connection", fn,
      serverHandle, connHandle);
    mConnRequestEvent.wait();
    ALOGD("%s: serverHandle: %u; connHandle: %u; nfa conn h: 0x%X; got incoming connection", fn,
      serverHandle, connHandle, connection->mNfaConnHandle);
  }

  if (connection->mNfaConnHandle == NFA_HANDLE_INVALID) {
    removeServerConnection(connHandle);
    ALOGD("%s: no handle assigned", fn);
    return false;
  }

  ALOGD("%s: serverHandle: %u; connHandle: %u; nfa conn h: 0x%X; try accept", fn,
    serverHandle, connHandle, connection->mNfaConnHandle);
  nfaStat = NFA_P2pAcceptConn(connection->mNfaConnHandle, maxInfoUnit, recvWindow);

  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("%s: fail to accept remote; error=0x%X", fn, nfaStat);
    return false;
  }

  ALOGD("%s: exit; serverHandle: %u; connHandle: %u; nfa conn h: 0x%X", fn,
    serverHandle, connHandle, connection->mNfaConnHandle);
  return true;
}

void P2pServer::unblockAll()
{
  AutoMutex mutex(mMutex);
  for (int jj = 0; jj < MAX_NFA_CONNS_PER_SERVER; jj++) {
    if (mServerConn[jj] != NULL) {
      mServerConn[jj]->mNfaConnHandle = NFA_HANDLE_INVALID;
      {
        SyncEventGuard guard1(mServerConn[jj]->mCongEvent);
        mServerConn[jj]->mCongEvent.notifyOne(); // Unblock write (if congested).
      }
      {
        SyncEventGuard guard2(mServerConn[jj]->mReadEvent);
        mServerConn[jj]->mReadEvent.notifyOne(); // Unblock receive().
      }
    }
  }
}

sp<NfaConn> P2pServer::allocateConnection(unsigned int handle)
{
  AutoMutex mutex(mMutex);
  // First, find a free connection block to handle the connection.
  for (int ii = 0; ii < MAX_NFA_CONNS_PER_SERVER; ii++) {
    if (mServerConn[ii] == NULL) {
      mServerConn[ii] = new NfaConn;
      mServerConn[ii]->mHandle = handle;
      return mServerConn[ii];
    }
  }

  return NULL;
}

sp<NfaConn> P2pServer::findServerConnection(tNFA_HANDLE nfaConnHandle)
{
  int jj = 0;

  AutoMutex mutex(mMutex);
  for (jj = 0; jj < MAX_NFA_CONNS_PER_SERVER; jj++) {
    if ((mServerConn[jj] != NULL) && (mServerConn[jj]->mNfaConnHandle == nfaConnHandle) )
      return mServerConn[jj];
  }

  // If here, not found.
  return NULL;
}

sp<NfaConn> P2pServer::findServerConnection(unsigned int handle)
{
  int jj = 0;

  AutoMutex mutex(mMutex);
  for (jj = 0; jj < MAX_NFA_CONNS_PER_SERVER; jj++) {
    if ((mServerConn[jj] != NULL) && (mServerConn[jj]->mHandle == handle) )
      return (mServerConn[jj]);
  }

  // If here, not found.
  return (NULL);
}

bool P2pServer::removeServerConnection(unsigned int handle)
{
  int jj = 0;

  AutoMutex mutex(mMutex);
  for (jj = 0; jj < MAX_NFA_CONNS_PER_SERVER; jj++) {
    if ((mServerConn[jj] != NULL) && (mServerConn[jj]->mHandle == handle) ) {
      mServerConn[jj] = NULL;
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
