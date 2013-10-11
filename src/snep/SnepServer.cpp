#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

// Well-known LLCP SAP Values defined by NFC forum.
const char* SnepServer::DEFAULT_SERVICE_NAME = "urn:nfc:sn:snep";

SnepCallback::SnepCallback()
{
}

SnepCallback::~SnepCallback()
{
}

SnepMessage* SnepCallback::doPut(NdefMessage* ndef)
{
  if (!ndef) {
    ALOGE("%s: invalid parameter", __FUNCTION__);
    return NULL;
  }
  // TODO : We should send the ndef message to gecko
  // onReceiveComplete(msg);
  return SnepMessage::getMessage(SnepMessage::RESPONSE_SUCCESS);
}

// The NFC Forum Default SNEP server is not allowed to respond to
// SNEP GET requests - see SNEP 1.0 TS section 6.1. However,
// since Android 4.1 used the NFC Forum default server to
// implement connection handover, we will support this
// until we can deprecate it.
SnepMessage* SnepCallback::doGet(int acceptableLength, NdefMessage* ndef)
{
  if (!ndef) {
    ALOGE("%s: invalid parameter", __FUNCTION__);
    return NULL;
  }
  
  /**
   * Response Codes : NOT IMPLEMENTED
   * The server does not support the functionality required to fulfill
   * the request.
   */
  return SnepMessage::getMessage(SnepMessage::RESPONSE_NOT_IMPLEMENTED);
}

// Connection thread, used to handle incoming connections.
void* SnepConnectionThreadFunc(void* arg)
{
  ALOGD("%s: connection thread enter", __FUNCTION__);

  SnepConnectionThread* pConnectionThread = reinterpret_cast<SnepConnectionThread*>(arg);
  if (!pConnectionThread) {
    ALOGE("%s: invalid parameter", __FUNCTION__);
    return NULL;
  }

  ISnepCallback* ICallback = pConnectionThread->mCallback;
  while(pConnectionThread->isServerRunning()) {
    // Handle message.
    if (!SnepServer::handleRequest(pConnectionThread->mMessenger, ICallback)) {
      break;
    }
  }

  if (pConnectionThread->mSock)
    pConnectionThread->mSock->close();

  // TODO : is this correct ??
  delete pConnectionThread;

  ALOGD("%s: connection thread exit", __FUNCTION__);
  return NULL;
}

/**
 * Connection thread is created when Snep server accept a connection request.
 */
SnepConnectionThread::SnepConnectionThread(
  SnepServer* server,ILlcpSocket* socket, int fragmentLength, ISnepCallback* ICallback)
 : mSock(socket)
 , mCallback(ICallback)
 , mServer(server)
{
  mMessenger = new SnepMessenger(false, socket, fragmentLength);
}

SnepConnectionThread::~SnepConnectionThread()
{
  delete mMessenger;
}

void SnepConnectionThread::run()
{
  pthread_t tid;
  if(pthread_create(&tid, NULL, SnepConnectionThreadFunc, this) != 0) {
    ALOGE("%s: pthread_create fail", __FUNCTION__);
    abort();
  }
}

bool SnepConnectionThread::isServerRunning() const
{
  return mServer->mServerRunning;
}

/**
 * Server thread, used to listen for incoming connection request.
 */
void* snepServerThreadFunc(void* arg)
{
  SnepServer* pSnepServer = reinterpret_cast<SnepServer*>(arg);
  if (!pSnepServer) {
    ALOGE("%s: invalid parameter", __FUNCTION__);
    return NULL;
  }

  ILlcpServerSocket* serverSocket = pSnepServer->mServerSocket;
  ISnepCallback* ICallback = pSnepServer->mCallback;
  const int fragmentLength = pSnepServer->mFragmentLength;

  if (!serverSocket) {
    ALOGE("%s: no server socket", __FUNCTION__);
    return NULL;
  }

  while(pSnepServer->mServerRunning) {
    if (!serverSocket) {
      ALOGD("%s: server socket shut down", __FUNCTION__);
      return NULL;
    }

    ILlcpSocket* communicationSocket = serverSocket->accept();

    if (communicationSocket) {
      const int miu = communicationSocket->getRemoteMiu();
      const int length = (fragmentLength == -1) ? miu : miu < fragmentLength ? miu : fragmentLength;

      SnepConnectionThread* pConnectionThread =
          new SnepConnectionThread(pSnepServer, communicationSocket, length, ICallback);
      pConnectionThread->run();
    }
  }

  return NULL;
}

SnepServer::SnepServer(ISnepCallback* ICallback) {
  mCallback = ICallback;
  mServiceName = DEFAULT_SERVICE_NAME;
  mServiceSap = DEFAULT_PORT;
  mFragmentLength = -1;
  mMiu = DEFAULT_MIU;
  mRwSize = DEFAULT_RW_SIZE;
}

SnepServer::SnepServer(const char* serviceName, int serviceSap, ISnepCallback* ICallback) {
  mCallback = ICallback;
  mServiceName = serviceName;
  mServiceSap = serviceSap;
  mFragmentLength = -1;
  mMiu = DEFAULT_MIU;
  mRwSize = DEFAULT_RW_SIZE;
}

SnepServer::SnepServer(ISnepCallback* ICallback, int miu, int rwSize) {
  mCallback = ICallback;
  mServiceName = DEFAULT_SERVICE_NAME;
  mServiceSap = DEFAULT_PORT;
  mFragmentLength = -1;
  mMiu = miu;
  mRwSize = rwSize;
}

SnepServer::SnepServer(const char* serviceName, int serviceSap, int fragmentLength, ISnepCallback* ICallback) {
  mCallback = ICallback;
  mServiceName = serviceName;
  mServiceSap = serviceSap;
  mFragmentLength = fragmentLength;
  mMiu = DEFAULT_MIU;
  mRwSize = DEFAULT_RW_SIZE;
}

SnepServer::~SnepServer()
{
}

void SnepServer::start()
{
  ALOGD("%s: enter", __FUNCTION__);

  INfcManager* pINfcManager = NfcService::getNfcManager();
  mServerSocket = pINfcManager->createLlcpServerSocket(mServiceSap, mServiceName, mMiu, mRwSize, 1024);

  if (!mServerSocket) {
    ALOGE("%s: cannot create llcp server socket", __FUNCTION__);
    abort();
  }

  pthread_t tid;
  if(pthread_create(&tid, NULL, snepServerThreadFunc, this) != 0)
  {
    ALOGE("%s: pthread_create failed", __FUNCTION__);
    abort();
  }
  mServerRunning = true;

  ALOGD("%s: exit", __FUNCTION__);
}

void SnepServer::stop()
{
  // TODO : need to kill thread here
  mServerSocket->close();
  mServerRunning = false;

  // Use pthread_join here to make sure all thread is finished ?
}

bool SnepServer::handleRequest(SnepMessenger* messenger, ISnepCallback* callback)
{
  if (!messenger || !callback) {
    ALOGE("%s:: invalid parameter", __FUNCTION__);
    return false;
  }

  SnepMessage* request = messenger->getMessage();
  SnepMessage* response = NULL;

  if (!request) {
    /**
     * Response Codes : BAD REQUEST
     * The request could not be understood by the server due to malformed syntax.
     */
    ALOGE("%s: bad snep message", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST);
    if (response) {
      messenger->sendMessage(*response);
      delete response;
    }
    return false;
  }

  if (((request->getVersion() & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    /**
     * Response Codes : UNSUPPORTED VERSION
     * The server does not support, or refuses to support, the SNEP protocol
     * version that was used in the request message.
     */
    ALOGE("%s: unsupported version", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_UNSUPPORTED_VERSION);

  } else if (request->getField() == SnepMessage::REQUEST_GET) {
    NdefMessage* ndef = request->getNdefMessage();
    response = callback->doGet(request->getAcceptableLength(), ndef);

  } else if (request->getField() == SnepMessage::REQUEST_PUT) {
    NdefMessage* ndef = request->getNdefMessage();
    response = callback->doPut(ndef);

  } else {
    ALOGE("%s: bad request", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST);
  }

  delete request;
  if (response) {
    messenger->sendMessage(*response);
    delete response;
  } else {
    ALOGE("%s: no response message is generated", __FUNCTION__);
    return false;
  }

  return true;
}
