/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionId.h"

int SessionId::mId = 0;

int
SessionId::generateNewId() {
  return ++mId;
}

int
SessionId::getCurrentId() {
  return mId;
}

bool
SessionId::isValid(int id) {
  return mId == id;
}
