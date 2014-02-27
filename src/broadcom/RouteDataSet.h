/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once
#include <libxml/parser.h>
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

class AidBuffer
{
public:
  /**
   * Parse a string of hex numbers.  Store result in an array ofbytes.
   *
   * @param  aid string of hex numbers.
   * @return None.
   */
  AidBuffer(std::string& aid);
  ~AidBuffer();

  uint8_t* buffer() { return mBuffer; };
  int length() { return mBufferLen; };

private:
  uint8_t* mBuffer;
  uint32_t mBufferLen;
};

/**
 * Import and export general routing data using a XML file.
 * See /data/bcm/param/route.xml
 */
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
  bool initialize();

  /**
   * Import data from an XML file.  Fill the database.
   *
   * @return True if ok.
   */
  bool import();

  /**
   * Obtain a database of routing data.
   *
   * @param  selection which database.
   * @return Pointer to database.
   */
  Database* getDatabase(DatabaseSelection selection);

  /**
   * Save XML data from a string into a file.
   *
   * @param  routesXml XML that represents routes.
   * @return  True if ok.
   */
  static bool saveToFile(const char* routesXml);

  /**
   * Load XML data from file into a string.
   *
   * @param  routesXml string to receive XML data.
   * @return True if ok.
   */
  static bool loadFromFile(std::string& routesXml);

  /**
   * Delete route data XML file.
   *
   * @return True if ok.
   */
  static bool deleteFile();
  /**
   * Print some diagnostic output
   *
   * @return None
   */
  void printDiagnostic();

private:
  Database mSecElemRouteDatabase; //routes when NFC service selects sec elem
  Database mDefaultRouteDatabase; //routes when NFC service deselects sec elem
  static const char* sConfigFile;
  static const bool sDebug = false;

  /**
   * Delete all routes stored in all databases.
   *
   * @return None.
   */
  void deleteDatabase();

  /**
   * Parse data for protocol routes.
   *
   * @param  element XML node for one protocol route.
   * @param  database store data in this database.
   * @return None.
   */
   void importProtocolRoute(xmlNodePtr& element, Database& database);

  /**
   * Parse data for technology routes.
   *
   * @param  element XML node for one technology route.
   * @param  database store data in this database.
   * @return None.
   */
  void importTechnologyRoute(xmlNodePtr& element, Database& database);
};
