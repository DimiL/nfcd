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
#include "DeviceHost.h"
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
  mNfcManager = aNfcManager;

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
    ALOGD("[Dimi]No config, use default EE");
    mDefaultEe = 0x00;
//    mDefaultEe = 0x4f;
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
  nfaStat = NFA_CeRegisterAidOnDH(NULL, 0, StackCallback);
  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("Failed to register wildcard AID for DH");
  }

  // Commit the routing configuration
  nfaStat = NFA_EeUpdateNow();
  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("Failed to commit routing configuration");
  }

//  uint8_t aid[] = {0xA0, 0x00, 0x00, 0x00, 0x01, 0x02};
//  nfaStat = NFA_EeAddAidRouting(mDefaultEe, 6, (UINT8*) aid, 0x01);
//  nfaStat = NFA_EeAddAidRouting(0x402, 6, (UINT8*) aid, 0x01);

  nfaStat = NFA_EeUpdateNow();
  if (nfaStat != NFA_STATUS_OK) {
    ALOGE("Failed to commit routing configuration");
  }

  ALOGD("[Dimi]SetDefaultRouting <<");
}

bool RoutingManager::addAidRouting(const UINT8* aid, UINT8 aidLen, int route)
{
  static const char fn [] = "RoutingManager::addAidRouting";
  ALOGD("%s: enter", fn);
//  tNFA_STATUS nfaStat = NFA_EeAddAidRouting(route, aidLen, (UINT8*) aid, 0x01);
  tNFA_STATUS nfaStat = NFA_EeAddAidRouting(mDefaultEe, aidLen, (UINT8*) aid, 0x01);
  if (nfaStat == NFA_STATUS_OK) {
    ALOGD("%s: routed AID", fn);
    return true;
  } else {
    ALOGE("%s: failed to route AID", fn);
    return false;
  }
}

void RoutingManager::NotifyHCEDataEvent(const uint8_t* aData, uint32_t aDataLength)
{
  HostCardEmulationEvent* pEvent = new HostCardEmulationEvent();
  pEvent->dataLength = aDataLength;
  pEvent->data = new uint8_t[aDataLength];
  memcpy(pEvent->data, aData, aDataLength);

  mNfcManager->NotifyHostCardEmulationData(pEvent);
}

void RoutingManager::StackCallback(UINT8 aEvent, tNFA_CONN_EVT_DATA* aEventData)
{
  static const char fn [] = "[Dimi]RoutingManager::stackCallback";
  RoutingManager& rm = RoutingManager::GetInstance();

  switch (aEvent)
  {
    case NFA_CE_REGISTERED_EVT:
    {
      tNFA_CE_REGISTERED& ce_registered = aEventData->ce_registered;
      ALOGD("%s: NFA_CE_REGISTERED_EVT; status=0x%X; h=0x%X",
            fn, ce_registered.status, ce_registered.handle);
    }
    break;

    case NFA_CE_DEREGISTERED_EVT:
    {
      tNFA_CE_DEREGISTERED& ce_deregistered = aEventData->ce_deregistered;
      ALOGD("%s: NFA_CE_DEREGISTERED_EVT; h=0x%X", fn, ce_deregistered.handle);
    }
    break;

    case NFA_CE_ACTIVATED_EVT:
    {
      ALOGD("%s: NFA_CE_ACTIVATED_EVT, NFA_CE_ACTIVATED_EVT", fn);
    }
    break;

    case NFA_DEACTIVATED_EVT:
    case NFA_CE_DEACTIVATED_EVT:
    {
      ALOGD("%s: NFA_DEACTIVATED_EVT, NFA_CE_DEACTIVATED_EVT", fn);
    }
    break;

    case NFA_CE_DATA_EVT:
    {
      tNFA_CE_DATA& ce_data = aEventData->ce_data;
      ALOGD("%s: NFA_CE_DATA_EVT; h=0x%X; data len=%u",
            fn, ce_data.handle, ce_data.len);
      for (int i = 0; i < ce_data.len; i++) {
//        ALOGD("[Dimi]NFA_CE_DATA_EVT: [%d]=0x%x", i, ce_data.p_data[i]);
      }

      rm.NotifyHCEDataEvent(ce_data.p_data, ce_data.len);

      if (ce_data.len > 4 &&
          ce_data.p_data[0] == 0x00 &&
          ce_data.p_data[1] == 0xA4 &&  // INS
          ce_data.p_data[2] == 0x04 &&  // P1
          ce_data.p_data[3] == 0x00 &&  // P2
          ce_data.p_data[4] == 0x06 &&  // LC
          ce_data.p_data[5] == 0xA0 &&  // DATA
          ce_data.p_data[6] == 0x00 &&  // DATA
          ce_data.p_data[7] == 0x00 &&  // DATA
          ce_data.p_data[8] == 0x00 &&  // DATA
          ce_data.p_data[9] == 0x01 &&  // DATA
          ce_data.p_data[10] == 0x02) { // DATA

        ALOGD("[Dimi]Response 0x90 0x00");
        uint8_t buf2[2] = {0x90, 0x00};
        NFA_SendRawFrame(buf2, 2, 0);

//        ALOGD("[Dimi]Response 0x00 0xA4");
//        uint8_t buf[11] = {0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x01, 0x02};
//        NFA_SendRawFrame(buf, 11, 0);
      } else {
        uint8_t buf[2] = {0x90, 0x00};
        NFA_SendRawFrame (buf, 2, 0);
        ALOGD("[Dimi]Response2 0x90 0x00");
      }

//      getInstance().handleData(ce_data.p_data, ce_data.len, ce_data.status);
    }
    break;
  }
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
