#include "../common/evk4.hpp"
#include "chameleon/source/background_cleaner.hpp"
#include "chameleon/source/dvs_display.hpp"
#include "pontella.hpp"
#include <QQmlPropertyMap>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

#pragma warning(disable : 4996)

inline std::string now() {
    auto time = std::time(nullptr);
    std::stringstream stream;
    stream << std::put_time(std::localtime(&time), "%FT%H-%M-%S");
    return stream.str();
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {"evk4_recorder displays and records events from an EVK4 camera"
         "Syntax: evk4_recorder [options]",
         "Available options:",
         "    -s [serial], --serial [serial]              sets the camera serial number",
         "                                                    defaults to first available",
         "    -r [directory], --recordings [directory]    sets the recordings directory",
         "                                                    defaults to the current directory",
         "    --bias-pr [integer]                         sets the \"pr\" bias (defaults to 124)",
         "    --bias-fo [integer]                         sets the \"fo\" bias (defaults to 83)",
         "    --bias-hpf [integer]                        sets the \"hpf\" bias (defaults to 0)",
         "    --bias-diff-on [integer]                    sets the \"diff_on\" bias (defaults to 102)",
         "    --bias-diff [integer]                       sets the \"diff\" bias (defaults to 77)",
         "    --bias-diff-off [integer]                   sets the \"diff_off\" bias (defaults to 73)",
         "    --bias-inv [integer]                        sets the \"inv\" bias (defaults to 91)",
         "    --bias-refr [integer]                       sets the \"refr\" bias (defaults to 20)",
         "    --bias-reqpuy [integer]                     sets the \"reqpuy\" bias (defaults to 140)",
         "    --bias-reqpux [integer]                     sets the \"reqpux\" bias (defaults to 124)",
         "    --bias-sendreqpdy [integer]                 sets the \"sendreqpdy\" bias (defaults to 148)",
         "    --bias-unknown-1 [integer]                  sets the \"unknown-1\" bias (defaults to 116)",
         "    --bias-unknown-2 [integer]                  sets the \"unknown-2\" bias (defaults to 81)",
         "    -h, --help                                  shows this help message"},
        argc,
        argv,
        0,
        {
            {"serial", {"s"}},
            {"recordings", {"r"}},
            {"log", {"l"}},
            {"bias-pr", {}},
            {"bias-fo", {}},
            {"bias-hpf", {}},
            {"bias-diff-on", {}},
            {"bias-diff", {}},
            {"bias-diff-off", {}},
            {"bias-inv", {}},
            {"bias-refr", {}},
            {"bias-reqpuy", {}},
            {"bias-reqpux", {}},
            {"bias-sendreqpdy", {}},
            {"bias-unknown-1", {}},
            {"bias-unknown-2", {}},
        },
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
            std::string recordings_directory = "";
            {
                auto recordings_directory_candidate = command.options.find("recordings");
                if (recordings_directory_candidate != command.options.end()) {
                    recordings_directory = recordings_directory_candidate->second;
                }
            }
            auto camera_parameters = sepia::evk4::default_parameters;
            for (const auto [index, name] : {
                     {0, "bias-pr"},
                     {1, "bias-fo"},
                     {2, "bias-hpf"},
                     {3, "bias-diff-on"},
                     {4, "bias-diff"},
                     {5, "bias-diff-off"},
                     {6, "bias-inv"},
                     {7, "bias-refr"},
                     {8, "bias-reqpuy"},
                     {9, "bias-reqpux"},
                     {10, "bias-sendreqpdy"},
                     {11, "bias-unknown-1"},
                     {12, "bias-unknown-2"},
                 }) {
                auto bias_candidate = command.options.find(name);
                if (bias_candidate != command.options.end()) {
                    const auto candidate_value = std::stoll(bias_candidate);
                    if (candidate_value < 0 || candidate_value > 255) {
                        throw std::runtime_error(
                            name + " must be in the range [0, 255] (got " + std::to_string(candidate_value) + ")");
                    }
                    switch (index) {
                        case 0:
                            camera_parameters.biases.pr = static_cast<uint8_t>(candidate_value);
                            break;
                        case 1:
                            camera_parameters.biases.fo = static_cast<uint8_t>(candidate_value);
                            break;
                        case 2:
                            camera_parameters.biases.hpf = static_cast<uint8_t>(candidate_value);
                            break;
                        case 3:
                            camera_parameters.biases.diff_on = static_cast<uint8_t>(candidate_value);
                            break;
                        case 4:
                            camera_parameters.biases.diff = static_cast<uint8_t>(candidate_value);
                            break;
                        case 5:
                            camera_parameters.biases.diff_off = static_cast<uint8_t>(candidate_value);
                            break;
                        case 6:
                            camera_parameters.biases.inv = static_cast<uint8_t>(candidate_value);
                            break;
                        case 7:
                            camera_parameters.biases.refr = static_cast<uint8_t>(candidate_value);
                            break;
                        case 8:
                            camera_parameters.biases.reqpuy = static_cast<uint8_t>(candidate_value);
                            break;
                        case 9:
                            camera_parameters.biases.reqpux = static_cast<uint8_t>(candidate_value);
                            break;
                        case 10:
                            camera_parameters.biases.sendreqpdy = static_cast<uint8_t>(candidate_value);
                            break;
                        case 11:
                            camera_parameters.biases.unknown_1 = static_cast<uint8_t>(candidate_value);
                            break;
                        case 12:
                            camera_parameters.biases.unknown_2 = static_cast<uint8_t>(candidate_value);
                            break;
                        default:
                            throw std::logic_error("unexpected bias index");
                    }
                }
            }

            QQmlPropertyMap parameters;
            QGuiApplication app(argc, argv);
            qmlRegisterType<chameleon::background_cleaner>("Chameleon", 1, 0, "BackgroundCleaner");
            qmlRegisterType<chameleon::dvs_display>("Chameleon", 1, 0, "DvsDisplay");
            QQmlApplicationEngine application_engine;
            application_engine.rootContext()->setContextProperty("header_width", sepia::evk4::width);
            application_engine.rootContext()->setContextProperty("header_height", sepia::evk4::height);
            application_engine.loadData(
#include "evk4_recorder.qml.hpp"
            );
            auto window = qobject_cast<QQuickWindow*>(application_engine.rootObjects().first());
            {
                QSurfaceFormat format;
                format.setDepthBufferSize(24);
                format.setStencilBufferSize(8);
                format.setVersion(3, 3);
                format.setProfile(QSurfaceFormat::CoreProfile);
                window->setFormat(format);
            }
            auto dvs_display = window->findChild<chameleon::dvs_display*>("dvs_display");

            auto local_pause = false;
            std::string filename;
            std::unique_ptr<sepia::write<sepia::type::dvs>> write;
            uint64_t initial_t = 0;
            bool initial_t_set = false;
            std::unique_ptr<sepia::evk4::base_camera> camera;
            std::size_t trigger_count = 0;
            camera = sepia::evk4::make_camera(
                [&](sepia::dvs_event event) {
                    if (!local_pause) {
                        dvs_display->push_unsafe(event);
                    }
                    if (write) {
                        if (!initial_t_set) {
                            initial_t_set = true;
                            initial_t = event.t;
                        }
                        event.t -= initial_t;
                        write->operator()(event);
                    }
                },
                [&](sepia::evk4::trigger_event trigger) { ++trigger_count; },
                [&]() { dvs_display->lock(); },
                [&]() { dvs_display->unlock(); },
                [&](std::exception_ptr exception) {
                    try {
                        std::rethrow_exception(exception);
                    } catch (const std::exception& exception) {
                        std::cerr << exception.what() << std::endl;
                    }
                    app.quit();
                });
            camera->set_drop_threshold(64);
            auto return_value = app.exec();
            if (return_value > 0) {
                throw std::runtime_error("qt returned a non-zero code");
            }
        });
}
