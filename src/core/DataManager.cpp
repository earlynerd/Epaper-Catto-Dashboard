#include "core/DataManager.h"

DataManager::DataManager() {}

/**
 * @brief Initialize DataManager and Mount SD Card.
 *
 * Verifies SD card presence using detection pin, then mounts it.
 * Also ensures basic config files (secrets.json, timezone.json) exist.
 *
 * @param spi Reference to the global SPI bus.
 * @return true if SD card mounted and basic files loaded/created.
 */
bool DataManager::begin(SPIClass &spi)
{
    pinMode(Config::Pins::SD_EN, OUTPUT);
    digitalWrite(Config::Pins::SD_EN, HIGH);
    pinMode(Config::Pins::SD_DET, INPUT_PULLUP);
    delay(100);

    if (digitalRead(Config::Pins::SD_DET))
    {
        Serial.println("[DataManager] No SD card detected.");
        return false;
    }

    // We use the shared SPI instance passed from main
    if (!SD.begin(Config::Pins::SD_CS, spi))
    {
        Serial.println("[DataManager] SD Mount Failed!");
        return false;
    }

    Serial.println("[DataManager] SD Card Mounted.");

    if (!loadSecrets())
    {
        Serial.println("[DataManager] secrets.json not found, creating empty template.");
        saveSecrets("", "", "", ""); // create template json file that can be manually filled in
    }
    else
        Serial.println("[DataManager] secrets.json loaded!.");
    if (!loadTimezone())
    {
        Serial.println("[DataManager] timezone.json not found, creating empty template.");
        saveTimezone("", "");
    }
    else
        Serial.println("[DataManager] timezone.json loaded!.");
    return true;
}

/**
 * @brief Loads pet data from SD card into memory.
 *
 * Handles crash recovery by checking for .tmp files from failed previous saves.
 *
 * @param petData Map to populate with loaded data.
 */
void DataManager::loadData(PetDataMap &petData)
{
    const char *tempFilename = "/pet_data.tmp";

    // crash recovery
    // Scenario: Power failed after deleting .json but before renaming .tmp
    if (!SD.exists(_filename) && SD.exists(tempFilename))
    {
        Serial.println("[DataManager] Detected failed save. Recovering from temp file...");
        if (SD.rename(tempFilename, _filename))
        {
            Serial.println("[DataManager] Recovery successful!");
        }
        else
        {
            Serial.println("[DataManager] Recovery rename failed. Attempting to load temp file directly.");
            // If rename fails, we can try to read the temp file directly below
            // by temporarily swapping the pointer, but usually rename works.
        }
    }

    // Check again (in case we just recovered it)
    if (!SD.exists(_filename))
    {
        Serial.println("[DataManager] No data file found. Creating new.");
        return;
    }

    File file = SD.open(_filename, FILE_READ);
    if (!file)
        return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        Serial.print("[DataManager] JSON Parse Error: ");
        Serial.println(error.c_str());
        // leave corrupted file, might be manually recoverable.
        return;
    }

    JsonObject root = doc.as<JsonObject>();
    for (JsonPair petPair : root)
    {
        int petId = atoi(petPair.key().c_str());
        JsonArray records = petPair.value().as<JsonArray>();

        for (JsonObject recordJson : records)
        {
            SL_Record rec;
            rec.timestamp = recordJson["ts"];
            rec.weight_lbs = recordJson["w_lb"];
            rec.duration_seconds = recordJson["dur_s"];
            rec.PetId = petId;
            petData[petId][rec.timestamp] = rec;
        }
    }
    Serial.println("[DataManager] Historical data loaded.");
}

/**
 * @brief Saves pet data to SD card atomically.
 *
 * Writes data to a temporary file first, then renames it to the target filename
 * to prevent data corruption if power is lost during write.
 * Prunes data older than 365 days.
 *
 * @param petData The data to save.
 */
void DataManager::saveData(const PetDataMap &petData)
{
    // ATOMIC SAVE
    const char *tempFilename = "/pet_data.tmp";

    // Delete temp file if it exists (cleanup from previous crash)
    if (SD.exists(tempFilename))
    {
        SD.remove(tempFilename);
    }

    // Write all present data to a .tmp file
    File file = SD.open(tempFilename, FILE_WRITE);
    if (!file)
    {
        Serial.println("[DataManager] Failed to open temp file for writing!");
        return;
    }

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    time_t now = time(NULL);
    time_t pruneTimestamp = now - (365 * 86400L); // Keep 365 days

    for (auto const &petPair : petData)
    {
        int petId = petPair.first;
        JsonArray petArray = root[String(petId)].to<JsonArray>();

        for (auto const &recordPair : petPair.second)
        {
            const SL_Record &record = recordPair.second;
            if (record.timestamp < pruneTimestamp)
                continue;

            JsonObject recJson = petArray.add<JsonObject>();
            recJson["ts"] = record.timestamp;
            recJson["w_lb"] = record.weight_lbs;
            recJson["dur_s"] = record.duration_seconds;
        }
    }

    if (serializeJson(doc, file) == 0)
    {
        Serial.println("[DataManager] Failed to write JSON content!");
        file.close();
        return;
    }

    // Ensure data is physically on the card before close
    file.flush();
    file.close();

    // Verify the Temp File
    File checkFile = SD.open(tempFilename);
    if (!checkFile || checkFile.size() == 0)
    {
        Serial.println("[DataManager] Temp file is invalid. Aborting save.");
        if (checkFile)
            checkFile.close();
        return;
    }
    checkFile.close();

    // If crash here (after remove, before rename), the 'Recovery Logic' in loadData() handles it.
    if (SD.exists(_filename))
    {
        SD.remove(_filename);
    }

    if (SD.rename(tempFilename, _filename))
    {
        Serial.println("[DataManager] Atomic Save Complete.");
    }
    else
    {
        Serial.println("[DataManager] Rename failed!");
        // Note: program leaves the .tmp file there so we can try to recover it next boot
    }
}

void DataManager::saveStatus(const SL_Status &status)
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["api_type"] = (int)status.api_type;
    root["is_drawer_full"] = status.is_drawer_full;
    root["device_name"] = status.device_name;
    root["device_type"] = status.device_type;
    root["litter_level_percent"] = status.litter_level_percent;
    root["waste_level_percent"] = status.waste_level_percent;
    root["is_error_state"] = status.is_error_state;
    root["status_text"] = status.status_text;
    root["timestamp"] = status.timestamp;

    File file = SD.open(_status_filename, FILE_WRITE);
    if (file)
    {
        serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        Serial.println("[DataManager] Status saved to SD.");
    }
}

void DataManager::savePlotRange(int range)
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["plot_range_index"] = range;

    File file = SD.open(_config_filename, FILE_WRITE);
    if (file)
    {
        serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        Serial.println("[DataManager] Config.json saved to SD.");
    }
}

int DataManager::getPlotRange()
{
    if (!SD.exists(_config_filename))
    {
        Serial.println("[DataManager] No Plot Range File found. creating....");
        savePlotRange(0);
        return 0;
    }
    File file = SD.open(_config_filename, FILE_READ);
    if (!file)
        return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Serial.print("[DataManager] config JSON Parse Error: ");
        Serial.println(error.c_str());
        return false;
    }
    JsonObject root = doc.as<JsonObject>();
    int range = root["plot_range_index"].as<int>();
    return range;
}

void DataManager::savePets(std::vector<SL_Pet> pets)
{

    std::vector<SL_Pet> storedPets = getPets();
    if (!storedPets.empty())
    {
        bool allmatch = true;
        for (auto pet : pets)
        {
            bool match = false;
            for (auto stored : storedPets)
            {
                if (pet.id == stored.id)
                    match = true;
                ;
            }
            if (!match)
            {
                allmatch = false;
                continue;
            }
        }
        if (!allmatch)
        { // the collected pets and the set stored on SD card dont match
          // or, at least theres at least one new pet here
          // maybe we rename the old file instead of saving over it, for manual recovery
          // do the same for the historical data
            if (SD.rename(_pets_filename, _pets_filename + ".bak"))
            {
                Serial.println("[DataManager] Pets stored on SD do not match incoming, renamed existing pets and historical data for manual review/recovery.");
            }
            if (SD.exists(_filename))
            {
                SD.rename(_filename, _filename + ".bak");
            }
        }
    }
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    for (const auto &pet : pets)
    {
        JsonObject thispet = root[pet.id].to<JsonObject>();
        thispet["name"] = pet.name;
        thispet["weight_lbs"] = pet.weight_lbs;
    }

    File file = SD.open(_pets_filename, FILE_WRITE);
    if (file)
    {
        serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        Serial.println("[DataManager] Pets saved to SD.");
    }
    else
        Serial.println("[DataManager] Error saving Pets to SD.");
}

void DataManager::saveSecrets(String ssid, String wifi_pass, String SL_Account, String SL_pass)
{
    _ssid = ssid;
    _wifi_pass = wifi_pass;
    _SL_Account = SL_Account;
    _SL_pass = SL_pass;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["ssid"] = ssid;
    root["wifi_pass"] = wifi_pass;
    root["SL_Account"] = SL_Account;
    root["SL_pass"] = SL_pass;
    File file = SD.open(_secrets_filename, FILE_WRITE);
    if (file)
    {
        serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        Serial.println("[DataManager] Secrets saved to SD.");
    }
}

bool DataManager::loadSecrets()
{
    _ssid = "";
    _wifi_pass = "";
    _SL_Account = "";
    _SL_pass = "";
    File file = SD.open(_secrets_filename, FILE_READ);
    if (!file)
        return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Serial.print("[DataManager] pets JSON Parse Error: ");
        Serial.println(error.c_str());
        return false;
    }
    JsonObject root = doc.as<JsonObject>();
    _ssid = root["ssid"].as<String>();
    _wifi_pass = root["wifi_pass"].as<String>();
    _SL_Account = root["SL_Account"].as<String>();
    _SL_pass = root["SL_pass"].as<String>();
    if ((_ssid.length() > 0) && (_wifi_pass.length() > 0) && (_SL_Account.length() > 0) && (_SL_pass.length() > 0))
        return true;
    return false;
}

std::vector<SL_Pet> DataManager::getPets()
{
    std::vector<SL_Pet> pets;
    if (!SD.exists(_pets_filename))
    {
        Serial.println("[DataManager] No Pets file found.");
        return pets;
    }
    File file = SD.open(_pets_filename, FILE_READ);
    if (!file)
        return pets;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Serial.print("[DataManager] pets JSON Parse Error: ");
        Serial.println(error.c_str());
        return pets;
    }
    JsonObject root = doc.as<JsonObject>();
    for (JsonPair rec : root)
    {
        SL_Pet thispet;
        thispet.id = rec.key().c_str();
        JsonObject details = rec.value().as<JsonObject>();
        thispet.name = details["name"].as<String>();
        thispet.weight_lbs = details["weight_lbs"];
        pets.push_back(thispet);
    }
    Serial.println("[DataManager] Pets recalled from SD.");
    return pets;
}

SL_Status DataManager::getStatus()
{
    SL_Status s;
    s.api_type = ApiType::PETKIT;
    s.waste_level_percent = 0;
    s.litter_level_percent = 0;
    s.timestamp = 0;
    s.device_name = "";
    s.device_type = "";
    s.is_drawer_full = 0;
    s.is_error_state = false;

    if (!SD.exists(_status_filename))
    {
        Serial.println("[DataManager] No Status file found.");
        return s;
    }
    else
        Serial.println("[DataManager] Status file loaded");

    File file = SD.open(_status_filename, FILE_READ);
    if (!file)
        return s;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Serial.print("[DataManager] Status JSON Parse Error: ");
        Serial.println(error.c_str());
        return s;
    }

    JsonObject root = doc.as<JsonObject>();
    s.api_type = root["api_type"];
    s.device_name = root["device_name"].as<String>();
    s.device_type = root["device_name"].as<String>();
    s.is_drawer_full = root["is_drawer_full"];
    s.litter_level_percent = root["litter_level_percent"];
    s.waste_level_percent = root["waste_level_percent"];
    s.timestamp = root["timestamp"];
    s.status_text = root["status_text"].as<String>();
    s.is_error_state = root["is_error_state"];
    return s;
}

/**
 * @brief Merges new API records into the existing dataset.
 *
 * @param mainData Reference to the main data map.
 * @param petId The ID of the pet the records belong to.
 * @param newRecords Vector of new records from the API.
 */
void DataManager::mergeData(PetDataMap &mainData, int petId, const std::vector<SL_Record> &newRecords)
{
    for (const auto &record : newRecords)
    {
        mainData[petId][record.timestamp] = record;
    }
}

time_t DataManager::getLatestTimestamp(const PetDataMap &petData)
{
    time_t latest = 0;
    for (auto const &petPair : petData)
    {
        const std::map<time_t, SL_Record> &recordsMap = petPair.second;
        if (!recordsMap.empty())
        {
            time_t petLatest = recordsMap.rbegin()->first;
            if (petLatest > latest)
            {
                latest = petLatest;
            }
        }
    }
    return latest;
}

void DataManager::saveTimezone(String tz, String region)
{
    _tz = tz;
    _region = region;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["tz"] = tz;
    root["region"] = region;
    File file = SD.open(_tz_filename, FILE_WRITE);
    if (file)
    {
        serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        Serial.println("[DataManager] Timezone saved to SD.");
    }
}

bool DataManager::loadTimezone()
{
    _tz = "";
    _region = "";
    File file = SD.open(_tz_filename, FILE_READ);
    if (!file)
        return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Serial.print("[DataManager] timezone JSON Parse Error: ");
        Serial.println(error.c_str());
        return false;
    }
    JsonObject root = doc.as<JsonObject>();
    _tz = root["tz"].as<String>();
    _region = root["region"].as<String>();
    if ((_tz.length() > 0) && (_region.length() > 0))
        return true;
    return false;
}

void DataManager::addEnvData(env_data newvalue)
{
    Serial.printf("{DataManager] Temperature: %.2f°C, Humidity %.2f%\r\n", newvalue.temperature, newvalue.humidity);
    std::vector<env_data> env = getEnvData();
    env.push_back(newvalue);
    saveEnvData(env);
}

void DataManager::saveEnvData(std::vector<env_data> &env)
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    JsonArray data = root["data"].to<JsonArray>();
    for (env_data dat : env)
    {
        JsonObject recJson = data.add<JsonObject>();
        recJson["temperature"] = dat.temperature;
        recJson["humidity"] = dat.humidity;
        recJson["timestamp"] = dat.timestamp;
    }
    File file = SD.open(_env_data_filename, FILE_WRITE);
    if (file)
    {
        serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        Serial.println("[DataManager] ENV data saved to SD.");
    }
}

std::vector<env_data> DataManager::getEnvData()
{
    std::vector<env_data> env;
    if (!SD.exists(_env_data_filename))
    {
        Serial.println("[DataManager] No environmental data found.");
        return env;
    }
    File file = SD.open(_env_data_filename, FILE_READ);
    if (!file)
        return env;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Serial.print("[DataManager] env JSON Parse Error: ");
        Serial.println(error.c_str());
        return env;
    }
    JsonObject root = doc.as<JsonObject>();
    JsonArray records = root["data"].as<JsonArray>();
    for (JsonObject rec : records)
    {
        env_data dat;
        dat.humidity = rec["humidity"];
        dat.temperature = rec["temperature"];
        dat.timestamp = rec["timestamp"];
        env.push_back(dat);
    }
    Serial.println("[DataManager] environmental data loaded from SD.");
    return env;
}

void DataManager::saveSystemConfig(const SystemConfig &config)
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["sleep_interval_min"] = config.sleep_interval_min;
    root["sleep_interval_low_batt_min"] = config.sleep_interval_low_batt_min;
    root["battery_low_threshold_v"] = config.battery_low_threshold_v;

    File file = SD.open(_system_config_filename, FILE_WRITE);
    if (file)
    {
        serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        Serial.println("[DataManager] System Config saved to SD.");
    }
}

SystemConfig DataManager::getSystemConfig()
{
    SystemConfig config; // Defaults instantiated

    if (!SD.exists(_system_config_filename))
    {
        Serial.println("[DataManager] No System Config found. Creating default.");
        saveSystemConfig(config);
        return config;
    }

    File file = SD.open(_system_config_filename, FILE_READ);
    if (!file)
        return config;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        Serial.print("[DataManager] System Config JSON Parse Error: ");
        Serial.println(error.c_str());
        return config;
    }

    JsonObject root = doc.as<JsonObject>();
    if (root["sleep_interval_min"])
        config.sleep_interval_min = root["sleep_interval_min"];
    if (root["sleep_interval_low_batt_min"])
        config.sleep_interval_low_batt_min = root["sleep_interval_low_batt_min"];
    if (root["battery_low_threshold_v"])
        config.battery_low_threshold_v = root["battery_low_threshold_v"];

    return config;
}

void DataManager::saveLayout(const std::vector<WidgetConfig> &layout)
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    JsonArray widgets = root["widgets"].to<JsonArray>();

    for (const auto &w : layout)
    {
        JsonObject obj = widgets.add<JsonObject>();
        obj["type"] = w.type;
        obj["x"] = w.x;
        obj["y"] = w.y;
        obj["w"] = w.w;
        obj["h"] = w.h;
        if (w.title.length() > 0)
            obj["title"] = w.title;
        if (w.dataSource.length() > 0)
            obj["dataSource"] = w.dataSource;
        obj["min"] = w.min;
        obj["max"] = w.max;
    }

    File file = SD.open(_layout_filename, FILE_WRITE);
    if (file)
    {
        serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        Serial.println("[DataManager] Layout saved to SD.");
    }
}

std::vector<WidgetConfig> DataManager::loadLayout()
{
    std::vector<WidgetConfig> layout;

    if (!SD.exists(_layout_filename))
    {
        if (getStatus().api_type == PETKIT)
        {
            Serial.println("[DataManager] No Layout file. Creating Petkit default.");
            // Create Default Layout (Based on previous hardcoded values for 800x480)
            // 1. Scatter Plot
            layout.push_back(WidgetConfig{"ScatterPlot", 0, 10, 800, 350, "Weight (lb) - %s", "scatter", 0, 0});

            // 2. Histograms
            layout.push_back(WidgetConfig{"Histogram", 0, 360, 300, 120, "Interval (Hours)", "interval", 0, 0});
            layout.push_back(WidgetConfig{"Histogram", 300, 360, 300, 120, "Duration (Minutes)", "duration", 0, 0});

            // 3. Status Widgets
            layout.push_back(WidgetConfig{"LinearGauge", 725, 2, 59, 22, "", "battery", 0, 100});
            layout.push_back(WidgetConfig{"TextLabel", 29, 8, 200, 20, "%b %d, %I:%M %p", "datetime", 0, 0});

            // Litter Gauge (Approximate placement for PetKit style)
            layout.push_back(WidgetConfig{"LinearGauge", 610, 380, 175, 38, "Litter:", "litter", 0, 100});
            layout.push_back(WidgetConfig{"StatusBox", 610, 427, 175, 38, "", "petkit_status", 0, 0});
        }
        else // whisker
        {
            Serial.println("[DataManager] No Layout file. Creating Whisker default.");
            // 1. Scatter Plot
            layout.push_back(WidgetConfig{"ScatterPlot", 0, 10, 800, 350, "Weight (lb) - %s", "scatter", 0, 0});

            // 2. Histograms
            layout.push_back(WidgetConfig{"Histogram", 0, 360, 600, 120, "Interval (Hours)", "interval", 0, 0});

            // 3. Status Widgets
            layout.push_back(WidgetConfig{"LinearGauge", 725, 2, 59, 22, "", "battery", 0, 100});
            layout.push_back(WidgetConfig{"TextLabel", 29, 8, 200, 20, "%b %d, %I:%M %p", "datetime", 0, 0});

            // Litter Gauge (Approximate placement for PetKit style)
            layout.push_back(WidgetConfig{"LinearGauge", 605, 380, 180, 35, "Litter:", "litter", 0, 100});
            layout.push_back(WidgetConfig{"LinearGauge", 605, 430, 180, 35, "Waste:", "waste", 0, 100});

        }
        saveLayout(layout);
        return layout;
    }

    File file = SD.open(_layout_filename, FILE_READ);
    if (!file)
        return layout;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        Serial.print("[DataManager] Layout JSON Error: ");
        Serial.println(error.c_str());
        return layout;
    }

    JsonArray widgets = doc["widgets"].as<JsonArray>();
    for (JsonObject obj : widgets)
    {
        WidgetConfig w;
        w.type = obj["type"].as<String>();
        w.x = obj["x"];
        w.y = obj["y"];
        w.w = obj["w"];
        w.h = obj["h"];
        if (obj["title"])
            w.title = obj["title"].as<String>();
        if (obj["dataSource"])
            w.dataSource = obj["dataSource"].as<String>();
        if (obj["min"])
            w.min = obj["min"];
        if (obj["max"])
            w.max = obj["max"];
        layout.push_back(w);
    }

    return layout;
}
