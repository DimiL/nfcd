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

#include "SyncEvent.h"

extern "C"
{
  #include "nfa_ee_api.h"
  #include "nfa_hci_api.h"
  #include "nfa_hci_defs.h"
  #include "nfa_ce_api.h"
}

class NfcManager;

class RoutingManager
{
public:
  static RoutingManager& GetInstance();
  bool Initialize(NfcManager* aNfcManager);

private:
  RoutingManager();
  ~RoutingManager();

  int mDefaultEe;
  SyncEvent mEeRegisterEvent;
  SyncEvent mRoutingEvent;

  void SetDefaultRouting();

  static void NfaEeCallback(tNFA_EE_EVT aEvent, tNFA_EE_CBACK_DATA* aEventData);
  static void StackCallback(UINT8 aEvent, tNFA_CONN_EVT_DATA* aEventData);
};
