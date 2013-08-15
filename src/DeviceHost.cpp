#include "DeviceHost.h"
#include "NfcService.h"

DeviceHost::DeviceHost()
{
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

void DeviceHost::notifyLlcpLinkActivation()
{
  nfc_service_send_MSG_LLCP_LINK_ACTIVATION();
}

void DeviceHost::notifyLlcpLinkDeactivated()
{
  nfc_service_send_MSG_LLCP_LINK_DEACTIVATION();
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
