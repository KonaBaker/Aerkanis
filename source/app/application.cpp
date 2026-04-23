#include "app/application.hpp"

#include <chrono>
#include <thread>
#include <utility>

namespace Detail
{
    auto sanitizeConfig(Aerkanis::ApplicationConfig config) -> Aerkanis::ApplicationConfig
    {
        if (config.width <= 0)
        {
            config.width = 1280;
        }
        if (config.height <= 0)
        {
            config.height = 720;
        }
        if (config.title.empty())
        {
            config.title = "Aerkanis";
        }
        config.resizable = true;
        return config;
    }
}  // namespace Detail

namespace Aerkanis
{

    auto Application::init(ApplicationConfig nextConfig) -> bool
    {
        config = Detail::sanitizeConfig(nextConfig);
        if (!appWindow.init(config.width, config.height, config.title, config.resizable))
        {
            return false;
        }

        appWindow.setCloseCallback([this]() { requestQuit(); });
        running = false;
        return true;
    }

    auto Application::window() noexcept -> Window&
    {
        return appWindow;
    }

    auto Application::window() const noexcept -> const Window&
    {
        return appWindow;
    }

    auto Application::setFrameCallback(FrameCallback callback) -> void
    {
        frameCallback = std::move(callback);
    }

    auto Application::setShutdownCallback(ShutdownCallback callback) -> void
    {
        shutdownCallback = std::move(callback);
    }

    auto Application::requestQuit() noexcept -> void
    {
        running = false;
    }

    auto Application::shutdown() noexcept -> void
    {
        auto nextShutdown = std::move(shutdownCallback);
        if (nextShutdown)
        {
            nextShutdown();
        }

        appWindow.shutdown();
        running = false;
    }

    auto Application::run() -> int
    {
        using clock = std::chrono::steady_clock;

        if (appWindow.handle() == nullptr)
        {
            return 1;
        }

        running = true;
        auto lastFrame = clock::now();

        while (running)
        {
            appWindow.pollEvents();
            if (!running || appWindow.shouldClose())
            {
                break;
            }

            const auto now = clock::now();
            const std::chrono::duration<float> delta = now - lastFrame;
            lastFrame = now;

            if (frameCallback)
            {
                frameCallback(delta.count());
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        shutdown();
        return 0;
    }

}  // namespace Aerkanis
