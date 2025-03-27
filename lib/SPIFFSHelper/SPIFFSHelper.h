#ifndef SPIFFS_HELPER_H
#define SPIFFS_HELPER_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>  // Use LittleFS if configured

bool setupSPIFFS();
String readJsonFile();
bool writeJsonFile(const String &jsonData);

#endif  