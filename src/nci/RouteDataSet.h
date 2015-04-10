/*
 * Copyright (C) 2013-2014  Mozilla Foundation
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

#include <vector>
#include <string>

extern "C"
{
  #include "nfa_api.h"
}

/**
 * Base class for every kind of route data.
 */
class RouteData
{
public:
  enum RouteType {ProtocolRoute, TechnologyRoute};
  RouteType mRouteType;

protected:
  RouteData(RouteType routeType)
    : mRouteType(routeType)
  {}
};

/**
 * Data for protocol routes.
 */
class RouteDataForProtocol : public RouteData
{
public:
  int mNfaEeHandle; //for example 0x4f3, 0x4f4
  bool mSwitchOn;
  bool mSwitchOff;
  bool mBatteryOff;
  tNFA_PROTOCOL_MASK mProtocol;

  RouteDataForProtocol()
    : RouteData(ProtocolRoute)
    , mNfaEeHandle(NFA_HANDLE_INVALID)
    , mSwitchOn(false)
    , mSwitchOff(false)
    , mBatteryOff(false)
    , mProtocol(0)
  {}
};

/**
 * Data for technology routes.
 */
class RouteDataForTechnology : public RouteData
{
public:
  int mNfaEeHandle; //for example 0x4f3, 0x4f4
  bool mSwitchOn;
  bool mSwitchOff;
  bool mBatteryOff;
  tNFA_TECHNOLOGY_MASK mTechnology;

  RouteDataForTechnology()
    : RouteData(TechnologyRoute)
    , mNfaEeHandle(NFA_HANDLE_INVALID)
    , mSwitchOn(false)
    , mSwitchOff(false)
    , mBatteryOff(false)
    , mTechnology(0)
  {}
};

class RouteDataSet
{
public:
  typedef std::vector<RouteData*> Database;
  enum DatabaseSelection {DefaultRouteDatabase, SecElemRouteDatabase};

  ~RouteDataSet();

  /**
   * Initialize resources.
   *
   * @return True if ok.
   */
  bool Initialize();

  /**
   * Import data from an XML file.  Fill the database.
   *
   * @return True if ok.
   */
  bool Import();

private:
  Database mSecElemRouteDatabase; //routes when NFC service selects sec elem.
  Database mDefaultRouteDatabase; //routes when NFC service deselects sec elem.
  Database* mCurrentDB;
  static const char* sConfigFile;

  /**
   * Delete all routes stored in all databases.
   *
   * @return None.
   */
  void DeleteDatabase();

  /**
   * Parse data for protocol routes.
   *
   * @param  aAttribute Array of XML property and value pair of a XML node
   * @return None.
   */
  void ImportProtocolRoute(const char** aAttribute);

  /**
   * Parse data for technology routes.
   *
   * @param  attribute Array of XML property and value pair of a XML node
   * @return None.
   */
  void ImportTechnologyRoute(const char** aAttribute);

  /**
   * Callback function when expat-lib process a start tag.
   *
   * @param  Data      User data set in XML_SetUserData function.
   * @param  aElement   The tag node
   * @param  aAttribute Array of the attribute name and value of the given tag.
   * @return None.
   */
  static void XmlStartElement(void* aData,
                              const char* aElement,
                              const char** aAttribute);

  /**
   * Callback function when expat-lib process a end tag.
   *
   * @param  aData     User data set in XML_SetUserData function.
   * @param  aelement  The tag node.
   * @param  attribute Array of the attribute name and value of the given tag.
   * @return None.
   */
  static void XmlEndElement(void* aData,
                            const char* aElement);
};
