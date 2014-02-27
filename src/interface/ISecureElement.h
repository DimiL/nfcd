/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INTERFACE_SECURE_ELEMENT "SecureElement"

class ISecureElement {
public:
  virtual ~ISecureElement() {};

  virtual int doOpenSecureElementConnection() = 0;

  virtual int doDisconnect(int handle) = 0;

  virtual void doTransceive(int handle, std::vector<uint8_t>& input, std::vector<uint8_t>& ouput) = 0;

  virtual void doGetTechList(int handle, std::vector<uint32_t>& techlist) = 0;

  virtual void doGetUid(int handle, std::vector<uint8_t>& uidlist) = 0;
};
