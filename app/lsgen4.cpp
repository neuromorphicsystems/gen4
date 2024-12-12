#include "../common/psee.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    for (const auto& device : sepia::psee::available_devices()) {
        std::string type_as_string;
        switch (device.type) {
            case sepia::psee::EVK3_HD:
                type_as_string = "EVK3 HD";
                break;
            case sepia::psee::EVK4:
                type_as_string = "EVK4";
                break;
            default:
                type_as_string = std::to_string(device.type);
                break;
        }
        std::cout << device.serial << " (" << type_as_string << "), " << sepia::usb::device_speed_to_string(device.speed) << std::endl;
    }
}
