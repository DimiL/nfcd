/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NfcIpcSocket_h__
#define mozilla_NfcIpcSocket_h__

#include <pthread.h>
#include <time.h>

class NfcIpcSocket{
private:
  static NfcIpcSocket* sInstance;

public:
  virtual ~NfcIpcSocket();

  static NfcIpcSocket* Instance();
  void initialize();
  void loop();

  void writeToOutgoingQueue(char *buffer, size_t length);
  void writeToIncomingQueue(char *buffer, size_t length);

private:
  NfcIpcSocket();

  pthread_t mReader_thread_id;
  pthread_t mWriter_thread_id;

  timespec mSleep_spec;
  timespec mSleep_spec_rem;

  static const size_t MAX_READ_SIZE = 1 << 10;

  static void* writer_thread(void *arg);
  static void* reader_thread(void *arg);

  void initSocket();
  int getListenSocket();
};

#endif
