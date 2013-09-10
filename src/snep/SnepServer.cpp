#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

const char* SnepServer::DEFAULT_SERVICE_NAME = "urn:nfc:sn:snep";

ISnepCallback*  gCallback;
const char*     gServiceName;
int             gServiceSap;
int             gFragmentLength;
int             gMiu;
int             gRwSize;

void* connectionThreadFunc(void* arg)
{
  bool running = true;
  ConnectionThread* pConnectionThread = reinterpret_cast<ConnectionThread*>(arg);

  while(running) {
    // Handle message
    if (!SnepServer::handleRequest(pConnectionThread->mMessenger, gCallback)) {
      break;
    }
  }

  return NULL;
}

ConnectionThread::ConnectionThread(ILlcpSocket* socket, int fragmentLength):
mSock(socket)
{
  mMessenger = new SnepMessenger(false, socket, fragmentLength);
}

void ConnectionThread::run()
{
  pthread_t tid;
  if(pthread_create(&tid, NULL, connectionThreadFunc, this) != 0) {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }
}

void* serverThreadFunc(void* arg)
{
  INfcManager* pINfcManager = NfcService::getNfcManager();
  ILlcpServerSocket* pServerSocket = pINfcManager->createLlcpServerSocket(
    gServiceSap, gServiceName, gMiu, gRwSize, 1024);

  while(true) {
    if (pServerSocket == NULL) {
        ALOGD("Server socket shut down.");
        return NULL;
    }

    ILlcpSocket* communicationSocket = pServerSocket->accept();
    
    if (communicationSocket != NULL) {
      int miu = communicationSocket->getRemoteMiu();
      // use math
      int fragmentLength = (gFragmentLength == -1) ? miu : miu < gFragmentLength ? miu : gFragmentLength;
      // use pthread
      pthread_t tid;
      if(pthread_create(&tid, NULL, connectionThreadFunc, NULL) != 0) {
        ALOGE("init_nfc_service pthread_create failed");
        abort();
      }
    }
  }

  return NULL;
}

SnepServer::SnepServer(ISnepCallback* callback) {
  gCallback = callback;
  gServiceName = DEFAULT_SERVICE_NAME;
  gServiceSap = DEFAULT_PORT;
  gFragmentLength = -1;
  gMiu = DEFAULT_MIU;
  gRwSize = DEFAULT_RW_SIZE;
}

SnepServer::SnepServer(const char* serviceName, int serviceSap, ISnepCallback* callback) {
  gCallback = callback;
  gServiceName = serviceName;
  gServiceSap = serviceSap;
  gFragmentLength = -1;
  gMiu = DEFAULT_MIU;
  gRwSize = DEFAULT_RW_SIZE;
}

SnepServer::SnepServer(ISnepCallback* callback, int miu, int rwSize) {
  gCallback = callback;
  gServiceName = DEFAULT_SERVICE_NAME;
  gServiceSap = DEFAULT_PORT;
  gFragmentLength = -1;
  gMiu = miu;
  gRwSize = rwSize;
}

SnepServer::SnepServer(const char* serviceName, int serviceSap, int fragmentLength, ISnepCallback* callback) {
  gCallback = callback;
  gServiceName = serviceName;
  gServiceSap = serviceSap;
  gFragmentLength = fragmentLength;
  gMiu = DEFAULT_MIU;
  gRwSize = DEFAULT_RW_SIZE;
}

SnepServer::~SnepServer()
{

}

void SnepServer::start()
{
  pthread_t tid;
  if(pthread_create(&tid, NULL, serverThreadFunc, this) != 0)
  {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }
}

bool SnepServer::handleRequest(SnepMessenger* messenger, ISnepCallback* callback)
{
  SnepMessage* request = messenger->getMessage();
  SnepMessage* response = NULL;
  if (request == NULL) {
    ALOGE("Bad snep message");
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST);
    messenger->sendMessage(*response);
    return false;
  } 

  if (((request->getVersion() & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_UNSUPPORTED_VERSION);
    messenger->sendMessage(*response);
  } else if (request->getField() == SnepMessage::REQUEST_GET) {
    response = callback->doGet(request->getAcceptableLength(), *request->getNdefMessage());
    messenger->sendMessage(*response);
  } else if (request->getField() == SnepMessage::REQUEST_PUT) {
    response = callback->doPut(*request->getNdefMessage());
    messenger->sendMessage(*response);
  } else {
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_UNSUPPORTED_VERSION);
    messenger->sendMessage(*response);
  }

  delete response;

  return true;
}
