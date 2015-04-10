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

/**
 * Adjust the controller's power states.
 */
#include "PowerSwitch.h"

#include "config.h"
#include "NfcDebug.h"

// Bug 934835 : B2G NFC: NFC Daemon crash sometimes when enabled
// Set a delay between power off and power on,because libnfc-nci will crash if
// turn off then turn on immediatelly.
// 500 millis-seconds is experiment result.
#define PowerOnOffDelay 500   // milli-seconds.

static uint32_t TimeDiff(timespec aStart, timespec aEnd)
{
  timespec temp;
  if ((aEnd.tv_nsec - aStart.tv_nsec) < 0) {
    temp.tv_sec = aEnd.tv_sec - aStart.tv_sec-1;
    temp.tv_nsec = 1000000000 + aEnd.tv_nsec - aStart.tv_nsec;
  } else {
    temp.tv_sec = aEnd.tv_sec - aStart.tv_sec;
    temp.tv_nsec = aEnd.tv_nsec - aStart.tv_nsec;
  }
  return (temp.tv_sec * 1000) + (temp.tv_nsec / 1000000);
}

void DoStartupConfig();

PowerSwitch PowerSwitch::sPowerSwitch;
const PowerSwitch::PowerActivity PowerSwitch::DISCOVERY = 0x01;
const PowerSwitch::PowerActivity PowerSwitch::SE_ROUTING = 0x02;
const PowerSwitch::PowerActivity PowerSwitch::SE_CONNECTED = 0x04;

PowerSwitch::PowerSwitch()
 : mCurrLevel(UNKNOWN_LEVEL)
 , mCurrDeviceMgtPowerState(NFA_DM_PWR_STATE_UNKNOWN)
 , mDesiredScreenOffPowerState(0)
 , mCurrActivity(0)
{
  mLastPowerOffTime = (struct timespec){0, 0};
}

PowerSwitch::~PowerSwitch()
{
}

PowerSwitch& PowerSwitch::GetInstance()
{
  return sPowerSwitch;
}

void PowerSwitch::Initialize(PowerLevel aLevel)
{
  mMutex.Lock();

  NCI_DEBUG("level=%s (%u)", PowerLevelToString(aLevel), aLevel);
  GetNumValue(NAME_SCREEN_OFF_POWER_STATE, &mDesiredScreenOffPowerState, sizeof(mDesiredScreenOffPowerState));
  NCI_DEBUG("desired screen-off state=%d", mDesiredScreenOffPowerState);

  switch (aLevel) {
    case FULL_POWER:
      mCurrDeviceMgtPowerState = NFA_DM_PWR_MODE_FULL;
      mCurrLevel = aLevel;
      break;
    case UNKNOWN_LEVEL:
      mCurrDeviceMgtPowerState = NFA_DM_PWR_STATE_UNKNOWN;
      mCurrLevel = aLevel;
      break;
    default:
      NCI_ERROR("not handled");
      break;
  }
  mMutex.Unlock();
}

PowerSwitch::PowerLevel PowerSwitch::GetLevel()
{
  PowerLevel level = UNKNOWN_LEVEL;
  mMutex.Lock();
  level = mCurrLevel;
  mMutex.Unlock();
  return level;
}

bool PowerSwitch::SetLevel(PowerLevel aNewLevel)
{
  bool retval = false;

  mMutex.Lock();

  NCI_DEBUG("level=%s (%u)", PowerLevelToString(aNewLevel), aNewLevel);
  if (mCurrLevel == aNewLevel) {
    retval = true;
    goto TheEnd;
  }

  switch (aNewLevel) {
    case FULL_POWER:
      if (mCurrDeviceMgtPowerState == NFA_DM_PWR_MODE_OFF_SLEEP) {
        retval = SetPowerOffSleepState(false);
      }
      break;
    case LOW_POWER:
    case POWER_OFF:
      if (IsPowerOffSleepFeatureEnabled()) {
        retval = SetPowerOffSleepState(true);
      } else if (mDesiredScreenOffPowerState == 1) { //.conf file desires full-power.
        mCurrLevel = FULL_POWER;
        retval = true;
      }
      break;
    default:
      NCI_ERROR("not handled");
      break;
  }

TheEnd:
  mMutex.Unlock();
  return retval;
}

bool PowerSwitch::SetModeOff(PowerActivity aDeactivated)
{
  bool retVal = false;

  mMutex.Lock();
  mCurrActivity &= ~aDeactivated;
  retVal = mCurrActivity != 0;
  NCI_DEBUG("(deactivated=0x%x) : mCurrActivity=0x%x", aDeactivated, mCurrActivity);
  mMutex.Unlock();
  return retVal;
}

bool PowerSwitch::SetModeOn(PowerActivity aActivated)
{
  bool retVal = false;

  mMutex.Lock();
  mCurrActivity |= aActivated;
  retVal = mCurrActivity != 0;
  NCI_DEBUG("(activated=0x%x) : mCurrActivity=0x%x", aActivated, mCurrActivity);
  mMutex.Unlock();
  return retVal;
}

bool PowerSwitch::SetPowerOffSleepState(bool aSleep)
{
  NCI_DEBUG("enter; sleep=%u", aSleep);
  tNFA_STATUS stat = NFA_STATUS_FAILED;
  bool retval = false;

  if (aSleep) { // Enter power-off-sleep state.
    // Make sure the current power state is ON
    if (mCurrDeviceMgtPowerState != NFA_DM_PWR_MODE_OFF_SLEEP) {
      SyncEventGuard guard(mPowerStateEvent);

      NCI_DEBUG("try power off");
      stat = NFA_PowerOffSleepMode(TRUE);
      if (stat == NFA_STATUS_OK) {
        mPowerStateEvent.Wait();
        mCurrLevel = LOW_POWER;
        clock_gettime(CLOCK_REALTIME, &mLastPowerOffTime);
      } else {
        NCI_ERROR("API fail; stat=0x%X", stat);
        goto TheEnd;
      }
    } else {
      NCI_ERROR("power is not ON; curr device mgt power state=%s (%u)",
                DeviceMgtPowerStateToString(mCurrDeviceMgtPowerState),
                mCurrDeviceMgtPowerState);
      goto TheEnd;
    }
  } else { // Exit power-off-sleep state.
    // Make sure the current power state is OFF
    if (mCurrDeviceMgtPowerState != NFA_DM_PWR_MODE_FULL) {
      mCurrDeviceMgtPowerState = NFA_DM_PWR_STATE_UNKNOWN;
      SyncEventGuard guard(mPowerStateEvent);

      NCI_DEBUG("try full power");

      struct timespec now;
      clock_gettime(CLOCK_REALTIME, &now);
      int timediff = TimeDiff(mLastPowerOffTime, now);
      if (timediff < PowerOnOffDelay) {
        usleep((PowerOnOffDelay -timediff) * 1000);
      }
      stat = NFA_PowerOffSleepMode(FALSE);
      if (stat == NFA_STATUS_OK) {
        mPowerStateEvent.Wait();
        if (mCurrDeviceMgtPowerState != NFA_DM_PWR_MODE_FULL) {
          NCI_ERROR("unable to full power; curr device mgt power stat=%s (%u)",
                    DeviceMgtPowerStateToString(mCurrDeviceMgtPowerState),
                    mCurrDeviceMgtPowerState);
          goto TheEnd;
        }
        DoStartupConfig();
        mCurrLevel = FULL_POWER;
      } else {
        NCI_ERROR("API fail; stat=0x%X", stat);
        goto TheEnd;
      }
    } else {
      NCI_ERROR("not in power-off state; curr device mgt power state=%s (%u)",
                DeviceMgtPowerStateToString(mCurrDeviceMgtPowerState),
                mCurrDeviceMgtPowerState);
      goto TheEnd;
    }
  }

  retval = true;
TheEnd:
  NCI_DEBUG("exit; return %u", retval);
  return retval;
}

const char* PowerSwitch::DeviceMgtPowerStateToString(uint8_t aDeviceMgtPowerState)
{
  switch (aDeviceMgtPowerState) {
    case NFA_DM_PWR_MODE_FULL:
      return "DM-FULL";
    case NFA_DM_PWR_MODE_OFF_SLEEP:
      return "DM-OFF";
    default:
      return "DM-unknown????";
  }
}

const char* PowerSwitch::PowerLevelToString(PowerLevel aLevel)
{
  switch (aLevel) {
    case UNKNOWN_LEVEL:
      return "PS-UNKNOWN";
    case FULL_POWER:
      return "PS-FULL";
    case LOW_POWER:
      return "PS-LOW-POWER";
    case POWER_OFF:
      return "PS-POWER-OFF";
    default:
      return "PS-unknown????";
  }
}

void PowerSwitch::Abort()
{
  NCI_DEBUG("enter");
  SyncEventGuard guard(mPowerStateEvent);
  mPowerStateEvent.NotifyOne();
}

void PowerSwitch::DeviceManagementCallback(uint8_t aEvent,
                                           tNFA_DM_CBACK_DATA* aEventData)
{
  switch (aEvent) {
    case NFA_DM_PWR_MODE_CHANGE_EVT: {
      tNFA_DM_PWR_MODE_CHANGE& power_mode = aEventData->power_mode;
      NCI_DEBUG("NFA_DM_PWR_MODE_CHANGE_EVT; status=%u; device mgt power mode=%s (%u)",
                power_mode.status,
                sPowerSwitch.DeviceMgtPowerStateToString(power_mode.power_mode),
                power_mode.power_mode);
      SyncEventGuard guard(sPowerSwitch.mPowerStateEvent);
      if (power_mode.status == NFA_STATUS_OK) {
        sPowerSwitch.mCurrDeviceMgtPowerState = power_mode.power_mode;
      }
      sPowerSwitch.mPowerStateEvent.NotifyOne();
      break;
    }
  }
}

bool PowerSwitch::IsPowerOffSleepFeatureEnabled()
{
  return mDesiredScreenOffPowerState == 0;
}
