#include "DataManager.h"

DataManager::DataManager() {}

bool DataManager::begin(SPIClass &spi) {
    pinMode(SD_EN_PIN, OUTPUT);
    digitalWrite(SD_EN_PIN, HIGH);
    pinMode(SD_DET_PIN, INPUT_PULLUP);
    delay(100);

    if (digitalRead(SD_DET_PIN)) {
        Serial.println("[DataManager] No SD card detected.");
        return false;
    }

    // We use the shared SPI instance passed from main
    if (!SD.begin(SD_CS_PIN, spi)) {
        Serial.println("[DataManager] SD Mount Failed!");
        return false;
    }
    
    Serial.println("[DataManager] SD Card Mounted.");
    return true;
}

void DataManager::loadData(PetDataMap &petData) {
    const char* tempFilename = "/pet_data.tmp";

    // crash recovery
    // Scenario: Power failed after deleting .json but before renaming .tmp
    if (!SD.exists(_filename) && SD.exists(tempFilename)) {
        Serial.println("[DataManager] Detected failed save. Recovering from temp file...");
        if (SD.rename(tempFilename, _filename)) {
            Serial.println("[DataManager] Recovery successful!");
        } else {
            Serial.println("[DataManager] Recovery rename failed. Attempting to load temp file directly.");
            // If rename fails, we can try to read the temp file directly below
            // by temporarily swapping the pointer, but usually rename works.
        }
    }

    // Check again (in case we just recovered it)
    if (!SD.exists(_filename)) {
        Serial.println("[DataManager] No data file found. Creating new.");
        return;
    }

    File file = SD.open(_filename, FILE_READ);
    if (!file) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("[DataManager] JSON Parse Error: ");
        Serial.println(error.c_str());
        // leave corrupted file, might be manually recoverable.
        return;
    }

    JsonObject root = doc.as<JsonObject>();
    for (JsonPair petPair : root) {
        int petId = atoi(petPair.key().c_str());
        JsonArray records = petPair.value().as<JsonArray>();

        for (JsonObject recordJson : records) {
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

void DataManager::saveData(const PetDataMap &petData) {
    // ATOMIC SAVE
    const char* tempFilename = "/pet_data.tmp";
    
    //Delete temp file if it exists (cleanup from previous crash)
    if (SD.exists(tempFilename)) {
        SD.remove(tempFilename);
    }

    //Write all present data to a .tmp file
    File file = SD.open(tempFilename, FILE_WRITE);
    if (!file) {
        Serial.println("[DataManager] Failed to open temp file for writing!");
        return;
    }

    JsonDocument doc; 
    JsonObject root = doc.to<JsonObject>();

    time_t now = time(NULL);
    time_t pruneTimestamp = now - (365 * 86400L); // Keep 365 days
    
    for (auto const &petPair : petData) {
        int petId = petPair.first;
        JsonArray petArray = root[String(petId)].to<JsonArray>();

        for (auto const &recordPair : petPair.second) {
            const SL_Record &record = recordPair.second;
            if (record.timestamp < pruneTimestamp) continue;

            JsonObject recJson = petArray.add<JsonObject>();
            recJson["ts"] = record.timestamp;
            recJson["w_lb"] = record.weight_lbs;
            recJson["dur_s"] = record.duration_seconds;
        }
    }

    if (serializeJson(doc, file) == 0) {
        Serial.println("[DataManager] Failed to write JSON content!");
        file.close();
        return;
    }
    
    //Ensure data is physically on the card before close
    file.flush(); 
    file.close();
    
    //Verify the Temp File
    File checkFile = SD.open(tempFilename);
    if (!checkFile || checkFile.size() == 0) {
         Serial.println("[DataManager] Temp file is invalid. Aborting save.");
         if(checkFile) checkFile.close();
         return;
    }
    checkFile.close();

    //If crash here (after remove, before rename), the 'Recovery Logic' in loadData() handles it.
    if (SD.exists(_filename)) {
        SD.remove(_filename);
    }
    
    if (SD.rename(tempFilename, _filename)) {
        Serial.println("[DataManager] Atomic Save Complete.");
    } else {
        Serial.println("[DataManager] Rename failed!");
        // Note: program leaves the .tmp file there so we can try to recover it next boot
    }
}

void DataManager::saveStatus(const SL_Status &status) {
    JsonDocument doc; 
    JsonObject root = doc.to<JsonObject>();
    root["is_drawer_full"] = status.is_drawer_full;
    root["device_name"] = status.device_name;
    root["device_type"] = status.device_type;
    root["litter_level_percent"] = status.litter_level_percent;
    root["waste_level_percent"] = status.waste_level_percent;
    root["is_error_state"] = status.is_error_state;
    root["status_text"] = status.status_text;
    root["timestamp"] = status.timestamp;

    File file = SD.open(_status_filename, FILE_WRITE);
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("[DataManager] Status saved to SD.");
    }
}

 SL_Status DataManager::getStatus()
 {
    SL_Status s;
    s.waste_level_percent = 0;
    s.litter_level_percent = 0;
    s.timestamp = 0;
    s.device_name = "";
    s.device_type = "";
    s.is_drawer_full = 0;
    s.is_error_state = false;

    if (!SD.exists(_status_filename)) {
        Serial.println("[DataManager] No Status file found.");
        return s;
    }
    else Serial.println("[DataManager] Status file loaded");

    File file = SD.open(_status_filename, FILE_READ);
    if (!file) return s;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        Serial.print("[DataManager] Status JSON Parse Error: ");
        Serial.println(error.c_str());
        return s;
    }
    
    JsonObject root = doc.as<JsonObject>();
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

void DataManager::mergeData(PetDataMap &mainData, int petId, const std::vector<SL_Record> &newRecords) {
    for (const auto &record : newRecords) {
        mainData[petId][record.timestamp] = record;
    }
}

time_t DataManager::getLatestTimestamp(const PetDataMap &petData) {
    time_t latest = 0;
    for (auto const &petPair : petData) {
        const std::map<time_t, SL_Record> &recordsMap = petPair.second;
        if (!recordsMap.empty()) {
            time_t petLatest = recordsMap.rbegin()->first;
            if (petLatest > latest) {
                latest = petLatest;
            }
        }
    }
    return latest;
}