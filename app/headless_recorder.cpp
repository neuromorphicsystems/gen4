#include "../common/evk4.hpp"
#include "pontella.hpp"

#pragma warning(disable : 4996)

int main(int argc, char* argv[]) {
    return pontella::main(
        {"headless_recorder records events from an EVK4 camera"
         "Syntax: headless_recorder [options] /path/to/output.es",
         "Available options:",
         "    -s [serial], --serial [serial]    sets the camera serial number",
         "                                          defaults to first available",
         "    -h, --help                        shows this help message"},
        argc,
        argv,
        1,
        {{"serial", {"s"}}},
        {},
        [&](pontella::command command) {
            const auto available_serials = sepia::evk4::available_serials();
            std::string serial = "";
            {
                auto serial_candidate = command.options.find("serial");
                if (serial_candidate == command.options.end()) {
                    if (available_serials.empty()) {
                        throw sepia::no_device_connected(sepia::evk4::name);
                    }
                    serial = available_serials.front();
                } else {
                    serial = serial_candidate->second;
                    auto found = false;
                    for (const auto& available_serial : available_serials) {
                        if (available_serial == serial) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        throw sepia::usb::serial_not_available(sepia::evk4::name, serial);
                    }
                }
            }
            sepia::write<sepia::type::dvs> write(sepia::filename_to_ofstream(command.arguments[0]), sepia::evk4::width, sepia::evk4::height);

            auto camera_parameters = sepia::evk4::default_parameters;
            bool initial_t_set = false;
            uint64_t initial_t = 0;
            uint64_t threshold_t = 0;
            uint64_t count = 0;
            auto camera = sepia::evk4::make_camera(
                [&](sepia::dvs_event event) {
                    if (!initial_t_set) {
                        initial_t_set = true;
                        initial_t = event.t;
                    }
                    event.t -= initial_t;
                    if (event.t > threshold_t) {
                        printf("%llu ev/s\n", count); // @DEV
                        count = 0;
                        threshold_t += 1000000;
                    }
                    ++count;
                    write(event);
                },
                [&](sepia::evk4::trigger_event trigger) { },
                [&]() { },
                [&]() { },
                [&](std::exception_ptr exception) {
                    try {
                        std::rethrow_exception(exception);
                    } catch (const std::exception& exception) {
                        std::cerr << exception.what() << std::endl;
                    }
                });
            std::cout << "Press return to stop";
            std::cout.flush();
            std::cin.get();
        });
}
