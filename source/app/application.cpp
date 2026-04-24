#include "app/application.hpp"

#include <GLFW/glfw3.h>

namespace Details
{
    constexpr double WaitTimeoutSeconds = 1.0 / 60.0;

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
        return config;
    }
}  // namespace Details

namespace Aerkanis
{

    auto Application::init(ApplicationConfig nextConfig) -> bool
    {
        shutdown();
        config = Details::sanitizeConfig(nextConfig);
        if (!appWindow.init(config.width, config.height, config.title, config.resizable))
        {
            return false;
        }

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

    auto Application::requestQuit() noexcept -> void
    {
        running = false;
    }

    auto Application::shutdown() noexcept -> void
    {
        appWindow.shutdown();
        running = false;
    }

    auto Application::run() -> int
    {
        if (appWindow.nativeWindow == nullptr)
        {
            shutdown();
            return 1;
        }

        running = true;

        while (running && !appWindow.shouldClose())
        {
            glfwWaitEventsTimeout(Details::WaitTimeoutSeconds);
        }

        shutdown();
        return 0;
    }

}  // namespace Aerkanis
