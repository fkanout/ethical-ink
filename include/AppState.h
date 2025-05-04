#pragma once

#include <Arduino.h>

// Define your FSM states
enum AppState {
  BOOTING,
  CHECKING_TIME,
  CONNECTING_WIFI_WITH_SAVED_CREDENTIALS,
  CONNECTING_WIFI,
  SYNCING_TIME,
  ADVERTISING_BLE,
  WAITING_FOR_WIFI_SCAN,
  WAITING_FOR_TIME_SYNC,
  RUNNING_MAIN_TASK,
  RUNNING_PERIODIC_TASKS,
  SLEEPING,
  FETAL_ERROR,
};

// Shared state variables
extern AppState state;
extern bool wifiAttempted;
extern unsigned long lastAttempt;

constexpr const char *WIFI_CRED_FILE = "/wifi.json";
constexpr const char *MOSQUE_FILE = "/data.json";
constexpr const char *PRAYER_TIME_FILE_NAME = "/prayer_times_month_";
constexpr const char *IQAMA_TIME_FILE_NAME = "/iqama_times_month_";

struct Countdown {
  int hours;
  int minutes;
};