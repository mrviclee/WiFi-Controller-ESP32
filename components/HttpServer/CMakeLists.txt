idf_component_register(
    SRCS "HttpServer.cpp"
    INCLUDE_DIRS "."
    REQUIRES 
            LedControl
            esp_https_server
            esp_http_server
            json
            mdns)