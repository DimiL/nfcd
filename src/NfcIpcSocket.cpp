/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <linux/prctl.h>
#include <cutils/sockets.h>
#include <cutils/record_stream.h>
#include <unistd.h>
#include <queue>
#include <string>
#define LOG_TAG "nfcd"
#include <cutils/log.h>

#include "NfcIpcSocket.h"
#include "MessageHandler.h"

#define NFCD_SOCKET_NAME "nfcd"
#define MAX_COMMAND_BYTES (8 * 1024)

using android::Parcel;

class Buffer {
public:
  Buffer(void* data, size_t dataLen) :
    mData(data), mDataLen(dataLen) {}
  size_t size() {return mDataLen;}
  void* data() {return mData;}
  void* mData;
  size_t mDataLen;
};

static std::queue<Parcel*> mOutgoing;
static std::queue<Buffer> mIncoming;

static pthread_mutex_t mReadMutex;
static pthread_mutex_t mWriteMutex;

static pthread_cond_t mRcond;
static pthread_cond_t mWcond;

static int nfcdRw;

/**
 * NFC daemon reader Thread
 * gecko -> nfcd
 * NFC queue from Gecko socket
 * Check incoming queue and process the message
 */
void* NfcIpcSocket::readerThreadFunc(void *arg)
{
  while (1) {
    pthread_mutex_lock(&mWriteMutex);

    if (!mIncoming.empty()) {
      Buffer buffer = mIncoming.front();

      if (buffer.size() == 0) {
        ALOGI("tag_writerThreadFunc received an empty bufferer.");
        pthread_mutex_unlock(&mWriteMutex);
        continue;
      }

      MessageHandler::processRequest((uint8_t*)buffer.data(), buffer.size());

      mIncoming.pop();
    } else {
      pthread_cond_wait(&mWcond, &mWriteMutex);
    }
    pthread_mutex_unlock(&mWriteMutex);
  }
  return NULL;
}

/**
 * NFC daemon writer thread
 * nfcd -> gecko
 * NFC queue to Gecko socket
 * Read from outgoing queue and write buffer to ipc socket
 */
void* NfcIpcSocket::writerThreadFunc(void *arg)
{
  while (1) {
    pthread_mutex_lock(&mReadMutex);
    while (!mOutgoing.empty()) {
      Parcel* parcel = mOutgoing.front();

      size_t writeOffset = 0;
      size_t len = parcel->dataSize();
      size_t written = 0;

      //TODO update this
      size_t size = __builtin_bswap32(parcel->dataSize());
      write(nfcdRw, (void*)&size, sizeof(uint32_t));

      ALOGD("Writing %d bytes to gecko ", parcel->dataSize());
      while (writeOffset < len) {
        do {
          written = write (nfcdRw, parcel->data() + writeOffset,
                           len - writeOffset);
        } while (written < 0 && errno == EINTR);

        if (written >= 0) {
          writeOffset += written;
        } else {
          ALOGE("Response: unexpected error on write errno:%d", errno);
          break;
        }
      }
      mOutgoing.pop();

      delete parcel;
    }
    while (mOutgoing.empty()) {
      pthread_cond_wait(&mRcond, &mReadMutex);
    }
    pthread_mutex_unlock(&mReadMutex);
  }
  return NULL;
}

/**
 * NfcIpcSocket
 */
NfcIpcSocket* NfcIpcSocket::sInstance = NULL;

NfcIpcSocket* NfcIpcSocket::Instance() {
    if (!sInstance)
        sInstance = new NfcIpcSocket();
    return sInstance;
}

NfcIpcSocket::NfcIpcSocket()
{
}

NfcIpcSocket::~NfcIpcSocket()
{
}

void NfcIpcSocket::initialize()
{
  initSocket();
}

void NfcIpcSocket::initSocket()
{
  pthread_mutex_init(&mReadMutex, NULL);
  pthread_mutex_init(&mWriteMutex, NULL);

  pthread_cond_init(&mRcond, NULL);
  pthread_cond_init(&mWcond, NULL);

  mSleep_spec.tv_sec = 0;
  mSleep_spec.tv_nsec = 500 * 1000;
  mSleep_spec_rem.tv_sec = 0;
  mSleep_spec_rem.tv_nsec = 0;

  if(pthread_create(&mReaderTid, NULL, readerThreadFunc, NULL) != 0)
  {
      ALOGE("main tag reader pthread_create failed");
      abort();
  }
  if(pthread_create(&mWriterTid, NULL, writerThreadFunc, NULL) != 0)
  {
      ALOGE("main tag writer pthread_create failed");
      abort();
  }
}

int NfcIpcSocket::getListenSocket() {
  const int nfcdConn = android_get_control_socket(NFCD_SOCKET_NAME);
  if (nfcdConn < 0) {
    ALOGE("Could not connect to %s socket: %s\n", NFCD_SOCKET_NAME, strerror(errno));
    return -1;
  }

  if (listen(nfcdConn, 4) != 0) {
    return -1;
  }
  return nfcdConn;
}

void NfcIpcSocket::loop()
{
  bool connected = false;
  int nfcdConn;
  int ret;

  while(1) {
    struct sockaddr_un peeraddr;
    socklen_t socklen = sizeof (peeraddr);

    if (!connected) {
      nfcdConn = getListenSocket();
      if (nfcdConn < 0) {
        nanosleep(&mSleep_spec, &mSleep_spec_rem);
        continue;
      }
    }

    nfcdRw = accept(nfcdConn, (struct sockaddr*)&peeraddr, &socklen);

    if (nfcdRw < 0 ) {
      ALOGE("Error on accept() errno:%d", errno);
      /* start listening for new connections again */
      continue;
    }

    ret = fcntl(nfcdRw, F_SETFL, O_NONBLOCK);
    if (ret < 0) {
      ALOGE ("Error setting O_NONBLOCK errno:%d", errno);
    }

    ALOGD("Socket connected");
    connected = true;

    RecordStream *rs = record_stream_new(nfcdRw, MAX_COMMAND_BYTES);

    struct pollfd fds[1];
    fds[0].fd = nfcdRw;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    while(connected) {
      poll(fds, 1, -1);
      if(fds[0].revents > 0) {
        fds[0].revents = 0;

        void* data;
        size_t dataLen;
        int ret = record_stream_get_next(rs, &data, &dataLen);
        ALOGD("# of bytes to be sent... %d data=%p ret=%d", dataLen, data, ret);
        if (ret == 0 && data == NULL) {
          // end-of-stream
          break;
        } else if (ret < 0) {
          break;
        }
        writeToIncomingQueue((uint8_t*)data, dataLen);
      }
    }
    record_stream_free(rs);
    close(nfcdRw);
  }

  return;
}

// Write NFC data to Gecko
// Outgoing queue contain the data should be send to gecko
void NfcIpcSocket::writeToOutgoingQueue(Parcel* parcel) {
  ALOGD("%s enter, data=%p, dataLen=%d", __func__, parcel->data(), parcel->dataSize());
  pthread_mutex_lock(&mReadMutex);

  if (parcel->data() != NULL && parcel->dataSize() > 0) {
    mOutgoing.push(parcel);
    pthread_cond_signal(&mRcond);
  }
  pthread_mutex_unlock(&mReadMutex);
}

// Write Gecko data to NFC
// Incoming queue contains
void NfcIpcSocket::writeToIncomingQueue(uint8_t* data, size_t dataLen) {
  ALOGD("%s enter, data=%p, dataLen=%d", __func__, data, dataLen);
  pthread_mutex_lock(&mWriteMutex);

  if (data != NULL && dataLen > 0) {
    Buffer buffer(data, dataLen);
    mIncoming.push(buffer);
    pthread_cond_signal(&mWcond);
  }
  pthread_mutex_unlock(&mWriteMutex);
}
