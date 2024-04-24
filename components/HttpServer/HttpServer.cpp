#include "HttpServer.hpp"

HttpServer::HttpServer(std::shared_ptr<LedControl> led, std::string host_name) : _led(led), _host_name(host_name)
{
    // ESP_LOGI(_TAG, "led pin: %d", _led);
    // _status = ESP_ERR_INVALID_STATE;
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
        .handler = &LedControlHandler,
        .user_ctx = this,
    };

    // Register webocket handlers
    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = &WebsocketHandler,
        .user_ctx = this,
        .is_websocket = true
    };

    httpd_register_uri_handler(_server, &led_endpoint);
    httpd_register_uri_handler(_server, &ws);

    // httpd_register_uri_handler(_server, &ws);
    httpd_register_err_handler(_server, HTTPD_404_NOT_FOUND, &NotFoundHandler);
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
esp_err_t HttpServer::LedControlHandler(httpd_req_t* req)
{
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

    // check header
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
    // char* json_header_value = new char[json_header_value_length + 1];
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

    // if (strcmp(json_header_value.get(), "application/json") == 0)
    ESP_LOGI(_TAG, "Content-Type value: %s", json_header_value.get());
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
    const char* state = NULL;
    if (cJSON_GetObjectItem(root, "state"))
    {
        state = cJSON_GetObjectItem(root, "state")->valuestring;
    }
    free(root);

    if (!state)
    {
        cJSON* root;
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "status", 400);
        cJSON_AddStringToObject(root, "error", "Bad Request");
        cJSON_AddStringToObject(root, "message", "Must contain member \"state\"");
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

    auto* http_server = reinterpret_cast<HttpServer*>(req->user_ctx);
    // TODO: this should be a critical section. hence it can't be turned on and off at the same time
    if (strcmp(state, "on") == 0)
    {
        ESP_LOGI(_TAG, "Turn on the led");
        http_server->_led->TurnOn();
    }
    else if (strcmp(state, "off") == 0)
    {
        ESP_LOGI(_TAG, "Turn off the led");
        http_server->_led->TurnOff();
    }
    else
    {
        ESP_LOGI(_TAG, "The state doesn't contain the correct command: %s", state);
        cJSON* root;
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "status", 400);
        cJSON_AddStringToObject(root, "error", "Bad Request");
        cJSON_AddStringToObject(root, "message", "State doesn't contain the correct command");
        const char* root_string = cJSON_Print(root);

        esp_err_t set_type_status = httpd_resp_set_type(req, "application/json");
        ESP_LOGI(_TAG, "Set Type Status: %s", esp_err_to_name(set_type_status));
        esp_err_t set_status_result = httpd_resp_set_status(req, "400 Bad Request");
        ESP_LOGI(_TAG, "Set status result: %s", esp_err_to_name(set_status_result));

        esp_err_t send_response_status = httpd_resp_send(req, root_string, HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(_TAG, "Send response status: %s", esp_err_to_name(send_response_status));
        free(root);
        return ESP_OK;
    }

    cJSON* response;
    response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "Success");
    const char* respones_string = cJSON_Print(response);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, respones_string, HTTPD_RESP_USE_STRLEN);
    free(response);

    return ESP_OK;
}

esp_err_t HttpServer::WebsocketHandler(httpd_req_t* req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(_TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    // initialize the websocket packet
    httpd_ws_frame_t ws_packet;
    memset(&ws_packet, 0, sizeof(httpd_ws_frame_t));
    ws_packet.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t status = httpd_ws_recv_frame(req, &ws_packet, 0);
    if (status != ESP_OK)
    {
        ESP_LOGI(_TAG, "httpd_ws_recv_frame failed to get frame len with %s", esp_err_to_name(status));
        return status;
    }

    ESP_LOGI(_TAG, "frame length is %d", ws_packet.len);
    auto buffer = std::make_unique<uint8_t>(ws_packet.len + 1);
    ws_packet.payload = buffer.get();

    status = httpd_ws_recv_frame(req, &ws_packet, ws_packet.len);
    if (status != ESP_OK)
    {
        ESP_LOGE(_TAG, "Failed to get the paylod %s", esp_err_to_name(status));
        return status;
    }

    ESP_LOGI(_TAG, "Got packet with message: %s", (char*)ws_packet.payload);
    ESP_LOGI(_TAG, "Packet Type: %d", ws_packet.type);
    if (ws_packet.type == HTTPD_WS_TYPE_TEXT && strcmp((char*)ws_packet.payload, "Trigger async") == 0)
    {
        return trigger_async_send(req->handle, req);
    }

    status = httpd_ws_send_frame(req, &ws_packet);
    if (status != ESP_OK)
    {
        ESP_LOGE(_TAG, "httpd_ws_send_frame failed with %s", esp_err_to_name(status));
    }

    return status;
}

esp_err_t HttpServer::trigger_async_send(httpd_handle_t handle, httpd_req_t* req)
{
    auto resp_arg = std::make_unique<async_resp_arg>();
    resp_arg->handle = handle,
        resp_arg->file_descriptor = httpd_req_to_sockfd(req);

    if (httpd_queue_work(handle, wsAsyncSendStatic, resp_arg.get()) != ESP_OK)
    {
        return ESP_FAIL;
    }

    resp_arg.release();
    return ESP_OK;
}

void HttpServer::wsAsyncSendStatic(void* arg)
{
    auto* resp_arg = static_cast<async_resp_arg*>(arg);
    wsAsyncSend(resp_arg->handle, resp_arg->file_descriptor);
    delete resp_arg;
}

void HttpServer::wsAsyncSend(httpd_handle_t handle, int file_descriptor)
{
    const char* data = "Async data";
    httpd_ws_frame_t ws_packet = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)data,
        .len = strlen(data)
    };
    httpd_ws_send_frame_async(handle, file_descriptor, &ws_packet);
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