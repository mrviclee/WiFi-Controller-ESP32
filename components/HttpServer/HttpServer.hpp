#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include <esp_log.h>
#include <esp_http_server.h>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <cJSON.h>
#include <mdns.h>
#include "LedControl.hpp"

class HttpServer
{
private:
    httpd_handle_t _server = NULL;
    std::shared_ptr<LedControl> _led;
    std::string _host_name;
    std::unordered_map<int, int> _clients;
    std::mutex _clientsMutex;

    static const char* _TAG;

    esp_err_t RootHandler(httpd_req_t* req);
    esp_err_t LedControlHttpHandler(httpd_req_t* req);
    esp_err_t NotFoundHandler(httpd_req_t* req, httpd_err_code_t error);
    esp_err_t LedControlWebsocketHandler(httpd_req_t* req);
    void BroadCastMessage();

    void AddClient(const int& client_id, const int& file_descriptor);
    void RemoveClient(const int& client_id);

    esp_err_t SendWebsocketTextMessage(httpd_req_t* req, const std::string& message);

    esp_err_t OnOpenConnection(int socket_file_descriptor);
    esp_err_t OnCloseConnection(int socket_file_descriptor);
public:
    HttpServer(httpd_handle_t server, std::shared_ptr<LedControl> led, std::string host_name = "");
    ~HttpServer();

    esp_err_t Start();
    esp_err_t Stop();

    static esp_err_t RootHandlerStatic(httpd_req_t* req);
    static esp_err_t LedControlHttpHandlerStatic(httpd_req_t* req);
    static esp_err_t NotFoundHandlerStatic(httpd_req_t* req, httpd_err_code_t error);
    static esp_err_t LedControlWebSocketHandlerStatic(httpd_req_t* req);
    static esp_err_t OnOpenConnectionStatic(httpd_handle_t server_handle, int socket_file_descriptor);
    static void OnCloseConnectionStatic(httpd_handle_t server_handle, int socket_file_descriptor);

    httpd_handle_t GetServer();

    // Helper Methods
    static std::string ConstructFailedJsonResponse(const uint16_t& error_status, const std::string& error_code, const std::string& error_message);
    static std::string ConstructCurrentSstateMessage(const std::string& current_state);

    
    /// @brief Parse the JSON request for the state
    /// {
    ///     "state": "on"
    /// }
    /// @param request The request body in string/char* format
    /// @param output_parsed_state The parsed string => if it succeds(returned true), then it will be either "on" or "off". Otherwise, it will be the error message.
    /// @return The boolean represents if the parse is sucessful or not. True will assign the out_parsed_state to the parsed state. False will assign the out_parsed_state to the error message.
    static bool ParseStateRequestJson(const char* request, std::string& output_parsed_state);
};

#endif