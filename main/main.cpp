extern "C"
{
    void app_main();
}

#include <stdio.h>
#include <string>

#include "LedControl.hpp"
#include "WifiControl.hpp"
#include "HttpServer.hpp"

// new
#include <esp_netif.h>
#include <esp_http_server.h>
#include <memory>
#include <cJSON.h>
#include <mdns.h>

void app_main(void)
{
    // esp_err_t nvs_flash_return =
    esp_err_t nvs_flash_return = nvs_flash_init();
    if (nvs_flash_return == ESP_ERR_NVS_NO_FREE_PAGES || nvs_flash_return == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_return = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_flash_return);

    WifiControl wifi_control("", "");
    WifiControl::Initialize();
    // wifi_control.ConnectInStationMode();
    wifi_control.ConnectInStationMode();

    ESP_LOGI("Main", "The GPIO_NUM_26: %d", GPIO_NUM_26);
    std::shared_ptr<LedControl> led = std::make_shared<LedControl>(GPIO_NUM_26);
    std::string host_name = "Baobao";
    httpd_handle_t server_handle = NULL;
    HttpServer server(server_handle, led, host_name);
    esp_err_t start = server.Start();

    while (server.GetServer())
    {
        sleep(1);
    }
}
