idf_component_register(
    SRCS "WifiControl.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi
             nvs_flash)