#include "HttpServer.hpp"

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

extern const uint8_t websocket_js_start[] asm("_binary_websocket_js_start");
extern const uint8_t websocket_js_end[]   asm("_binary_websocket_js_end");


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
    server_config.open_fn = OnOpenConnectionStatic;
    server_config.close_fn = OnCloseConnectionStatic;
    server_config.global_user_ctx = this;
    server_config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(_TAG, "Start server");
    esp_err_t status = httpd_start(&_server, &server_config);

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

    // Register url handlers
    httpd_uri_t root = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = &RootHandlerStatic,
        .user_ctx = this,
        .is_websocket = false,
        .handle_ws_control_frames = false
    };

    httpd_register_uri_handler(_server, &led_endpoint);
    httpd_register_uri_handler(_server, &ws);
    httpd_register_uri_handler(_server, &root);

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

const char* HttpServer::_TAG = "HttpServer";

esp_err_t HttpServer::RootHandler(httpd_req_t* req)
{
    if (strcmp(req->uri, "/") == 0 || strcmp(req->uri, "/index.html") == 0)
    {
        // Serve HTML
        httpd_resp_set_type(req, "text/html");
        size_t html_len = index_html_end - index_html_start;
        if (index_html_start[html_len - 1] == '\0')
        {
            html_len--;  // Adjust length to exclude the null terminator
        }
        httpd_resp_send(req, (const char*)index_html_start, html_len);
    }
    else if (strcmp(req->uri, "/websocket.js") == 0)
    {
        // Serve JavaScript
        httpd_resp_set_type(req, "application/javascript");
        size_t js_len = websocket_js_end - websocket_js_start;
        if (websocket_js_start[js_len - 1] == '\0')
        {
            js_len--;  // Adjust length to exclude the null terminator
        }
        httpd_resp_send(req, (const char*)websocket_js_start, js_len);
    }
    else
    {
        // Handle not found
        httpd_resp_send_404(req);
    }
    return ESP_OK;
}

esp_err_t HttpServer::LedControlHttpHandler(httpd_req_t* req)
{
    // Null check for the request
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));

    esp_err_t status = ESP_OK;
    if (!req)
    {
        ESP_LOGI(_TAG, "The request is null");

        std::string root_string = ConstructFailedJsonResponse(400, "Bad Request", "Invalid Request");

        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "400 Bad Request"));
        status = httpd_resp_send(req, root_string.c_str(), HTTPD_RESP_USE_STRLEN);

        return status;
    }

    // check header to ensure it includes the content-type
    size_t json_header_value_length = httpd_req_get_hdr_value_len(req, "Content-Type");
    ESP_LOGI(_TAG, "Content-Type header length: %d", json_header_value_length);
    if (json_header_value_length == 0)
    {
        std::string failed_response_string = ConstructFailedJsonResponse(400, "Bad Request", "Must have header Content-Type");

        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "400 Bad Request"));
        status = httpd_resp_send(req, failed_response_string.c_str(), HTTPD_RESP_USE_STRLEN);

        return status;
    }

    std::unique_ptr<char[]> json_header_value(new char[json_header_value_length + 1]);
    esp_err_t get_json_header_status =
        httpd_req_get_hdr_value_str(
            req,
            "Content-Type",
            json_header_value.get(),
            json_header_value_length + 1
        );

    if (get_json_header_status != ESP_OK)
    {
        ESP_LOGI(_TAG, "Cannot get the header Content-Type: Error Code: %s", esp_err_to_name(get_json_header_status));
        std::string failed_response_string = ConstructFailedJsonResponse(400, "Bad Request", "Must have header Content-Type");

        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "400 Bad Request"));
        status = httpd_resp_send(req, failed_response_string.c_str(), HTTPD_RESP_USE_STRLEN);

        return status;
    }

    ESP_LOGI(_TAG, "Content-Type value: %s", json_header_value.get());

    // Ensure the content type is json
    if (strcmp(json_header_value.get(), "application/json") != 0)
    {
        ESP_LOGI(_TAG, "The Content-Type is not application/json");
        std::string failed_response_string = ConstructFailedJsonResponse(400, "Bad Request", "Type must be application json");

        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "400 Bad Request"));

        status = httpd_resp_send(req, failed_response_string.c_str(), HTTPD_RESP_USE_STRLEN);

        return status;
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

        std::string failed_response_string = ConstructFailedJsonResponse(error_status, error_code, error_message);

        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "500 Internal Server Error"));

        status = httpd_resp_send(req, failed_response_string.c_str(), HTTPD_RESP_USE_STRLEN);

        return status;
    }

    // parse the buffer json object
    ESP_LOGI(_TAG, "Start deserializing the object");
    cJSON* root = cJSON_Parse(content_buffer.get());
    std::string parsed_result = "";
    bool is_parse_successful = ParseStateRequestJson(content_buffer.get(), parsed_result);

    if (!is_parse_successful)
    {
        std::string failed_error_string = ConstructFailedJsonResponse(400, "Bad Request", parsed_result);

        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "400 Bad Request"));
        status = httpd_resp_send(req, failed_error_string.c_str(), HTTPD_RESP_USE_STRLEN);

        return status;
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

    std::string response_string = ConstructCurrentSstateMessage("on");

    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "200 OK"));
    status = httpd_resp_send(req, response_string.c_str(), HTTPD_RESP_USE_STRLEN);

    return status;
}

esp_err_t HttpServer::LedControlWebsocketHandler(httpd_req_t* req)
{
    esp_err_t status;
    int current_led_state = _led->GetState();
    std::string current_led_state_string = current_led_state == 1 ? "on" : "off";

    if (req->method == HTTP_GET)
    {
        ESP_LOGI(_TAG, "Handshake done, the new connection was opened");

        std::string esponse_string = ConstructCurrentSstateMessage(current_led_state_string);
        status = SendWebsocketTextMessage(req, esponse_string);
        return ESP_OK;
    }

    // initialize the websocket packet
    httpd_ws_frame_t received_ws_packet;
    memset(&received_ws_packet, 0, sizeof(httpd_ws_frame_t));
    received_ws_packet.type = HTTPD_WS_TYPE_TEXT;

    // get the length of the webpacket frame
    status = httpd_ws_recv_frame(req, &received_ws_packet, 0);
    if (status != ESP_OK)
    {
        ESP_LOGW(_TAG, "httpd_ws_recv_frame failed to get frame len with %s", esp_err_to_name(status));

        int file_descriptor = httpd_req_to_sockfd(req);
        RemoveClient(file_descriptor);

        ESP_LOGW(_TAG, "Remove the client id: %d from the list", file_descriptor);

        return status;
    }

    ESP_LOGI(_TAG, "frame length is %d", received_ws_packet.len);
    if (received_ws_packet.len == 0)
    {
        ESP_LOGI(_TAG, "The frame length is 0. Preparing to send error response");
        std::string failed_response_string = ConstructFailedJsonResponse(400, "Bad Request", "The message cannot be empty");
        ESP_LOGI(_TAG, "The error is: %s", failed_response_string.c_str());

        status = SendWebsocketTextMessage(req, failed_response_string);
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

        status = SendWebsocketTextMessage(req, failed_response_string);
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
        status = SendWebsocketTextMessage(req, failed_response_string);
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

    current_led_state = _led->GetState();
    current_led_state_string = current_led_state == 1 ? "on" : "off";
    std::string success_response_string = ConstructCurrentSstateMessage(current_led_state_string);
    status = SendWebsocketTextMessage(req, success_response_string);
    BroadCastMessage();
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
    std::string root_string = ConstructFailedJsonResponse(404, "Not Found", "The requested resource was not found on this server.");

    // Set the type to application json and header to 404
    // TODO: define the "application/json" and "404 Not Found" as Macros
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, root_string.c_str(), HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
}

void HttpServer::BroadCastMessage()
{
    std::string state = _led->GetState() == 1 ? "on" : "off";
    std::string message = ConstructCurrentSstateMessage(state);

    httpd_ws_frame_t ws_packet;
    memset(&ws_packet, 0, sizeof(httpd_ws_frame_t));

    ws_packet = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)message.c_str(),
        .len = message.length()
    };

    for (std::unordered_map<int, int>::iterator it = _clients.begin(); it != _clients.end(); it++)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_ws_send_frame_async(_server, it->first, &ws_packet));
    }
}

esp_err_t HttpServer::OnOpenConnection(int socket_file_descriptor)
{
    ESP_LOGI(_TAG, "A new connection is made ID: %d", socket_file_descriptor);
    AddClient(socket_file_descriptor, socket_file_descriptor);
    return ESP_OK;
}

esp_err_t HttpServer::OnCloseConnection(int socket_file_descriptor)
{
    ESP_LOGI(_TAG, "The connection is closed id: %d", socket_file_descriptor);
    RemoveClient(socket_file_descriptor);
    close(socket_file_descriptor);
    return ESP_OK;
}

void HttpServer::AddClient(const int& client_id, const int& file_descriptor)
{
    std::lock_guard<std::mutex> lock(_clientsMutex);
    std::unordered_map<int, int>::const_iterator found_client = _clients.find(client_id);
    if (found_client != _clients.end())
    {
        ESP_LOGW(_TAG, "The client id: %d was already in the client list", client_id);
    }

    _clients[client_id] = file_descriptor;
}

void HttpServer::RemoveClient(const int& client_id)
{
    std::lock_guard<std::mutex> lock(_clientsMutex);
    std::unordered_map<int, int>::const_iterator found_client = _clients.find(client_id);
    if (found_client == _clients.end())
    {
        ESP_LOGW(_TAG, "The client id: %d is not in the client list", client_id);
        return;
    }

    _clients.erase(client_id);
}

esp_err_t HttpServer::SendWebsocketTextMessage(httpd_req_t* req, const std::string& message)
{
    httpd_ws_frame_t ws_packet;
    memset(&ws_packet, 0, sizeof(httpd_ws_frame_t));

    ws_packet = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)message.c_str(),
        .len = message.length()
    };

    esp_err_t status = httpd_ws_send_frame(req, &ws_packet);

    ESP_LOGI(_TAG, "The status is %s", esp_err_to_name(status));
    return status;
}

/* Static Handler Wrapper */
esp_err_t HttpServer::RootHandlerStatic(httpd_req_t* req)
{
    auto* http_server = reinterpret_cast<HttpServer*>(req->user_ctx);
    return http_server->RootHandler(req);
}

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

esp_err_t HttpServer::OnOpenConnectionStatic(httpd_handle_t server_handle, int socket_file_descriptor)
{
    auto* http_server = reinterpret_cast<HttpServer*>(httpd_get_global_user_ctx(server_handle));
    return http_server->OnOpenConnection(socket_file_descriptor);
}

void HttpServer::OnCloseConnectionStatic(httpd_handle_t server_handle, int socket_file_descriptor)
{
    auto* http_server = reinterpret_cast<HttpServer*>(httpd_get_global_user_ctx(server_handle));
    http_server->OnCloseConnection(socket_file_descriptor);
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

std::string HttpServer::ConstructCurrentSstateMessage(const std::string& current_state)
{
    cJSON* response;
    response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", current_state.c_str());
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