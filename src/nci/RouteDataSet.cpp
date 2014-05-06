/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RouteDataSet.h"
#include "libxml/xmlmemory.h"
#include <errno.h>
#include <sys/stat.h>

#undef LOG_TAG
#define LOG_TAG "NfcNci"
#include <cutils/log.h>

extern char bcm_nfc_location[];

AidBuffer::AidBuffer(std::string& aid)
 : mBuffer(NULL)
 , mBufferLen(0)
{
  unsigned int num = 0;
  const char delimiter = ':';
  std::string::size_type pos1 = 0;
  std::string::size_type pos2 = aid.find_first_of(delimiter);

  //parse the AID string; each hex number is separated by a colon;
  mBuffer = new uint8_t[aid.length()];
  while(true) {
    num = 0;
    if (pos2 == std::string::npos) {
      sscanf(aid.substr(pos1).c_str(), "%x", &num);
      mBuffer[mBufferLen] = (uint8_t)num;
      mBufferLen++;
      break;
    } else {
      sscanf(aid.substr(pos1, pos2-pos1+1).c_str(), "%x", &num);
      mBuffer[mBufferLen] = (uint8_t)num;
      mBufferLen++;
      pos1 = pos2 + 1;
      pos2 = aid.find_first_of(delimiter, pos1);
    }
  }
}

AidBuffer::~AidBuffer()
{
  delete[] mBuffer;
}

const char* RouteDataSet::sConfigFile = "/param/route.xml";

RouteDataSet::~RouteDataSet()
{
  deleteDatabase();
}

bool RouteDataSet::initialize()
{
  ALOGD("%s: enter", __FUNCTION__);
  //check that the libxml2 version in use is compatible
  //with the version the software has been compiled with
  // TODO : What's this ...
  //LIBXML_TEST_VERSION
  ALOGD("%s: exit; return=true", __FUNCTION__);
  return true;
}

void RouteDataSet::deleteDatabase()
{
  ALOGD("%s: default db size=%u; sec elem db size=%u",
        __FUNCTION__, mDefaultRouteDatabase.size(), mSecElemRouteDatabase.size());
  Database::iterator it;

  for (it = mDefaultRouteDatabase.begin(); it != mDefaultRouteDatabase.end(); it++) {
    delete (*it);
  }
  mDefaultRouteDatabase.clear();

  for (it = mSecElemRouteDatabase.begin(); it != mSecElemRouteDatabase.end(); it++) {
    delete (*it);
  }
  mSecElemRouteDatabase.clear();
}

bool RouteDataSet::import()
{
  ALOGD("%s: enter", __FUNCTION__);
  bool retval = false;
  xmlDocPtr doc;
  xmlNodePtr node1;
  std::string strFilename(bcm_nfc_location);
  strFilename += sConfigFile;

  deleteDatabase();

  doc = xmlParseFile(strFilename.c_str());
  if (doc == NULL) {
    ALOGD("%s: fail parse", __FUNCTION__);
    goto TheEnd;
  }

  node1 = xmlDocGetRootElement(doc);
  if (node1 == NULL) {
    ALOGE("%s: fail root element", __FUNCTION__);
    goto TheEnd;
  }
  ALOGD("%s: root=%s", __FUNCTION__, node1->name);

  node1 = node1->xmlChildrenNode;

  while (node1) { //loop through all elements in <Routes ...
    if (xmlStrcmp(node1->name, (const xmlChar*) "Route") == 0) {
      xmlChar* value = xmlGetProp(node1, (const xmlChar*) "Type");
      if (value && (xmlStrcmp(value, (const xmlChar*) "SecElemSelectedRoutes") == 0)) {
        ALOGD("%s: found SecElemSelectedRoutes", __FUNCTION__);
        xmlNodePtr node2 = node1->xmlChildrenNode;
        while (node2) {//loop all elements in <Route Type="SecElemSelectedRoutes" ...
          if (xmlStrcmp(node2->name, (const xmlChar*) "Proto")==0) {
            importProtocolRoute(node2, mSecElemRouteDatabase);
          } else if (xmlStrcmp(node2->name, (const xmlChar*) "Tech")==0) {
            importTechnologyRoute (node2, mSecElemRouteDatabase);
          }
          node2 = node2->next;
        } //loop all elements in <Route Type="SecElemSelectedRoutes" ...
      } else if (value && (xmlStrcmp (value, (const xmlChar*) "DefaultRoutes") == 0)) {
        ALOGD("%s: found DefaultRoutes", __FUNCTION__);
        xmlNodePtr node2 = node1->xmlChildrenNode;
        while (node2) { //loop all elements in <Route Type="DefaultRoutes" ...
          if (xmlStrcmp(node2->name, (const xmlChar*) "Proto") == 0) {
            importProtocolRoute(node2, mDefaultRouteDatabase);
          } else if (xmlStrcmp(node2->name, (const xmlChar*) "Tech") == 0) {
            importTechnologyRoute(node2, mDefaultRouteDatabase);
          }
          node2 = node2->next;
        } //loop all elements in <Route Type="DefaultRoutes" ...
      }
      if (value) {
        xmlFree(value);
      }
    } //check <Route ...
     node1 = node1->next;
  } //loop through all elements in <Routes ...
  retval = true;

TheEnd:
  xmlFreeDoc(doc);
  xmlCleanupParser();
  ALOGD("%s: exit; return=%u", __FUNCTION__, retval);
  return retval;
}

bool RouteDataSet::saveToFile(const char* routesXml)
{
  FILE* fh = NULL;
  size_t actualWritten = 0;
  bool retval = false;
  std::string filename(bcm_nfc_location);

  filename.append(sConfigFile);
  fh = fopen(filename.c_str(), "w");
  if (fh == NULL) {
    ALOGE("%s: fail to open file", __FUNCTION__);
    return false;
  }

  actualWritten = fwrite(routesXml, sizeof(char), strlen(routesXml), fh);
  retval = actualWritten == strlen(routesXml);
  fclose(fh);
  ALOGD("%s: wrote %u bytes", __FUNCTION__, actualWritten);
  if (retval == false) {
    ALOGE("%s: error during write", __FUNCTION__);
  }

  //set file permission to
  //owner read, write; group read; other read
  chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  return retval;
}

bool RouteDataSet::loadFromFile(std::string& routesXml)
{
  FILE* fh = NULL;
  size_t actual = 0;
  char buffer [1024];
  std::string filename(bcm_nfc_location);

  filename.append(sConfigFile);
  fh = fopen(filename.c_str(), "r");
  if (fh == NULL) {
    ALOGD("%s: fail to open file", __FUNCTION__);
    return false;
  }

  while (true) {
    actual = fread(buffer, sizeof(char), sizeof(buffer), fh);
    if (actual == 0) {
      break;
    }
    routesXml.append(buffer, actual);
  }
  fclose(fh);
  ALOGD("%s: read %u bytes", __FUNCTION__, routesXml.length());
  return true;
}


void RouteDataSet::importProtocolRoute(xmlNodePtr& element, Database& database)
{
  const xmlChar* id = (const xmlChar*) "Id";
  const xmlChar* secElem = (const xmlChar*) "SecElem";
  const xmlChar* trueString = (const xmlChar*) "true";
  const xmlChar* switchOn = (const xmlChar*) "SwitchOn";
  const xmlChar* switchOff = (const xmlChar*) "SwitchOff";
  const xmlChar* batteryOff = (const xmlChar*) "BatteryOff";
  RouteDataForProtocol* data = new RouteDataForProtocol;
  xmlChar* value = NULL;

  //ALOGD_IF (sDebug, "%s: element=%s", fn, element->name);
  value = xmlGetProp(element, id);
  if (value) {
    if (xmlStrcmp(value, (const xmlChar*) "T1T") == 0) {
      data->mProtocol = NFA_PROTOCOL_MASK_T1T;
    } else if (xmlStrcmp(value, (const xmlChar*) "T2T") == 0) {
      data->mProtocol = NFA_PROTOCOL_MASK_T2T;
    } else if (xmlStrcmp(value, (const xmlChar*) "T3T") == 0) {
      data->mProtocol = NFA_PROTOCOL_MASK_T3T;
    } else if (xmlStrcmp(value, (const xmlChar*) "IsoDep") == 0) {
      data->mProtocol = NFA_PROTOCOL_MASK_ISO_DEP;
    }
    xmlFree(value);
    //ALOGD_IF (sDebug, "%s: %s=0x%X", fn, id, data->mProtocol);
  }

  value = xmlGetProp(element, secElem);
  if (value) {
    data->mNfaEeHandle = strtol((char*) value, NULL, 16);
    xmlFree(value);
    data->mNfaEeHandle = data->mNfaEeHandle | NFA_HANDLE_GROUP_EE;
    //ALOGD_IF(sDebug, "%s: %s=0x%X", fn, secElem, data->mNfaEeHandle);
  }

  value = xmlGetProp(element, switchOn);
  if (value) {
    data->mSwitchOn = (xmlStrcmp(value, trueString) == 0);
    xmlFree(value);
  }

  value = xmlGetProp(element, switchOff);
  if (value) {
    data->mSwitchOff = (xmlStrcmp(value, trueString) == 0);
    xmlFree(value);
  }

  value = xmlGetProp(element, batteryOff);
  if (value) {
    data->mBatteryOff = (xmlStrcmp(value, trueString) == 0);
    xmlFree(value);
  }
  database.push_back(data);
}

void RouteDataSet::importTechnologyRoute(xmlNodePtr& element, Database& database)
{
  const xmlChar* id = (const xmlChar*) "Id";
  const xmlChar* secElem = (const xmlChar*) "SecElem";
  const xmlChar* trueString = (const xmlChar*) "true";
  const xmlChar* switchOn = (const xmlChar*) "SwitchOn";
  const xmlChar* switchOff = (const xmlChar*) "SwitchOff";
  const xmlChar* batteryOff = (const xmlChar*) "BatteryOff";
  RouteDataForTechnology* data = new RouteDataForTechnology;
  xmlChar* value = NULL;

  //ALOGD_IF (sDebug, "%s: element=%s", fn, element->name);
  value = xmlGetProp(element, id);
  if (value) {
    if (xmlStrcmp (value, (const xmlChar*) "NfcA") == 0) {
      data->mTechnology = NFA_TECHNOLOGY_MASK_A;
    } else if (xmlStrcmp (value, (const xmlChar*) "NfcB") == 0) {
      data->mTechnology = NFA_TECHNOLOGY_MASK_B;
    } else if (xmlStrcmp (value, (const xmlChar*) "NfcF") == 0) {
      data->mTechnology = NFA_TECHNOLOGY_MASK_F;
    }
    xmlFree(value);
    //ALOGD_IF (sDebug, "%s: %s=0x%X", fn, id, data->mTechnology);
  }

  value = xmlGetProp(element, secElem);
  if (value) {
    data->mNfaEeHandle = strtol((char*) value, NULL, 16);
    xmlFree(value);
    data->mNfaEeHandle = data->mNfaEeHandle | NFA_HANDLE_GROUP_EE;
    //ALOGD_IF (sDebug, "%s: %s=0x%X", fn, secElem, data->mNfaEeHandle);
  }

  value = xmlGetProp(element, switchOn);
  if (value) {
    data->mSwitchOn = (xmlStrcmp (value, trueString) == 0);
    xmlFree(value);
  }

  value = xmlGetProp(element, switchOff);
  if (value) {
    data->mSwitchOff = (xmlStrcmp (value, trueString) == 0);
    xmlFree(value);
  }

  value = xmlGetProp(element, batteryOff);
  if (value) {
    data->mBatteryOff = (xmlStrcmp (value, trueString) == 0);
    xmlFree(value);
  }
  database.push_back(data);
}

bool RouteDataSet::deleteFile()
{
  std::string filename(bcm_nfc_location);
  filename.append(sConfigFile);
  int stat = remove(filename.c_str());
  ALOGD("%s: exit %u", __FUNCTION__, stat==0);
  return stat == 0;
}

RouteDataSet::Database* RouteDataSet::getDatabase(DatabaseSelection selection)
{
  switch (selection) {
    case DefaultRouteDatabase:
      return &mDefaultRouteDatabase;
    case SecElemRouteDatabase:
      return &mSecElemRouteDatabase;
  }
  return NULL;
}

void RouteDataSet::printDiagnostic()
{
  Database* db = getDatabase(DefaultRouteDatabase);

  ALOGD("%s: default route database", __FUNCTION__);
  for (Database::iterator iter = db->begin(); iter != db->end(); iter++) {
    RouteData* routeData = *iter;
    switch (routeData->mRouteType) {
      case RouteData::ProtocolRoute:
      {
        RouteDataForProtocol* proto = (RouteDataForProtocol*) routeData;
        ALOGD("%s: ee h=0x%X; protocol=0x%X", __FUNCTION__, proto->mNfaEeHandle, proto->mProtocol);
      }
      break;
    // TODO: RouteData::TechnologyRoute isn't handled --- bug?
    }
  }

  ALOGD("%s: sec elem route database", __FUNCTION__);
  db = getDatabase(SecElemRouteDatabase);
  for (Database::iterator iter2 = db->begin(); iter2 != db->end(); iter2++) {
    RouteData* routeData = *iter2;
    switch (routeData->mRouteType) {
      case RouteData::ProtocolRoute:
        {
          RouteDataForProtocol* proto = (RouteDataForProtocol*) routeData;
          ALOGD("%s: ee h=0x%X; protocol=0x%X", __FUNCTION__, proto->mNfaEeHandle, proto->mProtocol);
        }
        break;
      case RouteData::TechnologyRoute:
        {
          RouteDataForTechnology* tech = (RouteDataForTechnology*) routeData;
          ALOGD("%s: ee h=0x%X; technology=0x%X", __FUNCTION__, tech->mNfaEeHandle, tech->mTechnology);
        }
        break;
    }
  }
}
