/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Adjust the controller's power states.
 */
#include "PowerSwitch.h"

#include "config.h"

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

// Bug 934835 : B2G NFC: NFC Daemon crash sometimes when enabled
// Set a delay between power off and power on,because libnfc-nci will crash if
// turn off then turn on immediatelly.
// 500 millis-seconds is experiment result.
#define PowerOnOffDelay 500   // milli-seconds.

static UINT32 TimeDiff(timespec start, timespec end)
{
  timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return (temp.tv_sec * 1000) + (temp.tv_nsec / 1000000);
}

void doStartupConfig();

PowerSwitch PowerSwitch::sPowerSwitch;
const PowerSwitch::PowerActivity PowerSwitch::DISCOVERY=0x01;
const PowerSwitch::PowerActivity PowerSwitch::SE_ROUTING=0x02;
const PowerSwitch::PowerActivity PowerSwitch::SE_CONNECTED=0x04;

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

PowerSwitch& PowerSwitch::getInstance()
{
  return sPowerSwitch;
}

void PowerSwitch::initialize(PowerLevel level)
{
  mMutex.lock();

  ALOGD("%s: level=%s (%u)", __FUNCTION__, powerLevelToString(level), level);
  GetNumValue(NAME_SCREEN_OFF_POWER_STATE, &mDesiredScreenOffPowerState, sizeof(mDesiredScreenOffPowerState));
  ALOGD("%s: desired screen-off state=%d", __FUNCTION__, mDesiredScreenOffPowerState);

  switch (level) {
    case FULL_POWER:
      mCurrDeviceMgtPowerState = NFA_DM_PWR_MODE_FULL;
      mCurrLevel = level;
      break;
    case UNKNOWN_LEVEL:
      mCurrDeviceMgtPowerState = NFA_DM_PWR_STATE_UNKNOWN;
      mCurrLevel = level;
      break;
    default:
      ALOGE("%s: not handled", __FUNCTION__);
      break;
  }
  mMutex.unlock();
}

PowerSwitch::PowerLevel PowerSwitch::getLevel()
{
  PowerLevel level = UNKNOWN_LEVEL;
  mMutex.lock();
  level = mCurrLevel;
  mMutex.unlock();
  return level;
}

bool PowerSwitch::setLevel(PowerLevel newLevel)
{
  bool retval = false;

  mMutex.lock();

  ALOGD("%s: level=%s (%u)", __FUNCTION__, powerLevelToString(newLevel), newLevel);
  if (mCurrLevel == newLevel) {
    retval = true;
    goto TheEnd;
  }

  switch (newLevel) {
    case FULL_POWER:
      if (mCurrDeviceMgtPowerState == NFA_DM_PWR_MODE_OFF_SLEEP)
        retval = setPowerOffSleepState(false);
      break;
    case LOW_POWER:
    case POWER_OFF:
      if (isPowerOffSleepFeatureEnabled()) {
        retval = setPowerOffSleepState(true);
      } else if (mDesiredScreenOffPowerState == 1) { //.conf file desires full-power.
        mCurrLevel = FULL_POWER;
        retval = true;
      }
      break;
    default:
      ALOGE("%s: not handled", __FUNCTION__);
      break;
  }

TheEnd:
  mMutex.unlock();
  return retval;
}

bool PowerSwitch::setModeOff(PowerActivity deactivated)
{
  bool retVal = false;

  mMutex.lock();
  mCurrActivity &= ~deactivated;
  retVal = mCurrActivity != 0;
  ALOGD("%s: (deactivated=0x%x) : mCurrActivity=0x%x", __FUNCTION__, deactivated, mCurrActivity);
  mMutex.unlock();
  return retVal;
}

bool PowerSwitch::setModeOn(PowerActivity activated)
{
  bool retVal = false;

  mMutex.lock();
  mCurrActivity |= activated;
  retVal = mCurrActivity != 0;
  ALOGD("%s: (activated=0x%x) : mCurrActivity=0x%x", __FUNCTION__, activated, mCurrActivity);
  mMutex.unlock();
  return retVal;
}

bool PowerSwitch::setPowerOffSleepState(bool sleep)
{
  ALOGD("%s: enter; sleep=%u", __FUNCTION__, sleep);
  tNFA_STATUS stat = NFA_STATUS_FAILED;
  bool retval = false;

  if (sleep) { // Enter power-off-sleep state.
    // Make sure the current power state is ON
    if (mCurrDeviceMgtPowerState != NFA_DM_PWR_MODE_OFF_SLEEP) {
      SyncEventGuard guard(mPowerStateEvent);

      ALOGD("%s: try power off", __FUNCTION__);
      stat = NFA_PowerOffSleepMode(TRUE);
      if (stat == NFA_STATUS_OK) {
        mPowerStateEvent.wait();
        mCurrLevel = LOW_POWER;
        clock_gettime(CLOCK_REALTIME, &mLastPowerOffTime);
      } else {
        ALOGE("%s: API fail; stat=0x%X", __FUNCTION__, stat);
        goto TheEnd;
      }
    } else {
      ALOGE("%s: power is not ON; curr device mgt power state=%s (%u)", __FUNCTION__,
        deviceMgtPowerStateToString(mCurrDeviceMgtPowerState), mCurrDeviceMgtPowerState);
      goto TheEnd;
    }
  } else { // Exit power-off-sleep state.
    // Make sure the current power state is OFF
    if (mCurrDeviceMgtPowerState != NFA_DM_PWR_MODE_FULL) {
      mCurrDeviceMgtPowerState = NFA_DM_PWR_STATE_UNKNOWN;
      SyncEventGuard guard(mPowerStateEvent);

      ALOGD("%s: try full power", __FUNCTION__);

      struct timespec now;
      clock_gettime(CLOCK_REALTIME, &now);
      int timediff = TimeDiff(mLastPowerOffTime, now);
      if (timediff < PowerOnOffDelay) {
        usleep((PowerOnOffDelay -timediff) * 1000);
      }
      stat = NFA_PowerOffSleepMode(FALSE);
      if (stat == NFA_STATUS_OK) {
        mPowerStateEvent.wait();
        if (mCurrDeviceMgtPowerState != NFA_DM_PWR_MODE_FULL) {
          ALOGE("%s: unable to full power; curr device mgt power stat=%s (%u)", __FUNCTION__,
            deviceMgtPowerStateToString(mCurrDeviceMgtPowerState), mCurrDeviceMgtPowerState);
          goto TheEnd;
        }
        doStartupConfig();
        mCurrLevel = FULL_POWER;
      } else {
        ALOGE("%s: API fail; stat=0x%X", __FUNCTION__, stat);
        goto TheEnd;
      }
    } else {
      ALOGE("%s: not in power-off state; curr device mgt power state=%s (%u)", __FUNCTION__,
        deviceMgtPowerStateToString(mCurrDeviceMgtPowerState), mCurrDeviceMgtPowerState);
      goto TheEnd;
    }
  }

  retval = true;
TheEnd:
  ALOGD("%s: exit; return %u", __FUNCTION__, retval);
  return retval;
}

const char* PowerSwitch::deviceMgtPowerStateToString(UINT8 deviceMgtPowerState)
{
  switch (deviceMgtPowerState) {
    case NFA_DM_PWR_MODE_FULL:
      return "DM-FULL";
    case NFA_DM_PWR_MODE_OFF_SLEEP:
      return "DM-OFF";
    default:
      return "DM-unknown????";
  }
}

const char* PowerSwitch::powerLevelToString(PowerLevel level)
{
  switch (level) {
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

void PowerSwitch::abort()
{
  ALOGD("%s", __FUNCTION__);
  SyncEventGuard guard(mPowerStateEvent);
  mPowerStateEvent.notifyOne();
}

void PowerSwitch::deviceManagementCallback(UINT8 event, tNFA_DM_CBACK_DATA* eventData)
{
  switch (event) {
    case NFA_DM_PWR_MODE_CHANGE_EVT: {
      tNFA_DM_PWR_MODE_CHANGE& power_mode = eventData->power_mode;
      ALOGD("%s: NFA_DM_PWR_MODE_CHANGE_EVT; status=%u; device mgt power mode=%s (%u)", __FUNCTION__,
        power_mode.status, sPowerSwitch.deviceMgtPowerStateToString(power_mode.power_mode), power_mode.power_mode);
      SyncEventGuard guard(sPowerSwitch.mPowerStateEvent);
      if (power_mode.status == NFA_STATUS_OK)
        sPowerSwitch.mCurrDeviceMgtPowerState = power_mode.power_mode;
      sPowerSwitch.mPowerStateEvent.notifyOne();
      break;
    }
  }
}

bool PowerSwitch::isPowerOffSleepFeatureEnabled()
{
  return mDesiredScreenOffPowerState == 0;
}
