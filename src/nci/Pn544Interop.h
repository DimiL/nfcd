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
