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
         "    -h, --help                                  shows this help message"},
        argc,
        argv,
        0,
        {{"serial", {"s"}}, {"recordings", {"r"}}, {"log", {"l"}}},
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

            auto camera_parameters = sepia::evk4::default_parameters;
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
