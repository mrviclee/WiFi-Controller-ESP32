#include "HttpServer.hpp"

HttpServer::HttpServer(httpd_handle_t server, std::shared_ptr<LedControl> led, std::string host_name)
    : _server(server), _led(led), _host_name(host_name)
{
}

HttpServer::~HttpServer()
{
}

esp_err_t HttpServer::Start()
{
    // if (_server || _status == ESP_OK)
    if (_server)
    {
        ESP_LOGI(_TAG, "Server already started");
        return ESP_OK;
    }

    // Setup local DNS
    if (_host_name != "")
    {
        mdns_init();
        mdns_hostname_set(_host_name.c_str());
        mdns_instance_name_set("ESP32 Web Server");

        ESP_LOGI(_TAG, "Completed MDNS setup");
    }

    // Setup http server config
    _server = NULL;
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.lru_purge_enable = true;

    ESP_LOGI(_TAG, "Start server");
    esp_err_t status = httpd_start(&_server, &server_config);

    // Register url handlers
    httpd_uri_t led_endpoint = {
        .uri = "/led",
        .method = HTTP_POST,
        .handler = &LedControlHttpHandlerStatic,
        .user_ctx = this,
        .is_websocket = false,
        .handle_ws_control_frames = false
    };

    // Register webocket handlers
    httpd_uri_t ws = {
        .uri = "/wsled",
        .method = HTTP_GET,
        .handler = &LedControlWebSocketHandlerStatic,
        .user_ctx = this,
        .is_websocket = true
    };

    httpd_register_uri_handler(_server, &led_endpoint);
    httpd_register_uri_handler(_server, &ws);

    // httpd_register_uri_handler(_server, &ws);
    httpd_register_err_handler(_server, HTTPD_404_NOT_FOUND, &NotFoundHandlerStatic);
    ESP_LOGI(_TAG, "Registered Led Handler");

    return status;
}

esp_err_t HttpServer::Stop()
{
    ESP_LOGI(_TAG, "Stop server");

    esp_err_t stop_status = ESP_OK;
    if (_server)
    {
        // _status = ESP_ERR_INVALID_STATE;
        stop_status = httpd_stop(_server);
    }

    return stop_status;
}

httpd_handle_t HttpServer::GetServer()
{
    return _server;
}

// esp_err_t HttpServer::GetServerStatus()
// {
//     return _status;
// }

const char* HttpServer::_TAG = "HttpServer";
esp_err_t HttpServer::LedControlHttpHandler(httpd_req_t* req)
{
    // Null check for the request
    if (!req)
    {
        ESP_LOGI(_TAG, "The request is null");

        cJSON* root;
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "status", 400);
        cJSON_AddStringToObject(root, "error", "Bad Request");
        cJSON_AddStringToObject(root, "message", "Invalid Request");
        const char* root_string = cJSON_Print(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, root_string, HTTPD_RESP_USE_STRLEN);

        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    // check header to ensure it includes the content-type
    size_t json_header_value_length = httpd_req_get_hdr_value_len(req, "Content-Type");
    ESP_LOGI(_TAG, "Content-Type header length: %d", json_header_value_length);
    if (json_header_value_length == 0)
    {
        cJSON* root;
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "status", 400);
        cJSON_AddStringToObject(root, "error", "Bad Request");
        cJSON_AddStringToObject(root, "message", "Must have header Content-Type");
        const char* root_string = cJSON_Print(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, root_string, HTTPD_RESP_USE_STRLEN);

        cJSON_Delete(root);
        return ESP_OK;
    }

    std::unique_ptr<char[]> json_header_value(new char[json_header_value_length + 1]);
    esp_err_t get_json_header_status = httpd_req_get_hdr_value_str(req, "Content-Type", json_header_value.get(), json_header_value_length + 1);
    if (get_json_header_status != ESP_OK)
    {
        ESP_LOGI(_TAG, "Cannot get the header Content-Type: Error Code: %s", esp_err_to_name(get_json_header_status));
        cJSON* root;
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "status", 400);
        cJSON_AddStringToObject(root, "error", "Bad Request");
        cJSON_AddStringToObject(root, "message", "Must have header Content-Type");
        const char* root_string = cJSON_Print(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, root_string, HTTPD_RESP_USE_STRLEN);

        cJSON_Delete(root);
        return ESP_OK;
    }

    ESP_LOGI(_TAG, "Content-Type value: %s", json_header_value.get());

    // Ensure the content type is json
    if (strcmp(json_header_value.get(), "application/json") != 0)
    {
        ESP_LOGI(_TAG, "The Content-Type is not application/json");
        cJSON* root;
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "status", 400);
        cJSON_AddStringToObject(root, "error", "Bad Request");
        cJSON_AddStringToObject(root, "message", "Type must be application json");
        const char* root_string = cJSON_Print(root);

        esp_err_t set_type_status = httpd_resp_set_type(req, "application/json");
        ESP_LOGI(_TAG, "Set Type Status: %s", esp_err_to_name(set_type_status));
        esp_err_t set_status_result = httpd_resp_set_status(req, "400 Bad Request");
        ESP_LOGI(_TAG, "Set status result: %s", esp_err_to_name(set_status_result));

        esp_err_t send_response_status = httpd_resp_send(req, root_string, HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(_TAG, "Send response status: %s", esp_err_to_name(send_response_status));

        cJSON_Delete(root);
        return ESP_OK;
    }

    // check buffer length
    size_t content_buffer_length = req->content_len + 1;
    std::unique_ptr<char[]> content_buffer(new char[content_buffer_length]);
    ESP_LOGI(_TAG, "The content length: %d", content_buffer_length);

    int http_read_content_status = httpd_req_recv(req, content_buffer.get(), content_buffer_length);
    if (http_read_content_status <= 0)
    {
        ESP_LOGI(_TAG, "Reading the request content is not successul");
        unsigned int error_status = 500;
        std::string error_code = "Internal Server Error";
        std::string error_message = "Internal Server Error";

        if (http_read_content_status == 0)
        {
            error_status = 400;
            error_code = "Bad Request";
            error_message = "Buffer length parameter is 0 or connection closed by peer";
        }
        //TODO: should timeout be internal server error?

        cJSON* root;
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "status", error_status);
        cJSON_AddStringToObject(root, "error", error_code.c_str());
        cJSON_AddStringToObject(root, "message", error_message.c_str());
        const char* root_string = cJSON_Print(root);

        esp_err_t set_type_status = httpd_resp_set_type(req, "application/json");
        ESP_LOGI(_TAG, "Set Type Status: %s", esp_err_to_name(set_type_status));
        esp_err_t set_status_result = httpd_resp_set_status(req, "500 Internal Server Error");
        ESP_LOGI(_TAG, "Set status result: %s", esp_err_to_name(set_status_result));

        esp_err_t send_response_status = httpd_resp_send(req, root_string, HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(_TAG, "Send response status: %s", esp_err_to_name(send_response_status));

        cJSON_Delete(root);
        return ESP_OK;
    }

    // parse the buffer json object
    ESP_LOGI(_TAG, "Start deserializing the object");
    cJSON* root = cJSON_Parse(content_buffer.get());
    std::string parsed_result = "";
    bool is_parse_successful = ParseStateRequestJson(content_buffer.get(), parsed_result);

    if (!is_parse_successful)
    {
        std::string failed_error_string = ConstructFailedJsonResponse(400, "Bad Request", parsed_result);

        esp_err_t set_type_status = httpd_resp_set_type(req, "application/json");
        ESP_LOGI(_TAG, "Set Type Status: %s", esp_err_to_name(set_type_status));
        esp_err_t set_status_result = httpd_resp_set_status(req, "400 Bad Request");
        ESP_LOGI(_TAG, "Set status result: %s", esp_err_to_name(set_status_result));

        esp_err_t send_response_status = httpd_resp_send(req, failed_error_string.c_str(), HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(_TAG, "Send response status: %s", esp_err_to_name(send_response_status));

        return send_response_status;
    }

    // TODO: this should be a critical section. hence it can't be turned on and off at the same time
    if (parsed_result.compare("on") == 0)
    {
        ESP_LOGI(_TAG, "Turn on the led");
        _led->TurnOn();
    }
    
    if (parsed_result.compare("off") == 0)
    {
        ESP_LOGI(_TAG, "Turn off the led");
        _led->TurnOff();
    }

    std::string response_string = ConstructSuccessResponse();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, response_string.c_str(), HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t HttpServer::LedControlWebsocketHandler(httpd_req_t* req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(_TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    // initialize the websocket packet
    httpd_ws_frame_t received_ws_packet;
    memset(&received_ws_packet, 0, sizeof(httpd_ws_frame_t));
    received_ws_packet.type = HTTPD_WS_TYPE_TEXT;

    // get the length of the webpacket frame
    esp_err_t status = httpd_ws_recv_frame(req, &received_ws_packet, 0);
    if (status != ESP_OK)
    {
        ESP_LOGI(_TAG, "httpd_ws_recv_frame failed to get frame len with %s", esp_err_to_name(status));
        return status;
    }

    ESP_LOGI(_TAG, "frame length is %d", received_ws_packet.len);
    if (received_ws_packet.len == 0)
    {
        ESP_LOGI(_TAG, "The frame length is 0. Preparing to send error response");
        std::string failed_response_string = ConstructFailedJsonResponse(400, "Bad Request", "The message cannot be empty");
        ESP_LOGI(_TAG, "The error is: %s", failed_response_string.c_str());

        httpd_ws_frame_t error_response_ws_packet;
        memset(&error_response_ws_packet, 0, sizeof(httpd_ws_frame_t));

        error_response_ws_packet = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t*)failed_response_string.c_str(),
            .len = failed_response_string.length()
        };

        status = httpd_ws_send_frame(req, &error_response_ws_packet);
        if (status != ESP_OK)
        {
            ESP_LOGE(_TAG, "Failed to send the error response %s", esp_err_to_name(status));
        }

        return status;
    }

    // initialize the buffer with the packet size + 1 becuase of the null byte \0
    auto buffer = std::make_unique<uint8_t[]>(received_ws_packet.len + 1);
    received_ws_packet.payload = buffer.get();

    // get the payload data
    status = httpd_ws_recv_frame(req, &received_ws_packet, received_ws_packet.len);
    if (status != ESP_OK)
    {
        ESP_LOGE(_TAG, "Failed to get the paylod %s", esp_err_to_name(status));
        return status;
    }

    ESP_LOGI(_TAG, "Got packet with message: %s", (char*)received_ws_packet.payload);
    ESP_LOGI(_TAG, "Packet Type: %d", received_ws_packet.type);

    if (received_ws_packet.type != HTTPD_WS_TYPE_TEXT)
    {
        ESP_LOGI(_TAG, "The websocket packet type is not HTTPD_WS_TYPE_TEXT");
        std::string failed_response_string = ConstructFailedJsonResponse(400, "Bad Request", "The type must be HTTPD_WS_TYPE_TEXT");
        ESP_LOGI(_TAG, "The error is: %s", failed_response_string.c_str());

        httpd_ws_frame_t error_response_ws_packet;
        memset(&error_response_ws_packet, 0, sizeof(httpd_ws_frame_t));

        error_response_ws_packet = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t*)failed_response_string.c_str(),
            .len = failed_response_string.length()
        };

        status = httpd_ws_send_frame(req, &error_response_ws_packet);
        if (status != ESP_OK)
        {
            ESP_LOGE(_TAG, "Failed to send the error response %s", esp_err_to_name(status));
        }

        return status;
    }

    // Start parsing the message
    std::string parsed_result = "";
    bool is_json_parse_sucessful = ParseStateRequestJson((char*)buffer.get(), parsed_result);

    ESP_LOGI(_TAG, "Parsed the json request. Status: %d, Parsed_result: %s", is_json_parse_sucessful, parsed_result.c_str());

    if (!is_json_parse_sucessful)
    {
        std::string failed_response_string = ConstructFailedJsonResponse(400, "Bad Request", parsed_result);

        httpd_ws_frame_t error_response_ws_packet;
        memset(&error_response_ws_packet, 0, sizeof(httpd_ws_frame_t));

        error_response_ws_packet = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t*)failed_response_string.c_str(),
            .len = failed_response_string.length()
        };

        status = httpd_ws_send_frame(req, &error_response_ws_packet);
        if (status != ESP_OK)
        {
            ESP_LOGE(_TAG, "Failed to send the error response %s", esp_err_to_name(status));
        }

        return status;
    }

    // // TODO: this should be a critical section. hence it can't be turned on and off at the same time
    if (parsed_result.compare("on") == 0)
    {
        ESP_LOGI(_TAG, "Turn on the led");
        _led->TurnOn();
    }

    if (parsed_result.compare("off") == 0)
    {
        ESP_LOGI(_TAG, "Turn off the led");
        _led->TurnOff();
    }

    std::string success_response_string = ConstructSuccessResponse();
    httpd_ws_frame_t success_response_ws_packet;
    memset(&success_response_ws_packet, 0, sizeof(httpd_ws_frame_t));

    success_response_ws_packet = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)success_response_string.c_str(),
        .len = success_response_string.length()
    };

    status = httpd_ws_send_frame(req, &success_response_ws_packet);

    ESP_LOGI(_TAG, "The status is %s", esp_err_to_name(status));
    return status;
}

esp_err_t HttpServer::NotFoundHandler(httpd_req_t* req, httpd_err_code_t error)
{
    // Set up the JSON object
    // {
    //      "status": 404,
    //      "error": "Not Found",
    //      "message": "The requested resource was not found on this server."
    // }
    cJSON* root;
    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "status", 404);
    cJSON_AddStringToObject(root, "error", "Not Found");
    cJSON_AddStringToObject(root, "message", "The requested resource was not found on this server.");
    std::string root_string = cJSON_Print(root);

    // Set the type to application json and header to 404
    // TODO: define the "application/json" and "404 Not Found" as Macros
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, root_string.c_str(), HTTPD_RESP_USE_STRLEN);

    cJSON_Delete(root);
    return ESP_FAIL;
}

/* Static Handler Wrapper */
esp_err_t HttpServer::LedControlHttpHandlerStatic(httpd_req_t* req)
{
    auto* http_server = reinterpret_cast<HttpServer*>(req->user_ctx);
    return http_server->LedControlHttpHandler(req);
}

esp_err_t HttpServer::NotFoundHandlerStatic(httpd_req_t* req, httpd_err_code_t error)
{
    auto* http_server = reinterpret_cast<HttpServer*>(req->user_ctx);
    return http_server->NotFoundHandler(req, error);
}

esp_err_t HttpServer::LedControlWebSocketHandlerStatic(httpd_req_t* req)
{
    auto* http_server = reinterpret_cast<HttpServer*>(req->user_ctx);
    return http_server->LedControlWebsocketHandler(req);
}

/* Helper Methods Implementation */
std::string HttpServer::ConstructFailedJsonResponse(const uint16_t& error_status, const std::string& error_code, const std::string& error_message)
{
    cJSON* root;
    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "status", error_status);
    cJSON_AddStringToObject(root, "error", error_code.c_str());
    cJSON_AddStringToObject(root, "message", error_message.c_str());
    std::string root_string = cJSON_Print(root);

    cJSON_Delete(root);
    return root_string;
}

std::string HttpServer::ConstructSuccessResponse()
{
    cJSON* response;
    response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "Success");
    std::string respones_string = cJSON_Print(response);

    cJSON_Delete(response);
    return respones_string;
}

bool HttpServer::ParseStateRequestJson(const char* request, std::string& output_parsed_state)
{
    cJSON* recevied_payload_json = cJSON_Parse(request);
    ESP_LOGI(_TAG, "Parsed the json sucessuflly");

    // Failed to parse
    if (!recevied_payload_json)
    {
        ESP_LOGW(_TAG, "Malformed Json");
        output_parsed_state = "The request message must be a valid json";
        return false;
    }

    cJSON* state_item = cJSON_GetObjectItem(recevied_payload_json, "state");

    if (!state_item || state_item->type != cJSON_String)
    {
        ESP_LOGW(_TAG, "The state item cannot be parsed");
        output_parsed_state = "Must contain member \"state\"";
        return false;
    }
    ESP_LOGI(_TAG, "Got the state item");

    const char* state = state_item->valuestring;
    if (strcmp(state, "on") != 0 && strcmp(state, "off") != 0)
    {
        output_parsed_state = "State doesn't contain the correct command";
        return false;
    }

    output_parsed_state = state;
    return true;
}