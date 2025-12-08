#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "core/SharedTypes.h"
#include "core/Config.h" 

class DataManager {
public:
    DataManager();
    
    // Initialize SD card using the shared SPI instance
    bool begin(SPIClass &spi);
    
    // Load historical data from SD into the provided map
    void loadData(PetDataMap &petData);
    
    // Save the provided map to SD, pruning old data
    void saveData(const PetDataMap &petData);
    
    //save latest status for display on plot
    void saveStatus(const SL_Status &status);

    //fetch sotred status info from SD card
    SL_Status getStatus();

    //push new temperature adn humidity datapoint 
    void addEnvData(env_data);

    std::vector<env_data> getEnvData();

    //save wifi and smart litterbox login to sd card
    void saveSecrets(String ssid, String wifi_pass, String SL_Account, String SL_pass);

    void savePlotRange(int range);
    int getPlotRange();

    String get_ssid(){return _ssid;}
    String get_wifi_pass(){return _wifi_pass;}
    String get_SL_Account(){return _SL_Account;}
    String get_SL_pass(){return _SL_pass;}
    String get_timezone(){return _tz;}
    String get_region(){return _region;}



    //store to SD card vector of pet names and IDs
    void savePets(std::vector<SL_Pet> pets);
    void saveTimezone(String tz, String region);
    //fetch stored pet vector from the SD card
    std::vector<SL_Pet> getPets();

    // Merge new records from API into the main map
    void mergeData(PetDataMap &mainData, int PetId, const std::vector<SL_Record> &newRecords);

    // Helper to find the most recent timestamp in the existing data
    time_t getLatestTimestamp(const PetDataMap &petData);

private:
    String _filename = "/pet_data.json";
    String _status_filename = "/status.json";
    String _pets_filename = "/pets.json";
    const char* _secrets_filename = "/secrets.json";
    const char* _tz_filename = "/timezone.json";
    const char* _config_filename = "/config.json";
    const char* _env_data_filename = "/env_data.json";

    String _ssid;
    String _wifi_pass;
    String _SL_Account;
    String _SL_pass;
    String _region;
    String _tz;
    bool loadSecrets();
    bool loadTimezone();
    void saveEnvData(std::vector<env_data>& env);
};

#endif