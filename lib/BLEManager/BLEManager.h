#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>

void setupBLE();
bool isNewBLEDataAvailable();  // Declare it here!
String getReceivedBLEData();

#endif // BLE_MANAGER_H