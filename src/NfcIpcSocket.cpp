#include "NfcIpcSocket.h"

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
#include <unistd.h>
#include <queue>
#include <string>

#define LOG_TAG "nfcd"
#include <cutils/log.h>

#define NFCD_SOCKET_NAME "nfcd"

static std::queue<std::string> mOutgoing;
static std::queue<std::string> mIncoming;

static pthread_mutex_t mReadMutex;
static pthread_mutex_t mWriteMutex;

static pthread_cond_t mRcond;
static pthread_cond_t mWcond;

static int nfcd_rw;


/******************************************************************************
 * NFC IPC Threads:
 ******************************************************************************/
// gecko -> nfcd
// NFC queue from Gecko socket
// Check incoming queue and process the message
void* NfcIpcSocket::writer_thread(void *arg)
{
  while (1) {
    pthread_mutex_lock(&mWriteMutex);

    if (!mIncoming.empty()) {
      std::string buff = mIncoming.front();

      if (buff.size() == 0) {
        ALOGI("tag_writer_thread received an empty buffer.");
        pthread_mutex_unlock(&mWriteMutex);
        continue;
      }

      // Dimi : TODO, Message handling
      mIncoming.pop();
    } else {
      pthread_cond_wait(&mWcond, &mWriteMutex);
    }
    pthread_mutex_unlock(&mWriteMutex);
  }
  return NULL;
}

// nfcd -> gecko
// NFC queue to Gecko socket
// Read from outgoing queue and write buffer to ipc socket
void* NfcIpcSocket::reader_thread(void *arg)
{
  while (1) {
    pthread_mutex_lock(&mReadMutex);
    while (!mOutgoing.empty()) {
      std::string buffer = mOutgoing.front();

      size_t write_offset = 0;
      size_t len = buffer.size() + 1;
      size_t written = 0;

      ALOGD("Writing %d bytes to gecko (%.*s)", buffer.size(), buffer.size(), buffer.c_str());
      while (write_offset < len) {
        do {
          written = write (nfcd_rw, buffer.c_str() + write_offset,
                           len - write_offset);
        } while (written < 0 && errno == EINTR);

        if (written >= 0) {
          write_offset += written;
        } else {
          ALOGE("Response: unexpected error on write errno:%d", errno);
          break;
        }
      }
      mOutgoing.pop();
    }
    while (mOutgoing.empty()) {
      pthread_cond_wait(&mRcond, &mReadMutex);
    }
    pthread_mutex_unlock(&mReadMutex);
  }
  return NULL;
}

/******************************************************************************
 * NfcIpcSocket class
 ******************************************************************************/
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
  mSleep_spec.tv_nsec = 500*1000;
  mSleep_spec_rem.tv_sec = 0;
  mSleep_spec_rem.tv_nsec = 0;

  if(pthread_create(&mReader_thread_id, NULL, reader_thread, NULL) != 0)
  {
      ALOGE("main tag reader pthread_create failed");
      abort();
  }
  if(pthread_create(&mWriter_thread_id, NULL, writer_thread, NULL) != 0)
  {
      ALOGE("main tag writer pthread_create failed");
      abort();
  }
}

int NfcIpcSocket::getListenSocket() {
  const int nfcd_conn = android_get_control_socket(NFCD_SOCKET_NAME);
  if (nfcd_conn < 0) {
    ALOGE("Could not connect to %s socket: %s\n", NFCD_SOCKET_NAME, strerror(errno));
    return -1;
  }

  if (listen(nfcd_conn, 4) != 0) {
    return -1;
  }
  return nfcd_conn;
}

void NfcIpcSocket::loop()
{
  bool connected = false;
  int nfcd_conn;
  int ret;

  while(1) {
    struct sockaddr_un peeraddr;
    socklen_t socklen = sizeof (peeraddr);

    if (!connected) {
      nfcd_conn = getListenSocket();
      if (nfcd_conn < 0) {
        nanosleep(&mSleep_spec, &mSleep_spec_rem);
        continue;
      }
    }

    nfcd_rw = accept(nfcd_conn, (struct sockaddr*)&peeraddr, &socklen);

    if (nfcd_rw < 0 ) {
      ALOGE("Error on accept() errno:%d", errno);
      /* start listening for new connections again */
      continue;
    }

    ret = fcntl(nfcd_rw, F_SETFL, O_NONBLOCK);
    if (ret < 0) {
      ALOGE ("Error setting O_NONBLOCK errno:%d", errno);
    }

    ALOGD("Socket connected");
    connected = true;

    struct pollfd fds[1];
    fds[0].fd = nfcd_rw;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    while(connected) {
      poll(fds, 1, -1);
      if(fds[0].revents > 0) {
        fds[0].revents = 0;

        char data[MAX_READ_SIZE] = {0};
        ssize_t bytes_sent = read(nfcd_rw, data, MAX_READ_SIZE);
        data[bytes_sent] = 0;
        ALOGD("# of bytes to be sent... %d", bytes_sent);
        if (bytes_sent <= 0) {
          if (bytes_sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
              // try again....
              continue;
            }
          } else {
            ALOGE("Failed to read from nfcd socket, closing...");
            connected = 0;
            break;
          }
        }

        ALOGD("Received message (%s)", data);
        writeToIncomingQueue(data, bytes_sent);
      }
    }
    close(nfcd_rw);
  }

  return;
}

// Write NFC buffer to Gecko
void NfcIpcSocket::writeToOutgoingQueue(char *buffer, size_t length) {
  pthread_mutex_lock(&mReadMutex);

  if (buffer != NULL && length > 0) {
    mOutgoing.push(std::string(buffer, length));
    free(buffer);
    pthread_cond_signal(&mRcond);
  }
  pthread_mutex_unlock(&mReadMutex);
}


// Write Gecko buffer to NFC
void NfcIpcSocket::writeToIncomingQueue(char *buffer, size_t length) {
  pthread_mutex_lock(&mWriteMutex);

  if (buffer != NULL && length > 0) {
    mIncoming.push(std::string(buffer, length));
    free(buffer);
    pthread_cond_signal(&mWcond);
  }
  pthread_mutex_unlock(&mWriteMutex);
}
