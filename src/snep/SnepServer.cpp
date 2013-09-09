#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

char* mServiceName;
int mServiceSap;
int mFragmentLength;
int mMiu;
int mRwSize;

const char* SnepServer::DEFAULT_SERVICE_NAME = "urn:nfc:sn:snep";

void* connectionThreadFunc(void* arg)
{
  bool running = true;
  /*
  while(running) {
    // Handle message
    if (!SnepServer::handleRequest(mMessager)) {
      break;
    }
  }*/

  return NULL;
}

void* serverThreadFunc(void* arg)
{
  INfcManager* pINfcManager = NfcService::getNfcManager();
  ILlcpServerSocket* pServerSocket = pINfcManager->createLlcpServerSocket(
    mServiceSap, mServiceName, mMiu, mRwSize, 1024);

  while(true) {
    if (pServerSocket == NULL) {
        ALOGD("Server socket shut down.");
        return NULL;
    }

    ILlcpSocket* communicationSocket = pServerSocket->accept();
    
    if (communicationSocket != NULL) {
      int miu = communicationSocket->getRemoteMiu();
      // use math
      int fragmentLength = (mFragmentLength == -1) ? miu : miu < mFragmentLength ? miu : mFragmentLength;
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

SnepServer::SnepServer()
{

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

bool SnepServer::handleRequest(SnepMessenger& messenger)
{
  SnepMessage* request = messenger.getMessage();
  if (request == NULL) {
    ALOGE("Bad snep message");
    //messenger.sendMessage(SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST));
    return false;
  } 

  if (((request->getVersion() & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    //messenger.sendMessage(SnepMessage.getMessage(SnepMessage::RESPONSE_UNSUPPORTED_VERSION));
  } else if (request->getField() == SnepMessage::REQUEST_GET) {
    // TODO : Add callback
    // messenger.sendMessage(callback.doGet(request.getAcceptableLength(), request.getNdefMessage()));
  } else if (request->getField() == SnepMessage::REQUEST_PUT) {
    // TODO : Add callback
    // messenger.sendMessage(callback.doPut(request.getNdefMessage()));
  } else {
    //messenger.sendMessage(SnepMessage.getMessage(SnepMessage::RESPONSE_BAD_REQUEST));
  }

  return true;
}
