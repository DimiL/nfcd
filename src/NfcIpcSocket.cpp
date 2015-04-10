/*
 * Copyright (C) 2013-2014  Mozilla Foundation
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

#include "IpcSocketListener.h"
#include "NfcIpcSocket.h"
#include "MessageHandler.h"
#include "NfcDebug.h"

#define NFCD_SOCKET_NAME "nfcd"
#define MAX_COMMAND_BYTES (8 * 1024)

using android::Parcel;

/**
 * NfcIpcSocket
 */
NfcIpcSocket* NfcIpcSocket::sInstance = NULL;

NfcIpcSocket* NfcIpcSocket::Instance() {
  if (!sInstance) {
    sInstance = new NfcIpcSocket();
  }
  return sInstance;
}

NfcIpcSocket::NfcIpcSocket()
  : mMsgHandler(NULL)
  , mListener(NULL)
  , mNfcdRw(-1)
{
}

NfcIpcSocket::~NfcIpcSocket()
{
}

void NfcIpcSocket::Initialize(MessageHandler* aMsgHandler)
{
  InitSocket();
  mMsgHandler = aMsgHandler;
}

void NfcIpcSocket::InitSocket()
{
  mSleep_spec.tv_sec = 0;
  mSleep_spec.tv_nsec = 500 * 1000;
  mSleep_spec_rem.tv_sec = 0;
  mSleep_spec_rem.tv_nsec = 0;
}

int NfcIpcSocket::GetListenSocket() {
  const int nfcdConn = android_get_control_socket(NFCD_SOCKET_NAME);
  if (nfcdConn < 0) {
    NFCD_ERROR("Could not connect to %s socket: %s\n", NFCD_SOCKET_NAME, strerror(errno));
    return -1;
  }

  if (listen(nfcdConn, 4) != 0) {
    return -1;
  }
  return nfcdConn;
}

int NfcIpcSocket::GetConnectedSocket(const char* aSocketName)
{
  static const size_t NBOUNDS = 2; // respect leading and trailing '\0'

  size_t len = strlen(aSocketName);
  if (len > (SIZE_MAX - NBOUNDS)) {
    NFCD_ERROR("Socket address too long\n");
    return -1;
  }

  size_t siz = len + NBOUNDS;
  if (siz > UNIX_PATH_MAX) {
    NFCD_ERROR("Socket address too long\n");
    return -1;
  }

  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  addr.sun_path[0] = '\0'; /* abstract socket namespace */
  memcpy(addr.sun_path + 1, aSocketName, len + 1);
  socklen_t addrLen = offsetof(struct sockaddr_un, sun_path) + siz;

  int nfcdRw = socket(AF_UNIX, SOCK_STREAM, 0);
  if (nfcdRw < 0) {
    NFCD_ERROR("Could not create %s socket: %s\n", aSocketName, strerror(errno));
    return -1;
  }

  int res = TEMP_FAILURE_RETRY(
    connect(nfcdRw, reinterpret_cast<struct sockaddr*>(&addr), addrLen));
  if (res < 0) {
    NFCD_ERROR("Could not connect %s socket: %s\n", aSocketName, strerror(errno));
    close(nfcdRw);
    return -1;
  }

  return nfcdRw;
}

void NfcIpcSocket::SetSocketListener(IpcSocketListener* aListener) {
  mListener = aListener;
}

void NfcIpcSocket::Loop(const char* aSocketName)
{
  bool connected = false;
  int nfcdConn = -1;
  int ret;

  while (1) {

    /* If a socket name was given to nfcd, we connect to it. Otherwise
     * we fall back to the old method of listening ourselves.
     */

    if (aSocketName) {
      if (aSocketName == reinterpret_cast<const char*>(uintptr_t(-1))) {
        break; /* connected before; return */
      }
      mNfcdRw = GetConnectedSocket(aSocketName);
      if (mNfcdRw < 0) {
        break; /* no connection; return */
      }
      /* signal success to next iteration */
      aSocketName = reinterpret_cast<const char*>(uintptr_t(-1));
    } else {
      struct sockaddr_un peeraddr;
      socklen_t socklen = sizeof(peeraddr);

      if (!connected) {
        nfcdConn = GetListenSocket();
        if (nfcdConn < 0) {
          nanosleep(&mSleep_spec, &mSleep_spec_rem);
          continue;
        }
      }

      mNfcdRw = accept(nfcdConn, (struct sockaddr*)&peeraddr, &socklen);

      if (mNfcdRw < 0 ) {
        NFCD_ERROR("Error on accept() errno:%d", errno);
        /* start listening for new connections again */
        continue;
      }
    }

    ret = fcntl(mNfcdRw, F_SETFL, O_NONBLOCK);
    if (ret < 0) {
      NFCD_ERROR("Error setting O_NONBLOCK errno:%d", errno);
    }

    NFCD_DEBUG("Socket connected");
    connected = true;

    RecordStream* rs = record_stream_new(mNfcdRw, MAX_COMMAND_BYTES);

    mListener->OnConnected();

    struct pollfd fds[1];
    fds[0].fd = mNfcdRw;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    while (connected) {
      poll(fds, 1, -1);
      if (fds[0].revents > 0) {
        fds[0].revents = 0;

        void* data;
        size_t dataLen;
        int ret = record_stream_get_next(rs, &data, &dataLen);
        NFCD_DEBUG(" %d of bytes to be sent... data=%p ret=%d", dataLen, data, ret);
        if (ret == 0 && data == NULL) {
          // end-of-stream
          break;
        } else if (ret < 0) {
          break;
        }
        WriteToIncomingQueue((uint8_t*)data, dataLen);
      }
    }
    record_stream_free(rs);
    close(mNfcdRw);
  }

  return;
}

// Write NFC data to Gecko
// Outgoing queue contain the data should be send to gecko
// TODO check thread, this should run on the NfcService thread.
void NfcIpcSocket::WriteToOutgoingQueue(uint8_t* aData, size_t aDataLen)
{
  NFCD_DEBUG("enter, data=%p, dataLen=%d", aData, aDataLen);

  if (aData == NULL || aDataLen == 0) {
    return;
  }

  size_t writeOffset = 0;
  int written = 0;

  NFCD_DEBUG("Writing %d bytes to gecko ", aDataLen);
  while (writeOffset < aDataLen) {
    do {
      written = write (mNfcdRw, aData + writeOffset, aDataLen - writeOffset);
    } while (written < 0 && errno == EINTR);

    if (written >= 0) {
      writeOffset += written;
    } else {
      NFCD_ERROR("Response: unexpected error on write errno:%d", errno);
      break;
    }
  }
}

// Write Gecko data to NFC
// Incoming queue contains
// TODO check thread, this should run on top of main thread of nfcd.
void NfcIpcSocket::WriteToIncomingQueue(uint8_t* aData, size_t aDataLen)
{
  NFCD_DEBUG("enter, data=%p, dataLen=%d", aData, aDataLen);

  if (aData != NULL && aDataLen > 0) {
    mMsgHandler->ProcessRequest(aData, aDataLen);
  }
}
