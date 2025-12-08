#include "App.h"

// Globals
DateRangeInfo dateRangeInfo[] = {
    {LAST_7_DAYS, "7 Days", 7 * 86400L},
    {LAST_30_DAYS, "30 Days", 30 * 86400L},
    {LAST_90_DAYS, "90 Days", 90 * 86400L},
    {LAST_365_DAYS, "365 Days", 365 * 86400L},
};

App::App() : hspi(HSPI), sht4() {
    networkManager = nullptr;
    plotManager = nullptr;
    display = nullptr;
}

/**
 * @brief Initialize all hardware components.
 * 
 * Sets up Serial, PSRAM, Pins (LED, EPD, Buttons, SD, Buzzer, Battery), 
 * I2C, ADC, SHT4x sensor, and the Display over SPI.
 */
void App::initHardware()
{
  Serial.begin(115200);
  psramInit();
  if (psramFound())
    Serial.println("Found and Initialized PSRAM");
  else
    Serial.println("No PSRAM Found");
  
  pinMode(Config::Pins::LED, OUTPUT);
  digitalWrite(Config::Pins::LED, LOW);
  
  pinMode(Config::Pins::EPD_RES, OUTPUT);
  pinMode(Config::Pins::EPD_DC, OUTPUT);
  pinMode(Config::Pins::EPD_CS, OUTPUT);

  pinMode(Config::Pins::BUTTON_KEY0, INPUT_PULLUP); // refresh
  pinMode(Config::Pins::BUTTON_KEY1, INPUT_PULLUP); // date range ++
  pinMode(Config::Pins::BUTTON_KEY2, INPUT_PULLUP); // date range --

  pinMode(Config::Pins::SD_EN, OUTPUT);
  digitalWrite(Config::Pins::SD_EN, HIGH);
  pinMode(Config::Pins::SD_DET, INPUT_PULLUP);

  pinMode(Config::Pins::BUZZER, OUTPUT);

  pinMode(Config::Pins::BATTERY_ENABLE, OUTPUT);
  digitalWrite(Config::Pins::BATTERY_ENABLE, HIGH); // Enable battery monitoring
  Wire.setPins(Config::Pins::I2C_SDA_PIN, Config::Pins::I2C_SCL_PIN);

  // Configure ADC
  analogReadResolution(12); // 12-bit resolution
  analogSetPinAttenuation(Config::Pins::BATTERY_ADC, ADC_11db);

  if (!sht4.begin())
  {
    Serial.println("Couldn't find SHT4x");
  }
  else
  {
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
  }

  // Initialize the Global SPI instance
  hspi.begin(Config::Pins::EPD_SCK, Config::Pins::SD_MISO, Config::Pins::EPD_MOSI, -1);

  // Initialize Display with raw pointer
  display = new GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>(
      GxEPD2_DRIVER_CLASS(Config::Pins::EPD_CS, Config::Pins::EPD_DC, Config::Pins::EPD_RES, Config::Pins::EPD_BUSY));

  // Pass the global hspi to the display
  display->epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display->init(0);

  pinMode(Config::Pins::LED, OUTPUT);
  digitalWrite(Config::Pins::LED, LOW);
}

/**
 * @brief Checks for Factory Reset button combination.
 * 
 * If Buttons 1 and 2 are held down during startup, triggers a factory reset
 * by wiping secrets and restarting the ESP. Beeps for feedback.
 */
void App::checkFactoryReset()
{
  const int chirpfrequency = 4000;
  const int shortbeep = 100;
  const int longbeep = 800;
  // three chances to change mind
  for (int i = 0; i < 3; i++)
  {
    if (digitalRead(Config::Pins::BUTTON_KEY1) == LOW && digitalRead(Config::Pins::BUTTON_KEY2) == LOW)
      tone(Config::Pins::BUZZER, chirpfrequency, shortbeep);
    else
      return;
    delay(1000);
  }
  if (digitalRead(Config::Pins::BUTTON_KEY1) == LOW && digitalRead(Config::Pins::BUTTON_KEY2) == LOW)
  {
    tone(Config::Pins::BUZZER, chirpfrequency, longbeep);
    Serial.println("Factory Reset Triggered!");
    dataManager.saveSecrets("", "", "", ""); 
    ESP.restart();
  }
}

/**
 * @brief Initializes storage and dependent managers.
 * 
 * Mounts SD card via DataManager and instantiates NetworkManager/PlotManager.
 */
void App::initStorage() {
    dataManager.begin(hspi);
    checkFactoryReset(); // Check reset usage after storage init
    
    // Initialize managers with raw pointers
    networkManager = new NetworkManager(&dataManager);
    plotManager = new PlotManager(display, &dataManager);

    dataManager.loadData(allPetData);
}

/**
 * @brief Main logic to update application data.
 * 
 * @param isViewUpdate If true, skips WiFi/API calls and just loads local data for a quick redraw.
 *                     If false, connects to WiFi, Syncs Time, and Fetches new API data.
 */
void App::updateData(bool isViewUpdate) {
  bool wifiSuccess = false;
  
  if (!isViewUpdate)
  {
    networkManager->connectOrProvision(display);

    if (networkManager->syncTime(rtc))
      wifiSuccess = true;

    if (networkManager->initApi())
    {
      networkManager->getApi()->setDebug(true);

      // Calculate how many days we are missing
      int daysToFetch = 30; // Default max

      time_t latestTimestamp = dataManager.getLatestTimestamp(allPetData);

      if (latestTimestamp > 0)
      {
        time_t now = time(NULL);
        long secondsDifference = now - latestTimestamp;
        Serial.printf("Latest timestamp from SD: %lu, %.2f days ago.\r\n", latestTimestamp, (float)secondsDifference / 86400.0);

        int daysDifference = (int)(secondsDifference / 86400) + 2; // +buffer
        daysToFetch = std::min(std::max(daysDifference, 1), 30);
      }
      Serial.printf("Requesting %d days of data from API.\r\n", daysToFetch);

      if (networkManager->getApi()->fetchAllData(daysToFetch))
      {
        allPets = networkManager->getApi()->getUnifiedPets();
        SL_Status status = networkManager->getApi()->getUnifiedStatus();
        if (!allPets.empty())
        {
          dataManager.savePets(allPets);
        }
        // Merge data
        for (const auto &pet : allPets)
        {
          auto records = networkManager->getApi()->getRecordsByPetId(pet.id, true);
          dataManager.mergeData(allPetData, pet.id.toInt(), records);
        }
        dataManager.saveData(allPetData);
        if (status.litter_level_percent > 0)
        {
          dataManager.saveStatus(status);
        }
      }
    }
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp);
    env_data point;
    point.temperature = temp.temperature;
    point.humidity = humidity.relative_humidity;
    point.timestamp = time(NULL);
    dataManager.addEnvData(point);
  }
  else
  {
    // If we are just updating the view (button1 or 2 press), try to load pets from NVS/SD
    networkManager->initializeFromRtc(rtc);
    allPets = dataManager.getPets();
  }
}

/**
 * @brief Renders the dashboard view to the E-Paper display.
 * 
 * @param rangeIndex The index of the date range to display.
 * @param status The current status of the litterbox.
 * @param wifiSuccess Whether the last WiFi connection was successful.
 */
void App::renderView(int rangeIndex, const SL_Status& status, bool wifiSuccess) {
    if (rangeIndex >= 0 && rangeIndex < Date_Range_Max) {
        plotManager->renderDashboard(allPets, allPetData, dateRangeInfo[rangeIndex], status, wifiSuccess);
        display->display();
        display->hibernate();
    }
}

/**
 * @brief Enters Deep Sleep to save power.
 * 
 * Calculates sleep duration based on battery voltage (2hr or 6hr) and valid wakeup pins.
 */
void App::enterSleep() {
  Serial.println("Sleeping...");
  
  // Load Runtime Config
  SystemConfig sysConfig = dataManager.getSystemConfig();

  // check battery low, extend sleep duration if so
  int mv = analogReadMilliVolts(Config::Pins::BATTERY_ADC);
  float battery_voltage = (mv / 1000.0) * 2;
  
  uint64_t sleepInterval; // needs to be in microseconds
  if (battery_voltage < sysConfig.battery_low_threshold_v)
  {
      Serial.printf("Battery Low (%.2fV < %.2fV). Sleeping for %d min.\n", battery_voltage, sysConfig.battery_low_threshold_v, sysConfig.sleep_interval_low_batt_min);
      sleepInterval = 1000000ull * 60ull * (uint64_t)sysConfig.sleep_interval_low_batt_min; 
  }
  else
  {
      Serial.printf("Battery OK (%.2fV). Sleeping for %d min.\n", battery_voltage, sysConfig.sleep_interval_min);
      sleepInterval = 1000000ull * 60ull * (uint64_t)sysConfig.sleep_interval_min; 
  }
    
  esp_sleep_enable_timer_wakeup(sleepInterval);
  // Wake up on Key 0, 1, or 2 (Low)
  esp_sleep_enable_ext1_wakeup(Config::BUTTON_KEY0_MASK | Config::BUTTON_KEY1_MASK | Config::BUTTON_KEY2_MASK, ESP_EXT1_WAKEUP_ANY_LOW);

  digitalWrite(Config::Pins::LED, HIGH);
  digitalWrite(Config::Pins::BATTERY_ENABLE, LOW);
  esp_deep_sleep_start();
}

/**
 * @brief Application Entry Point (Setup).
 * 
 * Handles Wakeup Cause logic (button presses requiring view update vs full wake),
 * runs the update/render pipeline, and goes back to sleep.
 */
void App::setup()
{
  initHardware();
  initStorage();

  int rangeIndex = dataManager.getPlotRange();
  rtc.begin();
  
  // Handle Wakeup Cause
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1)
  {
    uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_pins & Config::BUTTON_KEY1_MASK)
    {
      rangeIndex++;
      if (rangeIndex >= (int)Date_Range_Max)
        rangeIndex = 0;
    }
    else if (wakeup_pins & Config::BUTTON_KEY2_MASK)
    {
      rangeIndex--;
      if (rangeIndex < 0)
        rangeIndex = (int)Date_Range_Max - 1;
    }
    dataManager.savePlotRange(rangeIndex);
  }

  bool isViewUpdate = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);
  if (isViewUpdate)
  {
    uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_pins & Config::BUTTON_KEY0_MASK)
      isViewUpdate = false; // Key0 is the refresh button
  }
  
  // Update Logic
  updateData(isViewUpdate);

  // Get current status for rendering
  SL_Status status = dataManager.getStatus(); // Reload status in case it was updated
  bool wifiSuccess = false; // Simplified for now, logic inside updateData needs to bubble up or update state

  // Render Logic
  renderView(rangeIndex, status, wifiSuccess);
  
  // Sleep
  enterSleep();
}

void App::loop() {}
