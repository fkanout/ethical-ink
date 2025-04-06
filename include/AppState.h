#pragma once

#include <Arduino.h>

// Define your FSM states
enum AppState
{
    BOOTING,
    CHECKING_TIME,
    CONNECTING_WIFI,
    WAITING_FOR_TIME_SYNC,
    RUNNING_MAIN_TASK,
    SLEEPING
};

// Shared state variables
extern AppState state;
extern bool wifiAttempted;
extern unsigned long lastAttempt;