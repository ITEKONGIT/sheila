#pragma once

#include <drogon/WebSocketController.h>

#include <string>

namespace ssheila::http {

class EventsWebSocket : public drogon::WebSocketController<EventsWebSocket> {
public:
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/api/v1/events");
    WS_PATH_LIST_END

    void handleNewMessage(const drogon::WebSocketConnectionPtr& connection,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& request,
                             const drogon::WebSocketConnectionPtr& connection) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& connection) override;

    static void publish(const std::string& message);
};

}  // namespace ssheila::http
