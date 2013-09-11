#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

const char* SnepServer::DEFAULT_SERVICE_NAME = "urn:nfc:sn:snep";

void* connectionThreadFunc(void* arg)
{
  ALOGD("starting connection thread");

  bool running = true;
  ConnectionThread* pConnectionThread = reinterpret_cast<ConnectionThread*>(arg);
  if (pConnectionThread == NULL) {
    ALOGE("no connection thread class");
    return NULL;
  }

  ISnepCallback* callback = pConnectionThread->mCallback;
  while(running) {
    // Handle message
    if (!SnepServer::handleRequest(pConnectionThread->mMessenger, callback)) {
      break;
    }

    running = pConnectionThread->isServerRunning();
  }

  pConnectionThread->mSock->close();

  // TODO : is this correct ??
  delete pConnectionThread;

  ALOGD("finished connection thread");

  return NULL;
}

ConnectionThread::ConnectionThread(
  SnepServer* server,ILlcpSocket* socket, int fragmentLength, ISnepCallback* callback):
mServer(server),
mSock(socket),
mCallback(callback)
{
  mMessenger = new SnepMessenger(false, socket, fragmentLength);
}

ConnectionThread::~ConnectionThread()
{
  delete mMessenger;
}

void ConnectionThread::run()
{
  pthread_t tid;
  if(pthread_create(&tid, NULL, connectionThreadFunc, this) != 0) {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }
}

bool ConnectionThread::isServerRunning()
{
  return mServer->mServerRunning;
}

void* serverThreadFunc(void* arg)
{
  SnepServer* pSnepServer = reinterpret_cast<SnepServer*>(arg);
  if (pSnepServer == NULL) {
    ALOGE("no snep server class");
    return NULL;
  }

  ISnepCallback* callback = pSnepServer->mCallback;
  ILlcpServerSocket* serverSocket = pSnepServer->mServerSocket;
  int fragmentLength = pSnepServer->mFragmentLength;

  if (serverSocket == NULL) {
    ALOGE("no server socket");
    return NULL;
  }

  while(pSnepServer->mServerRunning) {
    if (serverSocket == NULL) {
        ALOGD("Server socket shut down.");
        return NULL;
    }

    ILlcpSocket* communicationSocket = serverSocket->accept();
    
    if (communicationSocket != NULL) {
      int miu = communicationSocket->getRemoteMiu();
      int length = (fragmentLength == -1) ? miu : miu < fragmentLength ? miu : fragmentLength;
      ConnectionThread* pConnectionThread = 
          new ConnectionThread(pSnepServer, communicationSocket, length, callback);
      pConnectionThread->run();
    }
  }

  return NULL;
}

SnepServer::SnepServer(ISnepCallback* callback) {
  mCallback = callback;
  mServiceName = DEFAULT_SERVICE_NAME;
  mServiceSap = DEFAULT_PORT;
  mFragmentLength = -1;
  mMiu = DEFAULT_MIU;
  mRwSize = DEFAULT_RW_SIZE;
}

SnepServer::SnepServer(const char* serviceName, int serviceSap, ISnepCallback* callback) {
  mCallback = callback;
  mServiceName = serviceName;
  mServiceSap = serviceSap;
  mFragmentLength = -1;
  mMiu = DEFAULT_MIU;
  mRwSize = DEFAULT_RW_SIZE;
}

SnepServer::SnepServer(ISnepCallback* callback, int miu, int rwSize) {
  mCallback = callback;
  mServiceName = DEFAULT_SERVICE_NAME;
  mServiceSap = DEFAULT_PORT;
  mFragmentLength = -1;
  mMiu = miu;
  mRwSize = rwSize;
}

SnepServer::SnepServer(const char* serviceName, int serviceSap, int fragmentLength, ISnepCallback* callback) {
  mCallback = callback;
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
  INfcManager* pINfcManager = NfcService::getNfcManager();
  mServerSocket = pINfcManager->createLlcpServerSocket(mServiceSap, mServiceName, mMiu, mRwSize, 1024);

  pthread_t tid;
  if(pthread_create(&tid, NULL, serverThreadFunc, this) != 0)
  {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }
  mServerRunning = true;
}

void SnepServer::stop()
{
  // TODO : need to kill thread here
  mServerSocket->close();
  mServerRunning = false;

  // use pthread_join here to make sure all thread is finished ?
}

bool SnepServer::handleRequest(SnepMessenger* messenger, ISnepCallback* callback)
{
  if (messenger == NULL || callback == NULL) {
    ALOGE("function %s incorrect parameter", __FUNCTION__);
    return false;
  }

  SnepMessage* request = messenger->getMessage();
  SnepMessage* response = NULL;

  if (request == NULL) {
    ALOGE("function %s : Bad snep message", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST);
    messenger->sendMessage(*response);
    return false;
  } 

  if (((request->getVersion() & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    ALOGE("function %s : Unsupported version", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_UNSUPPORTED_VERSION);
    messenger->sendMessage(*response);
  } else if (request->getField() == SnepMessage::REQUEST_GET) {
    response = callback->doGet(request->getAcceptableLength(), *request->getNdefMessage());
    messenger->sendMessage(*response);
  } else if (request->getField() == SnepMessage::REQUEST_PUT) {
    response = callback->doPut(*request->getNdefMessage());
    messenger->sendMessage(*response);
  } else {
    ALOGE("function %s : Bad request", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST);
    messenger->sendMessage(*response);
  }

  // TODO : check if we need to delete request here
  delete response;

  return true;
}
