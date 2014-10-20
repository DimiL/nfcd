#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"
#include "ISnepCallback.h"
#include "NfcDebug.h"

// Well-known LLCP SAP Values defined by NFC forum.
const char* SnepServer::DEFAULT_SERVICE_NAME = "urn:nfc:sn:snep";

// Connection thread, used to handle incoming connections.
void* SnepConnectionThreadFunc(void* arg)
{
  ALOGD("%s: connection thread enter", FUNC);

  SnepConnectionThread* pConnectionThread = reinterpret_cast<SnepConnectionThread*>(arg);
  if (!pConnectionThread) {
    ALOGE("%s: invalid parameter", FUNC);
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

  ALOGD("%s: connection thread exit", FUNC);
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
    ALOGE("%s: pthread_create fail", FUNC);
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
    ALOGE("%s: invalid parameter", FUNC);
    return NULL;
  }

  ILlcpServerSocket* serverSocket = pSnepServer->mServerSocket;
  ISnepCallback* ICallback = pSnepServer->mCallback;
  const int fragmentLength = pSnepServer->mFragmentLength;

  if (!serverSocket) {
    ALOGE("%s: no server socket", FUNC);
    return NULL;
  }

  while(pSnepServer->mServerRunning) {
    if (!serverSocket) {
      ALOGD("%s: server socket shut down", FUNC);
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

  if (serverSocket) {
    serverSocket->close();
    delete serverSocket;
  }

  return NULL;
}

SnepServer::SnepServer(ISnepCallback* ICallback)
 : mServerSocket(NULL)
 , mCallback(ICallback)
 , mServerRunning(false)
 , mServiceName(DEFAULT_SERVICE_NAME)
 , mServiceSap(DEFAULT_PORT)
 , mFragmentLength(-1)
 , mMiu(DEFAULT_MIU)
 , mRwSize(DEFAULT_RW_SIZE)
{
}

SnepServer::SnepServer(const char* serviceName, int serviceSap, ISnepCallback* ICallback)
 : mServerSocket(NULL)
 , mCallback(ICallback)
 , mServerRunning(false)
 , mServiceName(serviceName)
 , mServiceSap(serviceSap)
 , mFragmentLength(-1)
 , mMiu(DEFAULT_MIU)
 , mRwSize(DEFAULT_RW_SIZE)
{
}

SnepServer::SnepServer(ISnepCallback* ICallback, int miu, int rwSize)
 : mServerSocket(NULL)
 , mCallback(ICallback)
 , mServerRunning(false)
 , mServiceName(DEFAULT_SERVICE_NAME)
 , mServiceSap(DEFAULT_PORT)
 , mFragmentLength(-1)
 , mMiu(miu)
 , mRwSize(rwSize)
{
}

SnepServer::SnepServer(const char* serviceName, int serviceSap, int fragmentLength, ISnepCallback* ICallback)
 : mServerSocket(NULL)
 , mCallback(ICallback)
 , mServerRunning(false)
 , mServiceName(serviceName)
 , mServiceSap(serviceSap)
 , mFragmentLength(fragmentLength)
 , mMiu(DEFAULT_MIU)
 , mRwSize(DEFAULT_RW_SIZE)
{
}

SnepServer::~SnepServer()
{
  stop();
}

void SnepServer::start()
{
  ALOGD("%s: enter", FUNC);

  INfcManager* pINfcManager = NfcService::getNfcManager();
  mServerSocket = pINfcManager->createLlcpServerSocket(mServiceSap, mServiceName, mMiu, mRwSize, 1024);

  if (!mServerSocket) {
    ALOGE("%s: cannot create llcp server socket", FUNC);
    abort();
  }

  pthread_t tid;
  if(pthread_create(&tid, NULL, snepServerThreadFunc, this) != 0)
  {
    ALOGE("%s: pthread_create failed", FUNC);
    abort();
  }
  mServerRunning = true;

  ALOGD("%s: exit", FUNC);
}

void SnepServer::stop()
{
  mServerSocket = NULL;
  mServerRunning = false;

  // Use pthread_join here to make sure all thread is finished ?
}

bool SnepServer::handleRequest(SnepMessenger* messenger, ISnepCallback* callback)
{
  if (!messenger || !callback) {
    ALOGE("%s:: invalid parameter", FUNC);
    return false;
  }

  SnepMessage* request = messenger->getMessage();
  SnepMessage* response = NULL;

  if (!request) {
    /**
     * Response Codes : BAD REQUEST
     * The request could not be understood by the server due to malformed syntax.
     */
    ALOGE("%s: bad snep message", FUNC);
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
    ALOGE("%s: unsupported version", FUNC);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_UNSUPPORTED_VERSION);

  } else if (request->getField() == SnepMessage::REQUEST_GET) {
    NdefMessage* ndef = request->getNdefMessage();
    response = callback->doGet(request->getAcceptableLength(), ndef);

  } else if (request->getField() == SnepMessage::REQUEST_PUT) {
    NdefMessage* ndef = request->getNdefMessage();
    response = callback->doPut(ndef);

  } else {
    ALOGE("%s: bad request", FUNC);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST);
  }

  delete request;
  if (response) {
    messenger->sendMessage(*response);
    delete response;
  } else {
    ALOGE("%s: no response message is generated", FUNC);
    return false;
  }

  return true;
}
