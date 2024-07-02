#include "assets.hpp"
#include "chameleon/source/background_cleaner.hpp"
#include "chameleon/source/count_display.hpp"
#include "chameleon/source/dvs_display.hpp"
#include "configuration.hpp"
#include "pontella.hpp"
#include <QQmlPropertyMap>
#include <QtGui/QFontDatabase>
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
    message << "{\"t\":\"" << timestamp << "\",\"type\":\"" << type << "\",\"payload\":" << payload << "}\n";
    control_events << message.rdbuf();
    control_events.flush();
}

#if defined(_WIN32)
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    int argc = 0;
    char** argv = nullptr;
#else
int main(int argc, char* argv[]) {
#endif
    return pontella::main(
        {"gen4_recorder displays and records events from a Gen4 camera"
         "Syntax: gen4_recorder [options]",
         "Available options:",
         "    -c [path], --configuration [path]    sets the JSON configuration path",
         "                                             defaults to \"configuration.json\"",
         "    -h, --help                           shows this help message"},
        argc,
        argv,
        0,
        {
            {"configuration", {"c"}},
        },
        {},
        [&](pontella::command command) {
            gen4::configuration configuration;
            {
                auto configuration_candidate = command.options.find("configuration");
                if (configuration_candidate == command.options.end()) {
                    const auto default_path = std::filesystem::current_path() / "configuration.json";
                    configuration = gen4::configuration::from_path(default_path);
                } else {
                    configuration = gen4::configuration::from_path(configuration_candidate->second);
                }
            }
            std::filesystem::create_directories(configuration.recordings);

            QQmlPropertyMap parameters;

            // serial
            sepia::usb::device_properties device{
                "",
                sepia::usb::device_speed::unknown,
            };
            auto is_evk4 = true;
            {
                if (configuration.serial.has_value()) {
                    device.serial = configuration.serial.value();
                    auto found = false;
                    for (const auto& available_device : sepia::evk4::available_devices()) {
                        if (available_device.serial == device.serial) {
                            device.speed = available_device.speed;
                            found = true;
                            is_evk4 = true;
                            break;
                        }
                    }
                    if (!found) {
                        for (const auto& available_device : sepia::psee413::available_devices()) {
                            if (available_device.serial == device.serial) {
                                device.speed = available_device.speed;
                                found = true;
                                is_evk4 = false;
                                break;
                            }
                        }
                    }
                    if (!found) {
                        throw sepia::usb::serial_not_available(sepia::evk4::name, device.serial);
                    }
                } else {
                    auto found = false;
                    {
                        const auto available_devices = sepia::evk4::available_devices();
                        if (!available_devices.empty()) {
                            device = available_devices.front();
                            found = true;
                            is_evk4 = true;
                        }
                    }
                    if (!found) {
                        const auto available_devices = sepia::psee413::available_devices();
                        if (!available_devices.empty()) {
                            device = available_devices.front();
                            found = true;
                            is_evk4 = false;
                        }
                    }
                    if (!found) {
                        throw sepia::no_device_connected(
                            std::string(sepia::evk4::name) + " or " + sepia::psee413::name);
                    }
                }
            }
            parameters.insert("serial", QString::fromStdString(device.serial));
            parameters.insert("speed", QString::fromStdString(sepia::usb::device_speed_to_string(device.speed)));

            // event rate, recording status, and file size
            parameters.insert("event_rate", QString("0 ev/s"));
            parameters.insert("event_rate_on", QString("0 ev/s"));
            parameters.insert("event_rate_off", QString("0 ev/s"));

            // recordings directory
            parameters.insert(
                "recordings_directory",
                configuration.recordings.empty() ? QString("./") : QString::fromStdString(configuration.recordings));
            std::ofstream control_events(
                sepia::join({configuration.recordings, device.serial + "_control_events.jsonl"}), std::ostream::app);
            parameters.insert("recording_name", QVariant());
            parameters.insert("recording_status", QVariant());

            // camera parameters
            const auto biases_names =
                is_evk4 ? sepia::evk4::bias_currents::names() : sepia::psee413::bias_currents::names();
            std::unordered_set<std::string> biases_names_set(biases_names.begin(), biases_names.end());
            {
                const auto initialisation_timestamp = utc_timestamp();
                QList<QString> qt_biases_names;
                std::transform(
                    biases_names.begin(),
                    biases_names.end(),
                    std::back_inserter(qt_biases_names),
                    [](const std::string& name) { return QString::fromStdString(name); });
                parameters.insert("biases_names", QVariant(qt_biases_names));
                for (const auto& name : biases_names) {
                    const auto value = is_evk4 ? configuration.evk4_parameters.biases.by_name(name) :
                                                 configuration.psee413_parameters.biases.by_name(name);
                    parameters.insert(QString::fromStdString(name), value);
                    control_log(
                        control_events, initialisation_timestamp, name, std::to_string(static_cast<int32_t>(value)));
                }
            }

            // user interface events
            std::atomic_flag accessing_shared;
            auto bias_update_required = false;
            auto recording_start_required = false;
            auto recording_stop_required = false;
            auto shared_flip_left_right = false;
            auto shared_flip_bottom_top = false;
            auto shared_use_count_display = false;
            uint64_t shared_tau = 100000;
            parameters.insert("use_count_display", false);
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
                    } else if (name == "use_count_display") {
                        shared_use_count_display = value.toBool();
                    } else if (name == "tau") {
                        shared_tau = value.toUInt();
                    } else {
                        const auto name_string = name.toStdString();
                        if (biases_names_set.find(name_string) == biases_names_set.end()) {
                            std::cerr << (std::string("unknown parameter \"") + name_string + "\"\n");
                            std::cerr.flush();
                        } else {
                            if (is_evk4) {
                                configuration.evk4_parameters.biases.by_name(name_string) =
                                    static_cast<uint8_t>(value.toUInt());
                            } else {
                                configuration.psee413_parameters.biases.by_name(name_string) =
                                    static_cast<uint8_t>(value.toUInt());
                            }
                            bias_update_required = true;
                            control_log(
                                control_events,
                                utc_timestamp(),
                                name_string,
                                std::to_string(static_cast<int32_t>(static_cast<uint8_t>(value.toUInt()))));
                        }
                    }
                    accessing_shared.clear(std::memory_order_release);
                });

            QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
            QGuiApplication app(argc, argv);
            qmlRegisterType<chameleon::background_cleaner>("Chameleon", 1, 0, "BackgroundCleaner");
            qmlRegisterType<chameleon::dvs_display>("Chameleon", 1, 0, "DvsDisplay");
            qmlRegisterType<chameleon::count_display>("Chameleon", 1, 0, "CountDisplay");
            QQmlApplicationEngine application_engine;
            QFontDatabase::addApplicationFontFromData(assets::opensans_regular);
            QFontDatabase::addApplicationFontFromData(assets::opensans_semibold);
            QFontDatabase::addApplicationFontFromData(assets::robotomono_regular);
            QFontDatabase::addApplicationFontFromData(assets::materialicons_regular);
            QFontDatabase font_database;
            auto base_font = font_database.font("Open Sans", "Regular", 14);
            auto title_font = font_database.font("Open Sans", "SemiBold", 14);
            auto monospace_font = font_database.font("Roboto Mono", "Regular", 14);
            auto icons_font = font_database.font("Material Icons", "Regular", 16);
            application_engine.rootContext()->setContextProperty("base_font", base_font);
            application_engine.rootContext()->setContextProperty("title_font", title_font);
            application_engine.rootContext()->setContextProperty("monospace_font", monospace_font);
            application_engine.rootContext()->setContextProperty("icons_font", icons_font);
            application_engine.rootContext()->setContextProperty("header_width", sepia::evk4::width);
            application_engine.rootContext()->setContextProperty("header_height", sepia::evk4::height);
            application_engine.rootContext()->setContextProperty("parameters", &parameters);
            application_engine.loadData(
#include "gen4_recorder.qml.hpp"
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
            auto count_display = window->findChild<chameleon::count_display*>("count_display");

            const auto event_rate_factor = 1e6 / static_cast<double>(event_rate_resolution * event_rate_chunks);

            std::string filename;
            std::string filename_timestamp;
            std::unique_ptr<sepia::write<sepia::type::dvs>> write;
            uint64_t initial_t = 0;
            uint64_t previous_t = 0;
            auto initial_t_set = false;
            auto flip_left_right = false;
            auto flip_bottom_top = false;
            std::unique_ptr<sepia::camera> camera;
            std::vector<std::pair<uint64_t, uint64_t>> chunk_to_counts(event_rate_chunks + 1, {0ull, 0ull});
            std::size_t previous_active_chunk_index = 0;
            std::size_t active_chunk_index = 0;
            uint64_t active_chunk_threshold_t = 0;
            const auto locale = QLocale(QLocale::English, QLocale::Country::Australia);
            std::size_t counts_circular_buffer_index = 0;
            auto swapped_counts = false;
            uint64_t count_t = 0;
            auto use_count_display = false;
            uint64_t tau = 0;
            std::array<std::vector<uint32_t>, 2> counts_circular_buffer;
            for (auto& counts : counts_circular_buffer) {
                counts.resize(sepia::evk4::width * sepia::evk4::height);
                std::fill(counts.begin(), counts.end(), std::numeric_limits<uint32_t>::max());
            }
            auto handle_event = [&](sepia::dvs_event event) {
                while (event.t > active_chunk_threshold_t) {
                    active_chunk_index = (active_chunk_index + 1) % chunk_to_counts.size();
                    chunk_to_counts[active_chunk_index].first = 0;
                    chunk_to_counts[active_chunk_index].second = 0;
                    active_chunk_threshold_t += event_rate_resolution;
                }
                if (event.on) {
                    ++chunk_to_counts[active_chunk_index].first;
                } else {
                    ++chunk_to_counts[active_chunk_index].second;
                }
                auto display_event = event;
                if (flip_left_right) {
                    display_event.x = sepia::evk4::width - 1 - display_event.x;
                }
                if (flip_bottom_top) {
                    display_event.y = sepia::evk4::height - 1 - display_event.y;
                }
                dvs_display->push_unsafe(display_event);
                if (use_count_display) {
                    if (display_event.t > count_t + tau) {
                        counts_circular_buffer_index =
                            (counts_circular_buffer_index + 1) % counts_circular_buffer.size();
                        swapped_counts = true;
                        count_t = display_event.t;
                        std::fill(
                            counts_circular_buffer[counts_circular_buffer_index].begin(),
                            counts_circular_buffer[counts_circular_buffer_index].end(),
                            1);
                    }
                    ++counts_circular_buffer[counts_circular_buffer_index]
                                            [display_event.x + display_event.y * sepia::evk4::width];
                }
                previous_t = event.t;
                if (write) {
                    if (!initial_t_set) {
                        initial_t_set = true;
                        initial_t = event.t;
                        std::stringstream message;
                        message << "{\"t\":\"" << utc_timestamp()
                                << "\",\"type\":\"start_recording\",\"payload\":{\"filename\":\"" << filename
                                << "\",\"initial_t\":" << initial_t << ",\"filename_timestamp\":\""
                                << filename_timestamp << "\"}}\n";
                        control_events << message.rdbuf();
                        control_events.flush();
                    }
                    event.t -= initial_t;
                    write->operator()(event);
                }
            };
            auto drop_threshold = 0;
            auto before_buffer = [&](std::size_t fifo_used, std::size_t fifo_size) {
                dvs_display->lock();
                return drop_threshold == 0 || fifo_used < drop_threshold;
            };
            auto after_buffer = [&]() {
                dvs_display->unlock();
                while (accessing_shared.test_and_set(std::memory_order_acquire)) {
                }
                if (bias_update_required) {
                    bias_update_required = false;
                    if (is_evk4) {
                        dynamic_cast<sepia::evk4::base_camera*>(camera.get())
                            ->update_parameters(configuration.evk4_parameters);
                    } else {
                        dynamic_cast<sepia::psee413::base_camera*>(camera.get())
                            ->update_parameters(configuration.psee413_parameters);
                    }
                }
                flip_left_right = shared_flip_left_right;
                flip_bottom_top = shared_flip_bottom_top;
                if (previous_active_chunk_index != active_chunk_index) {
                    previous_active_chunk_index = active_chunk_index;
                    uint64_t event_count_on = 0;
                    uint64_t event_count_off = 0;
                    for (std::size_t index = active_chunk_index + 1;
                         index < active_chunk_index + chunk_to_counts.size();
                         ++index) {
                        event_count_on += chunk_to_counts[index % chunk_to_counts.size()].first;
                        event_count_off += chunk_to_counts[index % chunk_to_counts.size()].second;
                    }
                    parameters.insert(
                        "event_rate",
                        locale.toString(
                            static_cast<double>(event_count_on + event_count_off) * event_rate_factor, 'f', 0)
                            + " ev/s");
                    parameters.insert(
                        "event_rate_on",
                        QString("ON  ")
                            + locale.toString(static_cast<double>(event_count_on) * event_rate_factor, 'f', 0)
                            + " ev/s");
                    parameters.insert(
                        "event_rate_off",
                        QString("OFF ")
                            + locale.toString(static_cast<double>(event_count_off) * event_rate_factor, 'f', 0)
                            + " ev/s");
                    if (write) {
                        if (recording_stop_required) {
                            recording_stop_required = false;
                            write.reset();
                            drop_threshold = configuration.drop_threshold;
                            parameters.insert("recording_name", QVariant());
                            parameters.insert("recording_status", QVariant());
                            control_log(
                                control_events, utc_timestamp(), "stop_recording", std::string("\"") + filename + "\"");
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
                        filename = sepia::join({configuration.recordings, stem + ".es"});
                        filename_timestamp = timestamp;
                        initial_t_set = false;
                        initial_t = previous_t;
                        write = std::make_unique<sepia::write<sepia::type::dvs>>(
                            sepia::filename_to_ofstream(filename), sepia::evk4::width, sepia::evk4::height);
                        drop_threshold = 0;
                        parameters.insert("recording_status", "0 s (0 B)");
                        parameters.insert("recording_name", QString::fromStdString(filename));
                    }
                }
                if (shared_use_count_display != use_count_display) {
                    if (!use_count_display) {
                        use_count_display = true;
                        count_t = previous_t;
                    } else {
                        use_count_display = false;
                    }
                }
                tau = shared_tau;
                if (swapped_counts) {
                    const auto index = (counts_circular_buffer_index + counts_circular_buffer.size() - 1)
                                       % counts_circular_buffer.size();
                    count_display->assign(counts_circular_buffer[index].begin(), counts_circular_buffer[index].end());
                    swapped_counts = false;
                }
                accessing_shared.clear(std::memory_order_release);
            };
            auto handle_trigger_event = [&](sepia::evk4::trigger_event event) {
                std::stringstream message;
                message << "{\"t\":\"" << utc_timestamp()
                        << "\",\"type\":\"trigger_event\",\"payload\":{\"t\":" << event.t
                        << ",\"system_timestamp\":" << event.system_timestamp
                        << ",\"id\":" << static_cast<int32_t>(event.id)
                        << ",\"rising\":" << (event.rising ? "true" : "false") << "}}\n";
                control_events << message.rdbuf();
                control_events.flush();
            };
            if (is_evk4) {
                camera = sepia::evk4::make_camera(
                    std::move(handle_event),
                    std::move(handle_trigger_event),
                    before_buffer,
                    after_buffer,
                    [&](std::exception_ptr exception) {
                        try {
                            std::rethrow_exception(exception);
                        } catch (const std::exception& exception) {
                            std::cerr << exception.what() << std::endl;
                        }
                        app.quit();
                    },
                    configuration.evk4_parameters,
                    device.serial,
                    std::chrono::milliseconds(100),
                    64,
                    configuration.fifo_size,
                    []() {
                        std::cerr << "Warning: packet dropped\n";
                        std::cerr.flush();
                    });
            } else {
                camera = sepia::psee413::make_camera(
                    std::move(handle_event),
                    std::move(handle_trigger_event),
                    before_buffer,
                    after_buffer,
                    [&](std::exception_ptr exception) {
                        try {
                            std::rethrow_exception(exception);
                        } catch (const std::exception& exception) {
                            std::cerr << exception.what() << std::endl;
                        }
                        app.quit();
                    },
                    configuration.psee413_parameters,
                    device.serial,
                    std::chrono::milliseconds(100),
                    64,
                    configuration.fifo_size,
                    []() {
                        std::cerr << "Warning: packet dropped\n";
                        std::cerr.flush();
                    });
            }
            drop_threshold = configuration.drop_threshold;
            auto return_value = app.exec();
            if (return_value > 0) {
                throw std::runtime_error("qt returned a non-zero code");
            }
        });
}
