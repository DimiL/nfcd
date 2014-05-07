/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nfcd_ISecureElement_h
#define mozilla_nfcd_ISecureElement_h

#define INTERFACE_SECURE_ELEMENT "SecureElement"

class ISecureElement {
public:
  virtual ~ISecureElement() {};

  /**
   * Connect to the secure element.
   *
   * @return Handle of secure element. values < 0 represent failure.
   */
  virtual int doOpenSecureElementConnection() = 0;

  /**
   * Disconnect from the secure element.
   *
   * @param  handle Handle of secure element.
   * @return True if ok.
   */
  virtual int doDisconnect(int handle) = 0;

  /**
   * Send data to the secure element; retrieve response.
   *
   * @param  handle Secure element's handle.
   * @param  input  Data to send.
   * @param  output Buffer of received data.
   * @return True if ok.
   */
  virtual bool doTransceive(int handle, std::vector<uint8_t>& input, std::vector<uint8_t>& ouput) = 0;
};

#endif
