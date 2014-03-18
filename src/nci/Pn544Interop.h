/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

/**
 * Stop polling to let NXP PN544 controller poll.
 * PN544 should activate in P2P mode.
 *
 * @return None.
 */
void pn544InteropStopPolling();

/**
 * Is the code performing operations?
 *
 * @return True if the code is busy.
 */
bool pn544InteropIsBusy();

/**
 * Request to abort all operations.
 *
 * @return None.
 */
void pn544InteropAbortNow();
