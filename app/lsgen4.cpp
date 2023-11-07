#include "../common/evk4.hpp"
#include "../common/psee413.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    for (const auto& device : sepia::evk4::available_devices()) {
        std::cout << device.serial << " (EVK4), " << sepia::usb::device_speed_to_string(device.speed) << std::endl;
    }
    for (const auto& device : sepia::psee413::available_devices()) {
        std::cout << device.serial << " (PSEE413), " << sepia::usb::device_speed_to_string(device.speed) << std::endl;
    }
}
