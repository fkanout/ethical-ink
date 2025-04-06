#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H
#include <Arduino.h>

class BLEManager
{
public:
    static void setupBLE();
    static bool isNewBLEDataAvailable();
    static String getReceivedBLEData();
    static void restartBLE();
    static void sendBLEData(const String &data);

private:
    static void startBLETimeout();
};

#endif // BLE_MANAGER_H