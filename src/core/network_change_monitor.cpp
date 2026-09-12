#include "ssheila/core/network_change_monitor.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <iphlpapi.h>
#include <windows.h>
#else
#include <cerrno>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ssheila::core {

struct NetworkChangeMonitor::Impl {
    explicit Impl(Callback callbackIn)
        : callback(std::move(callbackIn)) {}

    Callback callback;
    std::thread thread;
    bool started{false};
    bool stopping{false};

#ifdef _WIN32
    std::mutex mutex;
    std::condition_variable condition;
    bool pending{false};
    HANDLE notification{nullptr};

    void request_refresh() {
        {
            const std::lock_guard lock{mutex};
            pending = true;
        }
        condition.notify_one();
    }

    static void CALLBACK address_change_callback(
        PVOID context,
        PMIB_UNICASTIPADDRESS_ROW,
        MIB_NOTIFICATION_TYPE) {
        static_cast<Impl*>(context)->request_refresh();
    }

    void run() {
        std::unique_lock lock{mutex};
        while (!stopping) {
            condition.wait(lock, [this] { return stopping || pending; });
            if (stopping) {
                break;
            }

            pending = false;
            // Coalesce the address/interface notifications Windows emits for
            // a single Wi-Fi transition into one discovery pass.
            while (!stopping) {
                if (condition.wait_for(lock, std::chrono::milliseconds(250),
                                       [this] { return stopping || pending; })) {
                    if (stopping) {
                        break;
                    }
                    pending = false;
                    continue;
                }
                break;
            }
            if (stopping) {
                break;
            }

            lock.unlock();
            callback();
            lock.lock();
        }
    }
#else
    int socket{-1};

    void run() {
        bool pending = false;
        auto deadline = std::chrono::steady_clock::time_point::max();
        std::array<char, 8192> buffer{};

        while (!stopping) {
            int timeout = 1000;
            if (pending) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
                timeout = remaining <= 0 ? 0 : static_cast<int>(remaining);
            }

            pollfd descriptor{socket, POLLIN, 0};
            const auto ready = poll(&descriptor, 1, timeout);
            if (ready > 0 && (descriptor.revents & POLLIN) != 0) {
                while (recv(socket, buffer.data(), buffer.size(), MSG_DONTWAIT) > 0) {
                }
                pending = true;
                deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            }

            if (pending && std::chrono::steady_clock::now() >= deadline) {
                pending = false;
                deadline = std::chrono::steady_clock::time_point::max();
                callback();
            }
        }
    }
#endif
};

NetworkChangeMonitor::NetworkChangeMonitor(Callback callback)
    : impl_(std::make_unique<Impl>(std::move(callback))) {}

NetworkChangeMonitor::~NetworkChangeMonitor() {
    stop();
}

bool NetworkChangeMonitor::start(std::string& error) {
    if (impl_->started) {
        return true;
    }

#ifdef _WIN32
    auto status = NotifyUnicastIpAddressChange(
        AF_INET, &Impl::address_change_callback, impl_.get(), FALSE, &impl_->notification);
    if (status != NO_ERROR) {
        error = "Could not subscribe to Windows IPv4 address changes: error " +
                std::to_string(status);
        return false;
    }
#else
    impl_->socket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (impl_->socket < 0) {
        error = "Could not create the Linux network-change socket: " +
                std::string{std::strerror(errno)};
        return false;
    }

    sockaddr_nl address{};
    address.nl_family = AF_NETLINK;
    address.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE;
    if (bind(impl_->socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        error = "Could not subscribe to Linux network changes: " +
                std::string{std::strerror(errno)};
        close(impl_->socket);
        impl_->socket = -1;
        return false;
    }
#endif

    impl_->stopping = false;
    try {
        impl_->thread = std::thread([impl = impl_.get()] { impl->run(); });
    } catch (const std::exception& exception) {
#ifdef _WIN32
        CancelMibChangeNotify2(impl_->notification);
        impl_->notification = nullptr;
#else
        close(impl_->socket);
        impl_->socket = -1;
#endif
        error = "Could not start the network-change watcher: " + std::string{exception.what()};
        return false;
    }
    impl_->started = true;
    return true;
}

void NetworkChangeMonitor::stop() {
    if (!impl_->started) {
        return;
    }

#ifdef _WIN32
    {
        const std::lock_guard lock{impl_->mutex};
        impl_->stopping = true;
    }
    impl_->condition.notify_one();
    CancelMibChangeNotify2(impl_->notification);
    impl_->notification = nullptr;
#else
    impl_->stopping = true;
#endif
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
#ifndef _WIN32
    close(impl_->socket);
    impl_->socket = -1;
#endif
    impl_->started = false;
}

}  // namespace ssheila::core
