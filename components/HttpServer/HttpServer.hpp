#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include <esp_log.h>
#include <esp_http_server.h>
#include <memory>
#include <cJSON.h>
#include <mdns.h>
#include "LedControl.hpp"

class HttpServer
{
private:
    httpd_handle_t _server = NULL;
    std::shared_ptr<LedControl> _led;
    std::string _host_name;
    // esp_err_t _status;

    struct async_resp_arg
    {
        httpd_handle_t handle;
        int file_descriptor;
    };
    

    static const char* _TAG;
    static esp_err_t LedControlHandler(httpd_req_t*  req);
    static esp_err_t NotFoundHandler(httpd_req_t* req, httpd_err_code_t error);
    static esp_err_t WebsocketHandler(httpd_req_t* req);
    static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t* req);
    static void wsAsyncSendStatic(void* arg);
    static void wsAsyncSend(httpd_handle_t handle, int file_descriptor);
public:
    HttpServer(std::shared_ptr<LedControl> led, std::string host_name = "");
    ~HttpServer();

    esp_err_t Start();
    esp_err_t Stop();

    httpd_handle_t GetServer();
};

#endif