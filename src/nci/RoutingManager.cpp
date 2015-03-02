/*
 * Copyright (C) 2014  Mozilla Foundation
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

#include "RoutingManager.h"
#include "SecureElement.h"
#include "NfcManager.h"
#include "config.h"

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

RoutingManager::RoutingManager ()
{
}

RoutingManager::~RoutingManager ()
{
  NFA_EeDeregister(NfaEeCallback);
}

bool RoutingManager::Initialize(NfcManager* aNfcManager)
{
  tNFA_STATUS nfaStat;
  {
    SyncEventGuard guard(mEeRegisterEvent);
    nfaStat = NFA_EeRegister(NfaEeCallback);
    if (nfaStat != NFA_STATUS_OK) {
      return false;
    }
    mEeRegisterEvent.Wait();
  }

  // Get the "default" route
  unsigned long num = 0;
  if (GetNumValue("DEFAULT_ISODEP_ROUTE", &num, sizeof(num))) {
    mDefaultEe = num;
  } else {
    mDefaultEe = 0x00;
  }

  SetDefaultRouting();
  return true;
}

RoutingManager& RoutingManager::GetInstance()
{
  static RoutingManager manager;
  return manager;
}

void RoutingManager::SetDefaultRouting()
{
  tNFA_STATUS nfaStat;
  SyncEventGuard guard(mRoutingEvent);

  ALOGD("[Dimi]SetDefaultRouting >>");

  // Default routing for NFC-A technology
  nfaStat = NFA_EeSetDefaultTechRouting(mDefaultEe, NFA_TECHNOLOGY_MASK_A, 0, 0);
  if (nfaStat == NFA_STATUS_OK) {
    mRoutingEvent.Wait();
  } else {
    ALOGE("Fail to set default tech routing");
  }

  // Default routing for IsoDep protocol
  nfaStat = NFA_EeSetDefaultProtoRouting(mDefaultEe, NFA_PROTOCOL_MASK_ISO_DEP, 0, 0);
  if (nfaStat == NFA_STATUS_OK) {
    mRoutingEvent.Wait();
  } else {
    ALOGE("Fail to set default proto routing");
  }

  // Tell the UICC to only listen on Nfc-A
  nfaStat = NFA_CeConfigureUiccListenTech(mDefaultEe, NFA_TECHNOLOGY_MASK_A);
  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("Failed to configure UICC listen technologies");
  }

  // Tell the host-routing to only listen on Nfc-A
  nfaStat = NFA_CeSetIsoDepListenTech(NFA_TECHNOLOGY_MASK_A);
  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("Failed to configure CE IsoDep technologies");
  }

  // Register a wild-card for AIDs routed to the host
  //nfaStat = NFA_CeRegisterAidOnDH (NULL, 0, stackCallback);
  //if (nfaStat != NFA_STATUS_OK) {
  //  ALOGE("Failed to register wildcard AID for DH");
  //}

  // Commit the routing configuration
  nfaStat = NFA_EeUpdateNow();
  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("Failed to commit routing configuration");
  }

  ALOGD("[Dimi]SetDefaultRouting <<");
}

void RoutingManager::StackCallback(UINT8 aEvent, tNFA_CONN_EVT_DATA* aEventData)
{
  RoutingManager& rm = RoutingManager::GetInstance();
}

void RoutingManager::NfaEeCallback(tNFA_EE_EVT aEvent, tNFA_EE_CBACK_DATA* aEventData)
{
  SecureElement& se = SecureElement::GetInstance();
  RoutingManager& rm = RoutingManager::GetInstance();

  switch (aEvent)
  {
    case NFA_EE_REGISTER_EVT:
    {
      SyncEventGuard guard(rm.mEeRegisterEvent);
      rm.mEeRegisterEvent.NotifyOne();
    }
    break;

    case NFA_EE_MODE_SET_EVT:
    {
      se.NotifyModeSet(aEventData->mode_set.ee_handle, aEventData->mode_set.status);
    }
    break;

    case NFA_EE_SET_TECH_CFG_EVT:
    {
      SyncEventGuard guard(rm.mRoutingEvent);
      rm.mRoutingEvent.NotifyOne();
    }
    break;

    case NFA_EE_SET_PROTO_CFG_EVT:
    {
      SyncEventGuard guard(rm.mRoutingEvent);
      rm.mRoutingEvent.NotifyOne();
    }
    break;
  }
}
