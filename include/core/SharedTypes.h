#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "PetKitApi.h" // Ensure this library is available

// Forward declaration to avoid including full heavy headers if possible
// but for these structs we usually need the definitions.

// Define the structure for our data map: PetID -> Timestamp -> Record
typedef std::map<int, std::map<time_t, SL_Record>> PetDataMap;

enum DateRangeEnum {
  LAST_7_DAYS,
  LAST_30_DAYS,
  LAST_90_DAYS,
  LAST_365_DAYS,
  Date_Range_Max
};

struct DateRangeInfo {
  DateRangeEnum type;
  char name[32];
  long seconds;
};

struct env_data
{
  float temperature;
  float humidity;
  time_t timestamp;
};

struct SystemConfig {
    int sleep_interval_min = 120;           // Default 2 hours
    int sleep_interval_low_batt_min = 360;  // Default 6 hours
    float battery_low_threshold_v = 3.50;   // Default 3.5V
};

// Global constants for NVS keys
#define NVS_NAMESPACE "petkitplotter"
#define NVS_PLOT_RANGE_KEY "plotrange"
#define NVS_PETS_KEY "pets"

#endif