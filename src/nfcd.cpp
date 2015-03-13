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

#include <getopt.h>
#include <stdlib.h>

#include "nfcd.h"

#include "NfcManager.h"
#include "NfcService.h"
#include "NfcIpcSocket.h"
#include "DeviceHost.h"
#include "MessageHandler.h"
#include "SnepServer.h"

static const char* DEFAULT_SOCKET_NAME = NULL; /* creates a listen socket */

struct Options {
  const char* mSocketName;

  Options()
    : mSocketName(DEFAULT_SOCKET_NAME)
  { }

  int Parse(int aArgc, char *aArgv[])
  {
    int res = 0;

    opterr = 0; /* no default error messages from getopt */

    do {
      int c = getopt(aArgc, aArgv, "a:h");

      if (c < 0) {
        break; /* end of options */
      }
      switch (c) {
        case 'a':
          res = ParseOpt_a(c, optarg);
          break;
        case 'h':
          res = ParseOpt_h(c, optarg);
          break;
        case '?':
          res = ParseOpt_QuestionMark(c, optarg);
          break;
        default:
          res = -1; /* unknown option */
          break;
      }
    } while (!res);

    return res;
  }

private:
  int ParseOpt_a(int aC, char* aArg)
  {
    if (!aArg) {
      fprintf(stderr, "Error: No network address specified.");
      return -1;
    }
    if (!strlen(aArg)) {
      fprintf(stderr, "Error: The specified network address is empty.");
      return -1;
    }
    mSocketName = aArg;

    return 0;
  }

  int ParseOpt_h(int aC, char* aArg)
  {
    printf("Usage: nfcd [OPTION]\n"
           "Wraps NFC behind a networking protocol\n"
           "\n"
           "General options:\n"
           "  -h    displays this help\n"
           "\n"
           "Networking:\n"
           "  -a    the network address\n"
           "\n"
           "The only supported address family is AF_UNIX with abstract\n"
           "names. Not setting a network address will create a listen\n"
           "socket.\n");

    return 1;
  }

  int ParseOpt_QuestionMark(int aC, char* aArg)
  {
    fprintf(stderr, "Unknown option %c\n", aC);

    return -1;
  }
};

int main(int argc, char *argv[]) {

  struct Options options;
  int res = options.Parse(argc, argv);

  if (res > 0) {
    return EXIT_SUCCESS;
  } else if (res < 0) {
    return EXIT_FAILURE;
  }

  // Create NFC Manager and do initialize.
  NfcManager* pNfcManager = new NfcManager();

  // Create service thread to receive message from nfc library.
  NfcService* service = NfcService::Instance();
  MessageHandler* msgHandler = new MessageHandler(service);
  service->Initialize(pNfcManager, msgHandler);

  // Create IPC socket & main thread will enter while loop to read data from socket.
  NfcIpcSocket* socket = NfcIpcSocket::Instance();
  socket->Initialize(msgHandler);
  socket->SetSocketListener(service);
  msgHandler->SetOutgoingSocket(socket);
  socket->Loop(options.mSocketName);

  //TODO delete NfcIpcSocket, NfcService
  delete msgHandler;
  delete pNfcManager;
  //exit(0);
}
