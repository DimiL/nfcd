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
#include "nfa_api.h"
#include "SyncEvent.h"
#include "sys/time.h"

/**
 * Adjust the controller's power states.
 */
class PowerSwitch
{
public:
  // Define the power level.
  enum PowerLevel {
    /**
     * Power level is unknown because the stack is off.
     */
    UNKNOWN_LEVEL,
    /**
     * Controller is in full-power state.
     */
    FULL_POWER,
    /**
     * Controller is in lower-power state.
     */
    LOW_POWER,
    /**
     * Controller power is off.
     */
    POWER_OFF
  };

  typedef int PowerActivity;

  /**
   * Discovery is enabled.
   */
  static const PowerActivity DISCOVERY;
  /**
   * Routing to SE is enabled.
   */
  static const PowerActivity SE_ROUTING;
  /**
   * SE is connected.
   */
  static const PowerActivity SE_CONNECTED;

  /**
   * Power level is unknown.
   */
  static const int PLATFORM_UNKNOWN_LEVEL = 0;
  /**
   * The phone is turned OFF.
   */
  static const int PLATFORM_POWER_OFF = 1;
  /**
   * The phone is turned ON but screen is OFF.
   */
  static const int PLATFORM_SCREEN_OFF = 2;
  /**
   * The phone is turned ON, screen is ON but user locked.
   */
  static const int PLATFORM_SCREEN_ON_LOCKED = 3;
  /**
   * The phone is turned ON, screen is ON, and user unlocked.
   */
  static const int PLATFORM_SCREEN_ON_UNLOCKED = 4;

  static const int VBAT_MONITOR_ENABLED = 1;
  static const int VBAT_MONITOR_PRIMARY_THRESHOLD = 5;
  static const int VBAT_MONITOR_SECONDARY_THRESHOLD = 8;

  PowerSwitch();
  ~PowerSwitch();

  /**
   * Get the singleton of this object.
   *
   * @return Reference to this object.
   */
  static PowerSwitch& getInstance();

  /**
   * Initialize member variables.
   *
   * @param  level Set the controller's power level.
   * @return       None.
   */
  void initialize(PowerLevel level);

  /**
   * Callback function for the stack.
   *
   * @param  event     Event ID.
   * @parm   eventData Event's data.
   * @return           None.
   */

  static void deviceManagementCallback(UINT8 event, tNFA_DM_CBACK_DATA* eventData);

  /**
   * Get the current power level of the controller.
   *
   * @return Power level.
   */
  PowerLevel getLevel();

  /**
   * Set the controller's power level.
   *
   * @param  level Set the controller's power level.
   * @return       True if ok.
   */
  bool setLevel(PowerLevel level);

  /**
   * Set a mode to be deactive.
   *
   * @return True if any mode is still active.
   */
  bool setModeOff(PowerActivity deactivated);

  /**
   * Set a mode to be active.
   *
   * @return True if any mode is active.
   */
  bool setModeOn(PowerActivity activated);

  /**
   * Whether power-off-sleep feature is enabled in .conf file.
   *
   * @return True if feature is enabled.
   */
  bool isPowerOffSleepFeatureEnabled();

  /**
   * Abort and unblock currrent operation.
   *
   * @return None.
   */
  void abort();

private:
  /**
   * Adjust controller's power-off-sleep state.
   *
   * @param  sleep Whether to enter sleep state.
   * @return       True if ok.
   */
  bool setPowerOffSleepState(bool sleep);

  /**
   * Decode power level to a string.
   *
   * @param  deviceMgtPowerState Power level.
   * @return                     Text representation of power level.
   */
  const char* deviceMgtPowerStateToString(UINT8 deviceMgtPowerState);

  /**
   * Decode power level to a string.
   *
   * @param  level Power level.
   * @return       Text representation of power level.
   */
  const char* powerLevelToString(PowerLevel level);

  // Singleton object.
  static PowerSwitch sPowerSwitch;
  // Device management power state power state is unknown.
  static const UINT8 NFA_DM_PWR_STATE_UNKNOWN = -1;

  PowerLevel mCurrLevel;
  // Device management power state; such as NFA_DM_PWR_STATE_???
  UINT8 mCurrDeviceMgtPowerState;
  // Read from .conf file; 0=power-off-sleep; 1=full power; 2=CE4 power.
  int mDesiredScreenOffPowerState;
  SyncEvent mPowerStateEvent;
  PowerActivity mCurrActivity;
  Mutex mMutex;
  struct timespec mLastPowerOffTime;
};
