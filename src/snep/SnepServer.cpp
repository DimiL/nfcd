#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepServer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

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
    ALOGE("function %s invalid parameter", __FUNCTION__);
    return NULL;
  }
  // TODO : We should send the ndef message to gecko
  // onReceiveComplete(msg);
  return SnepMessage::getMessage(SnepMessage::RESPONSE_SUCCESS);
}

SnepMessage* SnepCallback::doGet(int acceptableLength, NdefMessage* ndef)
{
  if (!ndef) {
    ALOGE("function %s invalid parameter", __FUNCTION__);
    return NULL;
  }
  // The NFC Forum Default SNEP server is not allowed to respond to
  // SNEP GET requests - see SNEP 1.0 TS section 6.1. However,
  // since Android 4.1 used the NFC Forum default server to
  // implement connection handover, we will support this
  // until we can deprecate it.
  return SnepMessage::getMessage(SnepMessage::RESPONSE_NOT_IMPLEMENTED);
}

void* connectionThreadFunc(void* arg)
{
  ALOGD("starting connection thread");

  bool running = true;
  ConnectionThread* pConnectionThread = reinterpret_cast<ConnectionThread*>(arg);
  if (pConnectionThread == NULL) {
    ALOGE("connectionThreadFunc invalid parameter");
    return NULL;
  }

  ISnepCallback* ICallback = pConnectionThread->mCallback;
  while(pConnectionThread->isServerRunning()) {
    // Handle message
    if (!SnepServer::handleRequest(pConnectionThread->mMessenger, ICallback)) {
      break;
    }
  }

  if (pConnectionThread->mSock)
    pConnectionThread->mSock->close();

  // TODO : is this correct ??
  delete pConnectionThread;

  ALOGD("finished connection thread");

  return NULL;
}

ConnectionThread::ConnectionThread(
  SnepServer* server,ILlcpSocket* socket, int fragmentLength, ISnepCallback* ICallback):
mSock(socket),
mCallback(ICallback),
mServer(server)
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
    ALOGE("connection pthread_create failed");
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

  ISnepCallback* ICallback = pSnepServer->mCallback;
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
          new ConnectionThread(pSnepServer, communicationSocket, length, ICallback);
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
  ALOGD("%s enter", __func__);
  INfcManager* pINfcManager = NfcService::getNfcManager();
  mServerSocket = pINfcManager->createLlcpServerSocket(mServiceSap, mServiceName, mMiu, mRwSize, 1024);

  if (mServerSocket == NULL) {
    ALOGE("%s cannot create llcp serfer socket", __func__);
  }

  pthread_t tid;
  if(pthread_create(&tid, NULL, serverThreadFunc, this) != 0)
  {
    ALOGE("%s init_nfc_service pthread_create failed", __func__);
    abort();
  }
  mServerRunning = true;
  ALOGD("%s exit", __func__);
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
    ALOGE("function %s invalid parameter", __FUNCTION__);
    return false;
  }

  SnepMessage* request = messenger->getMessage();
  SnepMessage* response = NULL;

  if (request == NULL) {
    ALOGE("function %s : Bad snep message", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST);
    if (response) {
      messenger->sendMessage(*response);
      delete response;
    }
    return false;
  } 

  if (((request->getVersion() & 0xF0) >> 4) != SnepMessage::VERSION_MAJOR) {
    ALOGE("function %s : Unsupported version", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_UNSUPPORTED_VERSION);
    if (response)
      messenger->sendMessage(*response);
  } else if (request->getField() == SnepMessage::REQUEST_GET) {
    NdefMessage* ndef = request->getNdefMessage();
    response = callback->doGet(request->getAcceptableLength(), ndef);
    if (response)
      messenger->sendMessage(*response);
  } else if (request->getField() == SnepMessage::REQUEST_PUT) {
    NdefMessage* ndef = request->getNdefMessage();
    response = callback->doPut(ndef);
    if (response)
      messenger->sendMessage(*response);
  } else {
    ALOGE("function %s : Bad request", __FUNCTION__);
    response = SnepMessage::getMessage(SnepMessage::RESPONSE_BAD_REQUEST);
    if (response)
      messenger->sendMessage(*response);
  }

  // TODO : check if we need to delete request here
  if (response) {
    delete response;
  } else {
    ALOGE("function %s : No response message generated", __FUNCTION__);
  }

  return true;
}
