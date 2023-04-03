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

constexpr uint64_t event_rate_resolution = 50000; // µs
constexpr uint64_t event_rate_chunks = 20;

#define return_match(bias_name)                                                                                        \
    if (name == #bias_name) {                                                                                          \
        return camera_parameters.biases.bias_name;                                                                     \
    }

#define assign_and_return_match(bias_name)                                                                             \
    if (name == #bias_name) {                                                                                          \
        camera_parameters.biases.bias_name = value;                                                                    \
        return;                                                                                                        \
    }

#define if_match(bias_name)                                                                                            \
    if (!found && name == #bias_name) {                                                                                \
        camera_parameters.biases.bias_name = static_cast<uint8_t>(value.toUInt());                                     \
        found = true;                                                                                                  \
    }

uint8_t name_to_bias(const sepia::evk4::parameters& camera_parameters, const std::string& name) {
    return_match(pr);
    return_match(fo);
    return_match(hpf);
    return_match(diff_on);
    return_match(diff);
    return_match(diff_off);
    return_match(inv);
    return_match(refr);
    return_match(reqpuy);
    return_match(reqpux);
    return_match(sendreqpdy);
    return_match(unknown_1);
    return_match(unknown_2);
    throw std::runtime_error(std::string("unknown bias name \"") + name + "\"");
}

void set_bias_with_name(sepia::evk4::parameters& camera_parameters, const std::string& name, uint8_t value) {
    assign_and_return_match(pr);
    assign_and_return_match(fo);
    assign_and_return_match(hpf);
    assign_and_return_match(diff_on);
    assign_and_return_match(diff);
    assign_and_return_match(diff_off);
    assign_and_return_match(inv);
    assign_and_return_match(refr);
    assign_and_return_match(reqpuy);
    assign_and_return_match(reqpux);
    assign_and_return_match(sendreqpdy);
    assign_and_return_match(unknown_1);
    assign_and_return_match(unknown_2);
    throw std::runtime_error(std::string("unknown bias name \"") + name + "\"");
}

std::pair<std::string, std::string> utc_timestamp_and_filename() {
    std::timespec timespec;
    std::timespec_get(&timespec, TIME_UTC);
    std::stringstream timestamp;
    std::stringstream filename;
    timestamp << std::put_time(std::gmtime(&timespec.tv_sec), "%FT%T.") << std::setfill('0') << std::setw(6)
              << (timespec.tv_nsec / 1000) << "Z";
    filename << std::put_time(std::gmtime(&timespec.tv_sec), "%FT%H-%M-%SZ");
    return {timestamp.str(), filename.str()};
}

std::string utc_timestamp() {
    std::timespec timespec;
    std::timespec_get(&timespec, TIME_UTC);
    std::stringstream timestamp;
    timestamp << std::put_time(std::gmtime(&timespec.tv_sec), "%FT%T.") << std::setfill('0') << std::setw(6)
              << (timespec.tv_nsec / 1000) << "Z";
    return timestamp.str();
}

QString duration_and_size_to_string(uint64_t duration, uint64_t size) {
    const auto seconds = static_cast<uint64_t>(std::round(static_cast<double>(duration) / 1e6));
    QString output;
    QTextStream stream(&output);
    if (seconds < 300) {
        stream << seconds << " s";
    } else if (seconds < 18000) {
        stream << (seconds / 60) << " min";
    } else if (seconds < 432000) {
        stream << (seconds / 3600) << " h";
    } else {
        stream << (seconds / 86400) << " days";
    }
    stream << " (";
    if (size < 1000) {
        stream << size << " B";
    } else {
        QTextStream stream(&output);
        stream.setRealNumberNotation(QTextStream::FixedNotation);
        stream.setRealNumberPrecision(2);
        const auto real_size = static_cast<double>(size);
        if (size < 1000000) {
            stream << (real_size / 1e3) << " kB";
        } else if (size < 1000000000) {
            stream << (real_size / 1e6) << " MB";
        } else if (size < 1000000000000) {
            stream << (real_size / 1e9) << " GB";
        } else {
            stream << (real_size / 1e12) << " TB";
        }
    }
    stream << ")";
    return output;
}

void control_log(
    std::ofstream& control_events,
    const std::string& timestamp,
    const std::string& type,
    const std::string& payload) {
    std::stringstream message;
    message << "{\"t\":\"" << timestamp << "\",\"type\":\"" << type << "\",\"payload\":\"" << payload << "\"}\n";
    control_events << message.rdbuf();
    control_events.flush();
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
         "    --pr [integer]                              sets the \"pr\" bias (defaults to 124)",
         "    --fo [integer]                              sets the \"fo\" bias (defaults to 83)",
         "    --hpf [integer]                             sets the \"hpf\" bias (defaults to 0)",
         "    --diff-on [integer]                         sets the \"diff_on\" bias (defaults to 102)",
         "    --diff [integer]                            sets the \"diff\" bias (defaults to 77)",
         "    --diff-off [integer]                        sets the \"diff_off\" bias (defaults to 73)",
         "    --inv [integer]                             sets the \"inv\" bias (defaults to 91)",
         "    --refr [integer]                            sets the \"refr\" bias (defaults to 20)",
         "    --reqpuy [integer]                          sets the \"reqpuy\" bias (defaults to 140)",
         "    --reqpux [integer]                          sets the \"reqpux\" bias (defaults to 124)",
         "    --sendreqpdy [integer]                      sets the \"sendreqpdy\" bias (defaults to 148)",
         "    --unknown-1 [integer]                       sets the \"unknown-1\" bias (defaults to 116)",
         "    --unknown-2 [integer]                       sets the \"unknown-2\" bias (defaults to 81)",
         "    -h, --help                                  shows this help message"},
        argc,
        argv,
        0,
        {
            {"serial", {"s"}},
            {"recordings", {"r"}},
            {"log", {"l"}},
            {"pr", {}},
            {"fo", {}},
            {"hpf", {}},
            {"diff-on", {}},
            {"diff", {}},
            {"diff-off", {}},
            {"inv", {}},
            {"refr", {}},
            {"reqpuy", {}},
            {"reqpux", {}},
            {"sendreqpdy", {}},
            {"unknown-1", {}},
            {"unknown-2", {}},
        },
        {},
        [&](pontella::command command) {
            QQmlPropertyMap parameters;

            // serial
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
            parameters.insert("serial", QString::fromStdString(serial));

            // event rate, recording status, and file size
            parameters.insert("event_rate", QString("0 ev/s"));

            // recordings directory
            std::string recordings_directory;
            {
                auto recordings_directory_candidate = command.options.find("recordings");
                if (recordings_directory_candidate != command.options.end()) {
                    recordings_directory = recordings_directory_candidate->second;
                }
            }
            parameters.insert(
                "recordings_directory",
                recordings_directory.empty() ? QString("./") : QString::fromStdString(recordings_directory));
            std::ofstream control_events(
                sepia::join({recordings_directory, serial + "_control_events.jsonl"}), std::ostream::app);
            parameters.insert("recording_name", QVariant());
            parameters.insert("recording_status", QVariant());

            // camera parameters
            auto camera_parameters = sepia::evk4::default_parameters;
            const auto initialisation_timestamp = utc_timestamp();
            for (const auto& [option_name, name] : {
                     std::pair<std::string, std::string>{"pr", "pr"},
                     std::pair<std::string, std::string>{"fo", "fo"},
                     std::pair<std::string, std::string>{"hpf", "hpf"},
                     std::pair<std::string, std::string>{"diff-on", "diff_on"},
                     std::pair<std::string, std::string>{"diff", "diff"},
                     std::pair<std::string, std::string>{"diff-off", "diff_off"},
                     std::pair<std::string, std::string>{"inv", "inv"},
                     std::pair<std::string, std::string>{"refr", "refr"},
                     std::pair<std::string, std::string>{"reqpuy", "reqpuy"},
                     std::pair<std::string, std::string>{"reqpux", "reqpux"},
                     std::pair<std::string, std::string>{"sendreqpdy", "sendreqpdy"},
                     std::pair<std::string, std::string>{"unknown-1", "unknown_1"},
                     std::pair<std::string, std::string>{"unknown-2", "unknown_2"},
                 }) {
                auto bias_candidate = command.options.find(option_name);
                if (bias_candidate != command.options.end()) {
                    const auto candidate_value = std::stoll(bias_candidate->second);
                    if (candidate_value < 0 || candidate_value > 255) {
                        throw std::runtime_error(
                            option_name + " must be in the range [0, 255] (got " + std::to_string(candidate_value)
                            + ")");
                    }
                    set_bias_with_name(camera_parameters, name, static_cast<uint8_t>(candidate_value));
                }
                parameters.insert(QString::fromStdString(name), name_to_bias(camera_parameters, name));
                control_log(
                    control_events,
                    initialisation_timestamp,
                    name,
                    std::to_string(static_cast<int32_t>(name_to_bias(camera_parameters, name))));
            }

            // user interface events
            std::atomic_flag accessing_shared;
            auto bias_update_required = false;
            auto recording_start_required = false;
            auto recording_stop_required = false;
            auto shared_flip_left_right = false;
            auto shared_flip_bottom_top = false;
            accessing_shared.clear(std::memory_order_release);
            QQmlPropertyMap::connect(
                &parameters,
                &QQmlPropertyMap::valueChanged,
                &parameters,
                [&](const QString& name, const QVariant& value) {
                    while (accessing_shared.test_and_set(std::memory_order_acquire)) {
                    }
                    if (name == "recording_start_required") {
                        recording_start_required = true;
                    } else if (name == "recording_stop_required") {
                        recording_stop_required = true;
                    } else if (name == "Flip left-right") {
                        shared_flip_left_right = value.toBool();
                    } else if (name == "Flip bottom-top") {
                        shared_flip_bottom_top = value.toBool();
                    } else {
                        auto found = false;
                        if_match(pr);
                        if_match(fo);
                        if_match(hpf);
                        if_match(diff_on);
                        if_match(diff);
                        if_match(diff_off);
                        if_match(inv);
                        if_match(refr);
                        if_match(reqpuy);
                        if_match(reqpux);
                        if_match(sendreqpdy);
                        if_match(unknown_1);
                        if_match(unknown_2);
                        if (found) {
                            bias_update_required = true;
                            control_log(
                                control_events,
                                utc_timestamp(),
                                name.toStdString(),
                                std::to_string(
                                    static_cast<int32_t>(name_to_bias(camera_parameters, name.toStdString()))));
                        } else {
                            std::cerr << (std::string("unknown parameter \"") + name.toStdString() + "\"\n");
                            std::cerr.flush();
                        }
                    }
                    accessing_shared.clear(std::memory_order_release);
                });

            QGuiApplication app(argc, argv);
            qmlRegisterType<chameleon::background_cleaner>("Chameleon", 1, 0, "BackgroundCleaner");
            qmlRegisterType<chameleon::dvs_display>("Chameleon", 1, 0, "DvsDisplay");
            QQmlApplicationEngine application_engine;
            QFont font("Monospace");
            font.setStyleHint(QFont::Monospace);
            font.setPointSize(15);
            application_engine.rootContext()->setContextProperty("monospace_font", font);
            application_engine.rootContext()->setContextProperty("header_width", sepia::evk4::width);
            application_engine.rootContext()->setContextProperty("header_height", sepia::evk4::height);
            application_engine.rootContext()->setContextProperty("parameters", &parameters);
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

            const auto event_rate_factor = 1e6 / static_cast<double>(event_rate_resolution * event_rate_chunks);

            std::string filename;
            std::unique_ptr<sepia::write<sepia::type::dvs>> write;
            uint64_t initial_t = 0;
            uint64_t previous_t = 0;
            auto initial_t_set = false;
            auto flip_left_right = false;
            auto flip_bottom_top = false;
            std::unique_ptr<sepia::evk4::base_camera> camera;
            std::size_t trigger_count = 0;
            std::vector<std::size_t> chunk_to_count(event_rate_chunks + 1, 0);
            std::size_t previous_active_chunk_index = 0;
            std::size_t active_chunk_index = 0;
            uint64_t active_chunk_threshold_t = 0;
            const auto locale = QLocale(QLocale::English, QLocale::Country::Australia);
            camera = sepia::evk4::make_camera(
                [&](sepia::dvs_event event) {
                    while (event.t > active_chunk_threshold_t) {
                        active_chunk_index = (active_chunk_index + 1) % chunk_to_count.size();
                        chunk_to_count[active_chunk_index] = 0;
                        active_chunk_threshold_t += event_rate_resolution;
                    }
                    ++chunk_to_count[active_chunk_index];
                    auto display_event = event;
                    if (flip_left_right) {
                        display_event.x = sepia::evk4::width - 1 - display_event.x;
                    }
                    if (flip_bottom_top) {
                        display_event.y = sepia::evk4::height - 1 - display_event.y;
                    }
                    dvs_display->push_unsafe(display_event);
                    if (write) {
                        if (!initial_t_set) {
                            initial_t_set = true;
                            initial_t = event.t;
                        }
                        previous_t = event.t;
                        event.t -= initial_t;
                        write->operator()(event);
                    }
                },
                [&](sepia::evk4::trigger_event trigger) { ++trigger_count; },
                [&]() { dvs_display->lock(); },
                [&]() {
                    dvs_display->unlock();
                    while (accessing_shared.test_and_set(std::memory_order_acquire)) {
                    }
                    if (bias_update_required) {
                        bias_update_required = false;
                        camera->update_parameters(camera_parameters);
                    }
                    flip_left_right = shared_flip_left_right;
                    flip_bottom_top = shared_flip_bottom_top;
                    if (previous_active_chunk_index != active_chunk_index) {
                        previous_active_chunk_index = active_chunk_index;
                        std::size_t event_count = 0;
                        for (std::size_t index = active_chunk_index + 1;
                             index < active_chunk_index + chunk_to_count.size();
                             ++index) {
                            event_count += chunk_to_count[index % chunk_to_count.size()];
                        }
                        parameters.insert(
                            "event_rate",
                            locale.toString(static_cast<double>(event_count) * event_rate_factor, 'f', 0) + " ev/s");
                        if (write) {
                            if (recording_stop_required) {
                                recording_stop_required = false;
                                write.reset();
                                camera->set_drop_threshold(128);
                                parameters.insert("recording_name", QVariant());
                                parameters.insert("recording_status", QVariant());
                                control_log(control_events, utc_timestamp(), "stop_recording", filename);
                                filename.clear();
                            } else {
                                parameters.insert(
                                    "recording_status",
                                    duration_and_size_to_string(
                                        previous_t - initial_t,
                                        static_cast<uint64_t>(std::filesystem::file_size(filename))));
                            }
                        } else if (recording_start_required) {
                            recording_start_required = false;
                            const auto [timestamp, stem] = utc_timestamp_and_filename();
                            filename = sepia::join({recordings_directory, stem + ".es"});
                            initial_t_set = false;
                            initial_t = 0;
                            previous_t = 0;
                            write = std::make_unique<sepia::write<sepia::type::dvs>>(
                                sepia::filename_to_ofstream(filename), sepia::evk4::width, sepia::evk4::height);
                            camera->set_drop_threshold(0);
                            parameters.insert("recording_status", "0 s (0 B)");
                            parameters.insert("recording_name", QString::fromStdString(filename));
                            control_log(control_events, timestamp, "start_recording", filename);
                        }
                    }
                    accessing_shared.clear(std::memory_order_release);
                },
                [&](std::exception_ptr exception) {
                    try {
                        std::rethrow_exception(exception);
                    } catch (const std::exception& exception) {
                        std::cerr << exception.what() << std::endl;
                    }
                    app.quit();
                },
                camera_parameters,
                serial);
            camera->set_drop_threshold(128);
            auto return_value = app.exec();
            if (return_value > 0) {
                throw std::runtime_error("qt returned a non-zero code");
            }
        });
}
