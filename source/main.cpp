#include <exception>
#include <iostream>

#include "app/application.hpp"

auto main() -> int
{
    try
    {
        Aerkanis::Application app{};
        if (!app.init({}))
        {
            std::cerr << "Aerkanis fatal error: failed to initialize application\n";
            return 1;
        }
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Aerkanis fatal error: " << e.what() << '\n';
        return 1;
    }
}
