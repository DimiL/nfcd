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

#include "DeviceHost.h"
#include "NfcDebug.h"
#include "NfcService.h"

void DeviceHost::NotifyTagDiscovered(INfcTag* aTag)
{
  NfcService::NotifyTagDiscovered(aTag);
}

void DeviceHost::NotifyTargetDeselected()
{
  NFCD_ERROR("not implement");
}

void DeviceHost::NotifyTransactionEvent(TransactionEvent* aEvent)
{
  NfcService::NotifySETransactionEvent(aEvent);
}

void DeviceHost::NotifyLlcpLinkActivated(IP2pDevice* aDevice)
{
  NfcService::NotifyLlcpLinkActivated(aDevice);
}

void DeviceHost::NotifyLlcpLinkDeactivated(IP2pDevice* aDevice)
{
  NfcService::NotifyLlcpLinkDeactivated(aDevice);
}

void DeviceHost::NotifyLlcpLinkFirstPacketReceived()
{
  NFCD_ERROR("not implement");
}

void DeviceHost::NotifySeFieldActivated()
{
  NFCD_ERROR("not implement");
}

void DeviceHost::NotifySeFieldDeactivated()
{
  NFCD_ERROR("not implement");
}

TransactionEvent::TransactionEvent()
 : originType(TransactionEvent::SIM)
 , originIndex(-1)
 , aidLen(0)
 , aid(NULL)
 , payloadLen(0)
 , payload(NULL)
{
}

TransactionEvent::~TransactionEvent()
{
  delete aid;
  delete payload;
}
