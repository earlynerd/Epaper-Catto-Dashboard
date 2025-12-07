#include "NetworkManager.h"
#include <ArduinoJson.h>
#include "certs.h"
#include "provisionerConfig.h"
#include "Fonts/FreeMono9pt7b.h"
#include "Fonts/FreeMonoBold24pt7b.h"

NetworkManager::NetworkManager(DataManager *dataManager)
{
    // Initialize time zone string to empty
    _dataManager = dataManager;
    memset(_time_zone, 0, sizeof(_time_zone));
}

void NetworkManager::connectOrProvision(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display)
{
    _display = display;
    WiFiProvisioner provisioner(provisionerCustom);

    // Define Provisioner Callbacks
    provisioner.onSuccess([this](const char *ssid, const char *password, const char *input, const char *pkuser, const char *pkpass)
                          {
                              Serial.printf("Connected to SSID: %s\n", ssid);
                              _dataManager->saveSecrets(ssid, password, pkuser, pkpass);
                              //_prefs.putString(NVS_SSID_KEY, ssid);
                              // if (password)
                              //    _prefs.putString(NVS_WIFI_PASS_KEY, password);
                              // if (pkuser)
                              //    _prefs.putString(NVS_PETKIT_USER_KEY, pkuser);
                              // if (pkpass)
                              //   _prefs.putString(NVS_PETKIT_PASS_KEY, pkpass);
                              Serial.println("Provisioning success! Restarting...");
                              //_prefs.end();
                              ESP.restart(); // Clean restart after provisioning
                          });

    // String ssid = _prefs.getString(NVS_SSID_KEY, "");
    String ssid = _dataManager->get_ssid();
    // String pass = _prefs.getString(NVS_WIFI_PASS_KEY, "");
    String pass = _dataManager->get_wifi_pass();

    if (ssid == "")
    {
        Serial.println("No saved WiFi. Starting provisioning.");
        printProvMessage();
        provisioner.startProvisioning();
        return;
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.print("Connecting to WiFi");

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("\nWiFi Timed Out. Starting provisioning.");
        if (display)
        {
            printProvMessage();
        }
        provisioner.startProvisioning();
    }
    Serial.println("\nWiFi Connected!");
}

void NetworkManager::printProvMessage()
{
    _display->fillScreen(GxEPD_WHITE);
    _display->setTextSize(1);
    _display->setTextColor(EPD_BLACK);
    _display->setFont(&FreeMonoBold24pt7b);
    _display->setCursor(20, 40);
    _display->print("WiFi Connect Fail.");
    _display->setTextSize(1);
    _display->setTextColor(EPD_BLACK);
    _display->setFont(&FreeMono9pt7b);
    _display->setCursor(10, 70);
    _display->print(" Connect to AP: \"CattoDashboard\" to configure.");
    _display->setCursor(10, 100);
    _display->print("If captive portal does not appear, navigate to \"192.168.4.1\" in browser.");
    _display->setCursor(10, 130);
    _display->print("Or, power off device, eject SD card, and edit \"secrets.json\" manually.");
    _display->display();
}

bool NetworkManager::syncTime(RTC_PCF8563 &rtc)
{
    // Try to load saved Timezone from NVS
    // String storedTZ = _prefs.getString(NVS_TZ_KEY, "");
    String storedTZ = _dataManager->get_timezone();
    if (storedTZ.length() > 0)
    {
        strncpy(_time_zone, storedTZ.c_str(), sizeof(_time_zone) - 1);
        setenv("TZ", storedTZ.c_str(), 1);
        tzset();
        Serial.printf("[Network] Loaded Timezone from SD: %s\n", _time_zone);
    }
    else
    {
        Serial.println("[Network] No Timezone on SD card. Will fetch from API.");
    }

    return getTimezoneAndSync(rtc);
}

bool NetworkManager::getTimezoneAndSync(RTC_PCF8563 &rtc)
{
    WiFiClientSecure client;
    client.setCACert(root_ca_worldtimeapi);
    HTTPClient http;

    // If we don't have a timezone yet, fetch it from WorldTimeAPI
    if (strlen(_time_zone) == 0)
    {
        bool tz_success = false;
        for (int i = 0; i < MAX_SYNC_RETRIES; ++i)
        {
            Serial.printf("[Time Sync] Fetching timezone attempt %d...\n", i + 1);

            if (http.begin(client, TIME_API_URL))
            {
                int httpCode = http.GET();
                if (httpCode == HTTP_CODE_OK)
                {
                    JsonDocument doc;
                    if (deserializeJson(doc, http.getStream()).code() == DeserializationError::Ok)
                    {
                        const char *tz_iana = doc["timezone"];
                        if (tz_iana)
                        {
                            // Save IANA (e.g. "America/New_York") for PetKit
                            //_prefs.putString(NVS_PETKIT_TIMEZONE_KEY, tz_iana);

                            // Convert IANA to POSIX (e.g. "EST5EDT,M3.2.0,M11.1.0") for ESP32 system time
                            const char *tz_posix = TzDbLookup::getPosix(tz_iana);

                            strncpy(_time_zone, tz_posix, sizeof(_time_zone) - 1);
                            //_prefs.putString(NVS_TZ_KEY, _time_zone);
                            _dataManager->saveTimezone(_time_zone, "us");

                            Serial.printf("[Time Sync] Discovered Timezone: %s (%s)\n", tz_iana, _time_zone);
                            tz_success = true;
                            http.end();
                            break;
                        }
                    }
                }
                http.end();
            }
            delay(1000);
        }

        if (!tz_success)
        {
            Serial.println("[Time Sync] Failed to determine timezone. Defaulting to UTC.");
            strncpy(_time_zone, "UTC0", sizeof(_time_zone) - 1);
        }
    }

    // Now perform the actual NTP Sync
    configTzTime(_time_zone, NTP_SERVER_1, NTP_SERVER_2);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 15000))
    { // 15s timeout
        time_t now_utc;
        time(&now_utc);
        rtc.adjust(DateTime(now_utc)); // Update Hardware RTC

        char time_buf[64];
        strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S %Z", &timeinfo);
        Serial.printf("[Network] Time synced: %s\n", time_buf);
        return true;
    }

    Serial.println("[Network] NTP Sync failed.");
    return false;
}

bool NetworkManager::initApi()
{
    // String user = _prefs.getString(NVS_PETKIT_USER_KEY, "");
    String user = _dataManager->get_SL_Account();
    // String pass = _prefs.getString(NVS_PETKIT_PASS_KEY, "");
    String pass = _dataManager->get_SL_pass();
    // String region = _prefs.getString(NVS_PETKIT_REGION_KEY, "us");
    String region = _dataManager->get_region();
    //_prefs.putString(NVS_PETKIT_REGION_KEY, region);        //TODO: find a way to look this up from timezone or something
    // String tz = _prefs.getString(NVS_PETKIT_TIMEZONE_KEY, "");
    String tz = _dataManager->get_region();

    if (user == "" || pass == "")
        return false;
    _petkit = new PetKitApi(user.c_str(), pass.c_str(), region.c_str(), tz.c_str());
    _litterbox = _petkit;
    if (_litterbox->login())
        return true;
    else
    {
        _petkit->~PetKitApi();
        _whisker = new WhiskerApi(user.c_str(), pass.c_str(), tz.c_str());
        _litterbox = _whisker;
    }
    if (_litterbox->login())
        return true;
    else
    {
        _whisker->~WhiskerApi();
        _litterbox = nullptr;
        return false;
    }

    //_petkit = new PetKitApi(user.c_str(), pass.c_str(), region.c_str(), tz.c_str(), LED_PIN);
}

bool NetworkManager::initializeFromRtc(RTC_PCF8563 &rtc)
{
    if (rtc.lostPower())
        return false;
    DateTime nowrtc = rtc.now();
    const timeval t = {.tv_sec = (time_t)nowrtc.unixtime(), .tv_usec = 0};
    settimeofday(&t, NULL);
    Serial.println("[Network] Time recalled from RTC");
    // String storedTZ = _prefs.getString(NVS_TZ_KEY, "");
    String storedTZ = _dataManager->get_timezone();
    if (storedTZ.length() > 0)
    {
        strncpy(_time_zone, storedTZ.c_str(), sizeof(_time_zone) - 1);
        setenv("TZ", storedTZ.c_str(), 1);
        tzset();
        Serial.printf("[Network] Loaded Timezone from SD: %s\n", _time_zone);
    }
    else
    {
        Serial.println("[Network] No Timezone on file.");
        return false;
    }

    return true;
}