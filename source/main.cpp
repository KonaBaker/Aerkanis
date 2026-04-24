#include <exception>
#include <iostream>

#include "app/application.hpp"

auto main() -> int
{
    try
    {
        Aerkanis::Application app{};
        app.init({});
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Aerkanis fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
