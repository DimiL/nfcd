#include "DeviceHost.h"
#include "NfcService.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

DeviceHost::DeviceHost()
{
  ALOGE("DeviceHost constructor");
}

DeviceHost::~DeviceHost()
{
}

void DeviceHost::notifyNdefMessageListeners()
{
}

void DeviceHost::notifyTargetDeselected()
{
}

void DeviceHost::notifyTransactionListeners()
{
}

void DeviceHost::notifyLlcpLinkActivation(void* pDevice)
{
  ALOGE("DeviceHost::notifyLlcpLinkActivation");
  nfc_service_send_MSG_LLCP_LINK_ACTIVATION(pDevice);
}

void DeviceHost::notifyLlcpLinkDeactivated(void* pDevice)
{
  nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(pDevice);
}

void DeviceHost::notifyLlcpLinkFirstPacketReceived()
{
}

void DeviceHost::notifySeFieldActivated()
{
  nfc_service_send_MSG_SE_FIELD_ACTIVATED();
}

void DeviceHost::notifySeFieldDeactivated()
{
  nfc_service_send_MSG_SE_FIELD_DEACTIVATED();
}
