#include "antropy_farm_gateway/service.hpp"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        std::string settings_path = "config/settings.json";
        if (argc > 1) settings_path = argv[1];
        auto settings = antropy::gateway::load_settings(settings_path);
        antropy::gateway::GatewayService service(std::move(settings));
        service.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "antropy_farm_gateway error: " << e.what() << std::endl;
        return 1;
    }
}
