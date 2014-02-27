/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "SyncEvent.h"
#include "RouteDataSet.h"
#include <vector>

extern "C"
{
    #include "nfa_api.h"
}

class HostAidRouter
{
public:
  /**
   * Obtain a reference to the singleton object of HostAidRouter
   *
   * @return Reference to HostAidRouter object.
   */
  static HostAidRouter& getInstance();

  /**
   * Initialize all resources.
   *
   * @return True if ok.
   */
  bool initialize();

  /**
   * Route Proximity Payment System Environment request to the host.
   * This function is called when there is no route data.
   *
   * @return True if ok.
   */
  bool addPpseRoute();

  /**
   * Delete all AID routes to the host.
   *
   * @return True if ok.
   */
  bool deleteAllRoutes();

  /**
   * Is AID-routing-to-host feature enabled?
   *
   * @return True if enabled.
   */
  bool isFeatureEnabled () { return mIsFeatureEnabled; };
  
  /**
   * Begin to route AID request to the host.
   *
   * @param  aid Buffer that contains Application ID.
   * @param  aidLen Actual length of the buffer.
   * @return True if enabled.
   */
  bool startRoute (const UINT8* aid, UINT8 aidLen);

private:
  typedef std::vector<tNFA_HANDLE> AidHandleDatabase;

  tNFA_HANDLE mTempHandle;
  bool mIsFeatureEnabled;
  static HostAidRouter sHostAidRouter; //singleton object.
  RouteDataSet mRouteDataSet; //route data from xml file.
  SyncEvent mRegisterEvent;
  SyncEvent mDeregisterEvent;
  AidHandleDatabase mHandleDatabase; //store all AID handles that are registered with the stack.

  HostAidRouter();
  ~HostAidRouter();

  /**
   * Receive events from the NFC stack.
   *
   * @return None.
   */
  static void stackCallback (UINT8 event, tNFA_CONN_EVT_DATA* eventdata);
};
