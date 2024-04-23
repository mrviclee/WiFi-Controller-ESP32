#include "WifiControl.hpp"

WifiControl::WifiControl(std::string ssid, std::string password)
{
    _ssid = ssid;
    _password = password;
}

WifiControl::~WifiControl()
{
}

void WifiControl::Initialize()
{
    _wifi_event_group = xEventGroupCreate();
    _retry_count = 0;
}

void WifiControl::WifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            if (_retry_count < _max_number_of_retries)
            {
                esp_wifi_connect();
                _retry_count++;
            }
            else
            {
                xEventGroupSetBits(_wifi_event_group, WIFI_FAILED_BIT);
            }
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            // ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            _retry_count = 0;
            xEventGroupSetBits(_wifi_event_group, WIFI_CONNECTED_BIT);
            // printf("WIFI Connected IP: %d \n", IP2STR(&event->ip_info.ip));
            std::printf("WIFI Connected\n");
        }
    }
}

void WifiControl::ConnectInStationMode()
{
    // Initialize Phase
    // init lwip
    ESP_ERROR_CHECK(esp_netif_init());

    // init event
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // init wifi
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_driver_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_driver_config));

    // create app task
    esp_event_handler_instance_t instance_wifi_any_id;
    esp_event_handler_instance_t instance_ip_got_id;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiControl::WifiEventHandler, NULL, &instance_wifi_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiControl::WifiEventHandler, NULL, &instance_ip_got_id));

    // Configure Phase
    // configure wifi
    // TODO: Configure the other memebers such as the security threshold
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = ""}};
    strncpy((char *)wifi_configuration.sta.ssid, _ssid.c_str(), 32);
    strncpy((char *)wifi_configuration.sta.password, _password.c_str(), 64);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_configuration));

    // Start Phase
    ESP_ERROR_CHECK(esp_wifi_start());

    // start wifi
    // wifi event sta start
    EventBits_t wifi_event_bits = xEventGroupWaitBits(_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (wifi_event_bits & WIFI_CONNECTED_BIT)
    {
        printf("Connected to local network");
    }
    else if (wifi_event_bits & WIFI_FAILED_BIT)
    {
        printf("Failed to connec to the wifi");
    }
    else
    {
        printf("To be handled event");
    }
}