#pragma once

#include <functional>
#include <string>

#include "app/window.hpp"

namespace Aerkanis {

struct ApplicationConfig {
    int width = 1280;
    int height = 720;
    std::string title = "Aerkanis";
    bool resizable = true;
};

struct Application {
    using FrameCallback = std::function<void(float)>;
    using ShutdownCallback = std::function<void()>;

    ApplicationConfig config{};
    Window appWindow{};
    FrameCallback frameCallback{};
    ShutdownCallback shutdownCallback{};
    bool running{false};

    auto init(ApplicationConfig nextConfig = {}) -> bool;
    auto window() noexcept -> Window&;
    auto window() const noexcept -> const Window&;
    auto setFrameCallback(FrameCallback callback) -> void;
    auto setShutdownCallback(ShutdownCallback callback) -> void;
    auto requestQuit() noexcept -> void;
    auto shutdown() noexcept -> void;
    auto run() -> int;
};

}  // namespace Aerkanis
