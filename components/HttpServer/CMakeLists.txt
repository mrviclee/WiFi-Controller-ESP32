idf_component_register(
    SRCS "HttpServer.cpp"
    INCLUDE_DIRS "."
    REQUIRES 
            LedControl
            esp_https_server
            esp_http_server
            json
            mdns
            EMBED_TXTFILES "${CMAKE_CURRENT_SOURCE_DIR}/WebPage/index.html" "${CMAKE_CURRENT_SOURCE_DIR}/WebPage/websocket.js")