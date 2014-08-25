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
#include "NfcService.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

void DeviceHost::notifyTagDiscovered(INfcTag* pTag)
{
  NfcService::notifyTagDiscovered(pTag);
}

void DeviceHost::notifyTargetDeselected()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

void DeviceHost::notifyTransactionEvent(TransactionEvent* pEvent)
{
  NfcService::notifySETransactionEvent(pEvent);
}

void DeviceHost::notifyLlcpLinkActivated(IP2pDevice* pDevice)
{
  NfcService::notifyLlcpLinkActivated(pDevice);
}

void DeviceHost::notifyLlcpLinkDeactivated(IP2pDevice* pDevice)
{
  NfcService::notifyLlcpLinkDeactivated(pDevice);
}

void DeviceHost::notifyLlcpLinkFirstPacketReceived()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

void DeviceHost::notifySeFieldActivated()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

void DeviceHost::notifySeFieldDeactivated()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

TransactionEvent::TransactionEvent()
 : aidLen(0)
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
