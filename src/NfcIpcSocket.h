/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_NfcIpcSocket_h
#define mozilla_nfcd_NfcIpcSocket_h

#include <pthread.h>
#include <time.h>

class NfcIpcSocket{
private:
  static NfcIpcSocket* sInstance;

public:
  ~NfcIpcSocket();

  static NfcIpcSocket* Instance();
  void initialize();
  void loop();

  static void writeToOutgoingQueue(char *buffer, size_t length);
  static void writeToIncomingQueue(char *buffer, size_t length);

private:
  NfcIpcSocket();

  pthread_t mReaderTid;
  pthread_t mWriterTid;

  timespec mSleep_spec;
  timespec mSleep_spec_rem;

  static const size_t MAX_READ_SIZE = 1 << 10;

  static void* writerThreadFunc(void *arg);
  static void* readerThreadFunc(void *arg);

  void initSocket();
  int getListenSocket();
};

#endif // mozilla_nfcd_NfcIpcSocket_h
