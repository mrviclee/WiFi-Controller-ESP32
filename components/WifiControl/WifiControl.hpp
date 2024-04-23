#ifndef WIFICONTROL_HPP
#define WIFICONTROL_HPP

#include <stdio.h>
#include <string>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <iostream>
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

class WifiControl
{
private:
    // wifi name
    std::string _ssid;
    std::string _password;

    // An event group that represents the wifi connection status
    // Bit 0 represents connected
    // Bit 1 represents failed
    static inline EventGroupHandle_t _wifi_event_group = xEventGroupCreate();
#ifndef WIFI_CONNECTED_BIT
#define WIFI_CONNECTED_BIT BIT0
#endif

#ifndef WIFI_FAILED_BIT
#define WIFI_FAILED_BIT BIT1
#endif

    // Configuration for retries
    static const uint8_t _max_number_of_retries = 10;
    static inline uint8_t _retry_count = 0;
    
    static void WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    // void InitializationPhase();
    // void ConfigurePhase();
    // void StartPhase();
    // void ConnectPhase();

public:
    WifiControl(std::string ssid, std::string password);
    ~WifiControl();

    //TODO: get the wifi connection status or make this function return a value indicating the connection status
    void ConnectInStationMode();
    void DisConnect();
    static void Initialize();

};
#endif