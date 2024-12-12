#pragma once

#include <iomanip>
#include <sstream>

#include "usb.hpp"

namespace sepia {
    namespace psee {

        const uint32_t EVK3_HD = 1;
        const uint32_t EVK4 = 2;

        std::initializer_list<usb::identity> identities = {
            {0x04b4, 0x00f4},
            {0x04b4, 0x00f5},
            {0x31f7, 0x0003},
        };

        /// returns the device type and serial
        ///
        /// type map
        /// 0: reserved (any)
        /// 1: EVK3 HD
        /// 2: EVK4
        std::pair<uint32_t, std::string> get_type_and_serial(sepia::usb::interface& interface) {
            const auto type_buffer = interface.control_transfer(
                "sensor type",
                0xC0,
                0x72,
                0x00,
                0x00,
                std::vector<uint8_t>(2)
            );
            uint32_t type = 0;
            if (type_buffer[0] == 0x30) {
                type = EVK3_HD;
            } else if (type_buffer[0] == 0x31) {
                type = EVK4;
            }
            interface.bulk_transfer("serial request", 0x02, {0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
            const auto buffer = interface.bulk_transfer("serial response", 0x82, std::vector<uint8_t>(16));
            std::stringstream serial;
            serial << std::hex;
            for (uint8_t index = 11; index > 7; --index) {
                serial << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(buffer[index]);
            }
            return {type, serial.str()};
        }

        /// available_devices returns a list of connected devices serials and speeds.
        inline std::vector<usb::device_properties> available_devices() {
            return usb::available_devices(identities, get_type_and_serial);
        }
    }
}
