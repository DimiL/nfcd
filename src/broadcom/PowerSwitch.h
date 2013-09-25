/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Adjust the controller's power states.
 */
#pragma once
#include "nfa_api.h"
#include "SyncEvent.h"

class PowerSwitch
{
public:

  enum PowerLevel {UNKNOWN_LEVEL, FULL_POWER, LOW_POWER, POWER_OFF};

  typedef int PowerActivity;
  static const PowerActivity DISCOVERY;
  static const PowerActivity SE_ROUTING;
  static const PowerActivity SE_CONNECTED;

  static const int PLATFORM_UNKNOWN_LEVEL = 0;
  static const int PLATFORM_POWER_OFF = 1;
  static const int PLATFORM_SCREEN_OFF = 2;
  static const int PLATFORM_SCREEN_ON_LOCKED = 3;
  static const int PLATFORM_SCREEN_ON_UNLOCKED = 4;

  static const int VBAT_MONITOR_ENABLED = 1;
  static const int VBAT_MONITOR_PRIMARY_THRESHOLD = 5;
  static const int VBAT_MONITOR_SECONDARY_THRESHOLD = 8;

  PowerSwitch ();
  ~PowerSwitch ();

  static PowerSwitch& getInstance();
  static void deviceManagementCallback(UINT8 event, tNFA_DM_CBACK_DATA* eventData);

  PowerLevel getLevel();
  void initialize(PowerLevel level);
  bool setLevel(PowerLevel level);
  bool setModeOff(PowerActivity deactivated);
  bool setModeOn(PowerActivity activated);
  bool isPowerOffSleepFeatureEnabled();

  void abort();

private:
  static PowerSwitch sPowerSwitch; //singleton object
  static const UINT8 NFA_DM_PWR_STATE_UNKNOWN = -1; //device management power state power state is unknown
 
  bool setPowerOffSleepState(bool sleep);
  const char* deviceMgtPowerStateToString(UINT8 deviceMgtPowerState);
  const char* powerLevelToString(PowerLevel level);

  PowerLevel mCurrLevel;
  UINT8 mCurrDeviceMgtPowerState; //device management power state; such as NFA_DM_PWR_STATE_???
  int mDesiredScreenOffPowerState; //read from .conf file; 0=power-off-sleep; 1=full power; 2=CE4 power
  SyncEvent mPowerStateEvent;
  PowerActivity mCurrActivity;
  Mutex mMutex;
};
