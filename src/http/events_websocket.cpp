#include "ssheila/http/events_websocket.hpp"

#include <json/json.h>

#include <mutex>
#include <unordered_set>

namespace ssheila::http {
namespace {

std::mutex connectionsMutex;
std::unordered_set<drogon::WebSocketConnectionPtr> connections;

}  // namespace

void EventsWebSocket::handleNewMessage(const drogon::WebSocketConnectionPtr& connection,
                                       std::string&& message,
                                       const drogon::WebSocketMessageType& type) {
    if (type == drogon::WebSocketMessageType::Ping) {
        connection->send(message, drogon::WebSocketMessageType::Pong);
    }
}

void EventsWebSocket::handleNewConnection(const drogon::HttpRequestPtr&,
                                          const drogon::WebSocketConnectionPtr& connection) {
    {
        std::lock_guard lock{connectionsMutex};
        connections.insert(connection);
    }
    Json::Value event;
    event["type"] = "system.connected";
    event["service"] = "sSheila";
    connection->send(event.toStyledString());
}

void EventsWebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr& connection) {
    std::lock_guard lock{connectionsMutex};
    connections.erase(connection);
}

void EventsWebSocket::publish(const std::string& message) {
    std::lock_guard lock{connectionsMutex};
    for (const auto& connection : connections) {
        if (connection->connected()) {
            connection->send(message);
        }
    }
}

}  // namespace ssheila::http
