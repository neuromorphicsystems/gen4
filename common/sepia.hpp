#pragma once

#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef SEPIA_COMPILER_WORKING_DIRECTORY
#ifdef _WIN32
std::string SEPIA_TRANSLATE(std::string filename) {
    for (auto& character : filename) {
        if (character == '/') {
            character = '\\';
        }
    }
    return filename;
}
#define SEPIA_DIRNAME                                                                                                  \
    sepia::dirname(                                                                                                    \
        std::string(__FILE__).size() > 1 && __FILE__[1] == ':' ?                                                       \
            __FILE__ :                                                                                                 \
            SEPIA_TRANSLATE(SEPIA_COMPILER_WORKING_DIRECTORY) + ("\\" __FILE__))
#else
#define SEPIA_STRINGIFY(characters) #characters
#define SEPIA_TOSTRING(characters) SEPIA_STRINGIFY(characters)
#define SEPIA_DIRNAME                                                                                                  \
    sepia::dirname(__FILE__[0] == '/' ? __FILE__ : SEPIA_TOSTRING(SEPIA_COMPILER_WORKING_DIRECTORY) "/" __FILE__)
#endif
#endif
#ifdef _WIN32
#define SEPIA_PACK(declaration) __pragma(pack(push, 1)) declaration __pragma(pack(pop))
#else
#define SEPIA_PACK(declaration) declaration __attribute__((__packed__))
#endif

/// sepia bundles functions and classes to represent a camera and handle its raw
/// stream of events.
namespace sepia {

    /// event_stream_version returns the implemented Event Stream version.
    inline std::array<uint8_t, 3> event_stream_version() {
        return {2, 0, 0};
    }

    /// event_stream_signature returns the Event Stream format signature.
    inline std::string event_stream_signature() {
        return "Event Stream";
    }

    /// type associates an Event Stream type name with its byte.
    enum class type : uint8_t {
        generic = 0,
        dvs = 1,
        atis = 2,
        color = 4,
    };

    /// make_unique creates a unique_ptr.
    template <typename T, typename... Args>
    inline std::unique_ptr<T> make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    /// false_function is a function returning false.
    inline bool false_function() {
        return false;
    }

    /// event represents the parameters of an observable event.
    template <type event_stream_type>
    struct event;

    /// generic_event represents the parameters of a generic event.
    template <>
    struct event<type::generic> {
        /// t represents the event's timestamp.
        uint64_t t;

        /// bytes stores the data payload associated with the event.
        std::vector<uint8_t> bytes;
    };
    using generic_event = event<type::generic>;

    /// dvs_event represents the parameters of a change detection.
    template <>
    SEPIA_PACK(struct event<type::dvs> {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the
        /// horizontal axis. x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the
        /// vertical axis. y is 0 on the bottom, and increases from bottom to top.
        uint16_t y;

        /// on is false if the luminance decreased.
        bool on;
    });
    using dvs_event = event<type::dvs>;

    /// atis_event represents the parameters of a change detection or an exposure
    /// measurement.
    template <>
    SEPIA_PACK(struct event<type::atis> {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the
        /// horizontal axis. x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the
        /// vertical axis. y is 0 on the bottom, and increases bottom to top.
        uint16_t y;

        /// is_threshold_crossing is false if the event is a change detection, and
        /// true if it is a threshold crossing.
        bool is_threshold_crossing;

        /// change detection: polarity is false if the light is decreasing.
        /// exposure measurement: polarity is false for a first threshold crossing.
        bool polarity;
    });
    using atis_event = event<type::atis>;

    /// color_event represents the parameters of a color event.
    template <>
    SEPIA_PACK(struct event<type::color> {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the
        /// horizontal axis. x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the
        /// vertical axis. y is 0 on the bottom, and increases bottom to top.
        uint16_t y;

        /// r represents the red component of the color.
        uint8_t r;

        /// g represents the green component of the color.
        uint8_t g;

        /// b represents the blue component of the color.
        uint8_t b;
    });
    using color_event = event<type::color>;

    /// simple_event represents the parameters of a specialized DVS event.
    SEPIA_PACK(struct simple_event {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the
        /// horizontal axis. x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the
        /// vertical axis. y is 0 on the bottom, and increases from bottom to top.
        uint16_t y;
    });

    /// threshold_crossing represents the parameters of a specialized ATIS event.
    SEPIA_PACK(struct threshold_crossing {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the
        /// horizontal axis. x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the
        /// vertical axis. y is 0 on the bottom, and increases from bottom to top.
        uint16_t y;

        /// is_second is false if the event is a first threshold crossing.
        bool is_second;
    });

    /// unreadable_file is thrown when an input file does not exist or is not
    /// readable.
    class unreadable_file : public std::runtime_error {
        public:
        unreadable_file(const std::string& filename) :
            std::runtime_error("the file '" + filename + "' could not be open for reading") {}
    };

    /// unwritable_file is thrown whenan output file is not writable.
    class unwritable_file : public std::runtime_error {
        public:
        unwritable_file(const std::string& filename) :
            std::runtime_error("the file '" + filename + "'' could not be open for writing") {}
    };

    /// wrong_signature is thrown when an input file does not have the expected
    /// signature.
    class wrong_signature : public std::runtime_error {
        public:
        wrong_signature() : std::runtime_error("the stream does not have the expected signature") {}
    };

    /// unsupported_version is thrown when an Event Stream file uses an unsupported
    /// version.
    class unsupported_version : public std::runtime_error {
        public:
        unsupported_version() : std::runtime_error("the stream uses an unsupported version") {}
    };

    /// incomplete_header is thrown when the end of file is reached while reading
    /// the header.
    class incomplete_header : public std::runtime_error {
        public:
        incomplete_header() : std::runtime_error("the stream has an incomplete header") {}
    };

    /// unsupported_event_type is thrown when an Event Stream file uses an
    /// unsupported event type.
    class unsupported_event_type : public std::runtime_error {
        public:
        unsupported_event_type() : std::runtime_error("the stream uses an unsupported event type") {}
    };

    /// coordinates_overflow is thrown when an event has coordinates outside the
    /// range provided by the header.
    class coordinates_overflow : public std::runtime_error {
        public:
        coordinates_overflow() : std::runtime_error("an event has coordinates outside the header-provided range") {}
    };

    /// end_of_file is thrown when the end of an input file is reached.
    class end_of_file : public std::runtime_error {
        public:
        end_of_file() : std::runtime_error("end of file reached") {}
    };

    /// no_device_connected is thrown when device auto-select is called without
    /// devices connected.
    class no_device_connected : public std::runtime_error {
        public:
        no_device_connected(const std::string& device_family) :
            std::runtime_error("no " + device_family + " is connected") {}
    };

    /// device_disconnected is thrown when an active device is disonnected.
    class device_disconnected : public std::runtime_error {
        public:
        device_disconnected(const std::string& device_name) : std::runtime_error(device_name + " disconnected") {}
    };

    /// parse_error is thrown when a JSON parse error occurs.
    class parse_error : public std::runtime_error {
        public:
        parse_error(const std::string& what, std::size_t character_count, std::size_t line_count) :
            std::runtime_error(
                "JSON parse error: " + what + " (line " + std::to_string(line_count) + ":"
                + std::to_string(character_count) + ")") {}
    };

    /// parameter_error is a logical error regarding a parameter.
    class parameter_error : public std::logic_error {
        public:
        parameter_error(const std::string& what) : std::logic_error(what) {}
    };

    /// dirname returns the directory part of the given path.
    inline std::string dirname(const std::string& path) {
#ifdef _WIN32
        const auto separator = '\\';
        const auto escape = '^';
#else
        const auto separator = '/';
        const auto escape = '\\';
#endif
        for (std::size_t index = path.size();;) {
            index = path.find_last_of(separator, index);
            if (index == std::string::npos) {
                return ".";
            }
            if (index == 0 || path[index - 1] != escape) {
                return path.substr(0, index);
            }
        }
    }

    /// join concatenates several path components.
    template <typename Iterator>
    inline std::string join(Iterator begin, Iterator end) {
#ifdef _WIN32
        const auto separator = '\\';
#else
        const auto separator = '/';
#endif
        std::string path;
        for (; begin != end; ++begin) {
            path += *begin;
            if (!path.empty() && begin != std::prev(end) && path.back() != separator) {
                path.push_back(separator);
            }
        }
        return path;
    }
    inline std::string join(std::initializer_list<std::string> components) {
        return join(components.begin(), components.end());
    }

    /// filename_to_ifstream creates a readable stream from a file.
    inline std::unique_ptr<std::ifstream> filename_to_ifstream(const std::string& filename) {
        auto stream = sepia::make_unique<std::ifstream>(filename, std::ifstream::in | std::ifstream::binary);
        if (!stream->good()) {
            throw unreadable_file(filename);
        }
        return stream;
    }

    /// filename_to_ofstream creates a writable stream from a file.
    inline std::unique_ptr<std::ofstream> filename_to_ofstream(const std::string& filename) {
        auto stream = sepia::make_unique<std::ofstream>(filename, std::ofstream::out | std::ofstream::binary);
        if (!stream->good()) {
            throw unwritable_file(filename);
        }
        return stream;
    }

    /// header bundles an event stream's header parameters.
    struct header {
        /// version contains the version's major, minor and patch numbers in that
        /// order.
        std::array<uint8_t, 3> version;

        /// event_stream_type is the type of the events in the associated stream.
        type event_stream_type;

        /// width is at least one more than the largest x coordinate among the
        /// stream's events.
        uint16_t width;

        /// heaight is at least one more than the largest y coordinate among the
        /// stream's events.
        uint16_t height;
    };

    /// read_header checks the header and retrieves meta-information from the given
    /// stream.
    inline header read_header(std::istream& event_stream) {
        {
            auto read_signature = event_stream_signature();
            event_stream.read(&read_signature[0], read_signature.size());
            if (event_stream.eof() || read_signature != event_stream_signature()) {
                throw wrong_signature();
            }
        }
        header header = {};
        {
            event_stream.read(reinterpret_cast<char*>(header.version.data()), header.version.size());
            if (event_stream.eof()) {
                throw incomplete_header();
            }
            if (std::get<0>(header.version) != std::get<0>(event_stream_version())
                || std::get<1>(header.version) < std::get<1>(event_stream_version())) {
                throw unsupported_version();
            }
        }
        {
            const auto type_char = static_cast<char>(event_stream.get());
            if (event_stream.eof()) {
                throw incomplete_header();
            }
            const auto type_byte = *reinterpret_cast<const uint8_t*>(&type_char);
            if (type_byte == static_cast<uint8_t>(type::generic)) {
                header.event_stream_type = type::generic;
            } else if (type_byte == static_cast<uint8_t>(type::dvs)) {
                header.event_stream_type = type::dvs;
            } else if (type_byte == static_cast<uint8_t>(type::atis)) {
                header.event_stream_type = type::atis;
            } else if (type_byte == static_cast<uint8_t>(type::color)) {
                header.event_stream_type = type::color;
            } else {
                throw unsupported_event_type();
            }
        }
        if (header.event_stream_type != type::generic) {
            std::array<uint8_t, 4> size_bytes;
            event_stream.read(reinterpret_cast<char*>(size_bytes.data()), size_bytes.size());
            if (event_stream.eof()) {
                throw incomplete_header();
            }
            header.width = static_cast<uint16_t>(
                (static_cast<uint16_t>(std::get<0>(size_bytes))
                 | (static_cast<uint16_t>(std::get<1>(size_bytes)) << 8)));
            header.height = static_cast<uint16_t>(
                (static_cast<uint16_t>(std::get<2>(size_bytes))
                 | (static_cast<uint16_t>(std::get<3>(size_bytes)) << 8)));
        }
        return header;
    }

    /// read_header checks the header and retrieves meta-information from the given
    /// stream.
    inline header read_header(std::unique_ptr<std::istream> event_stream) {
        return read_header(*event_stream);
    }

    /// write_header writes the header bytes to a byte stream.
    template <type event_stream_type>
    inline void write_header(std::ostream& event_stream, uint16_t width, uint16_t height) {
        event_stream.write(event_stream_signature().data(), event_stream_signature().size());
        event_stream.write(reinterpret_cast<char*>(event_stream_version().data()), event_stream_version().size());
        std::array<uint8_t, 5> bytes{
            static_cast<uint8_t>(event_stream_type),
            static_cast<uint8_t>(width & 0b11111111),
            static_cast<uint8_t>((width & 0b1111111100000000) >> 8),
            static_cast<uint8_t>(height & 0b11111111),
            static_cast<uint8_t>((height & 0b1111111100000000) >> 8),
        };
        event_stream.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
    }
    template <type event_stream_type, typename = typename std::enable_if<event_stream_type == type::generic>>
    inline void write_header(std::ostream& event_stream) {
        event_stream.write(event_stream_signature().data(), event_stream_signature().size());
        event_stream.write(reinterpret_cast<char*>(event_stream_version().data()), event_stream_version().size());
        auto type_byte = static_cast<uint8_t>(event_stream_type);
        event_stream.put(*reinterpret_cast<char*>(&type_byte));
    }
    template <>
    inline void write_header<type::generic>(std::ostream& event_stream, uint16_t, uint16_t) {
        write_header<type::generic>(event_stream);
    }

    /// split separates a stream of DVS or ATIS events into specialized streams.
    template <type event_stream_type, typename HandleFirstSpecializedEvent, typename HandleSecondSpecializedEvent>
    class split;

    /// split separates a stream of DVS events into two streams of simple events.
    template <typename HandleIncreaseEvent, typename HandleDecreaseEvent>
    class split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent> {
        public:
        split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>(
            HandleIncreaseEvent&& handle_increase_event,
            HandleDecreaseEvent&& handle_decrease_event) :
            _handle_increase_event(std::forward<HandleIncreaseEvent>(handle_increase_event)),
            _handle_decrease_event(std::forward<HandleDecreaseEvent>(handle_decrease_event)) {}
        split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>(
            const split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>&) = delete;
        split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>(
            split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>&&) = default;
        split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>&
        operator=(const split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>&) = delete;
        split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>&
        operator=(split<type::dvs, HandleIncreaseEvent, HandleDecreaseEvent>&&) = default;
        virtual ~split() {}

        /// operator() handles an event.
        virtual void operator()(dvs_event dvs_event) {
            if (dvs_event.on) {
                _handle_increase_event(simple_event{dvs_event.t, dvs_event.x, dvs_event.y});
            } else {
                _handle_decrease_event(simple_event{dvs_event.t, dvs_event.x, dvs_event.y});
            }
        }

        protected:
        HandleIncreaseEvent _handle_increase_event;
        HandleDecreaseEvent _handle_decrease_event;
    };

    /// split separates a stream of ATIS events into a stream of DVS events and a
    /// stream of theshold crossings.
    template <typename HandleDvsEvent, typename HandleThresholdCrossing>
    class split<type::atis, HandleDvsEvent, HandleThresholdCrossing> {
        public:
        split<type::atis, HandleDvsEvent, HandleThresholdCrossing>(
            HandleDvsEvent&& handle_dvs_event,
            HandleThresholdCrossing&& handle_threshold_crossing) :
            _handle_dvs_event(std::forward<HandleDvsEvent>(handle_dvs_event)),
            _handle_threshold_crossing(std::forward<HandleThresholdCrossing>(handle_threshold_crossing)) {}
        split<type::atis, HandleDvsEvent, HandleThresholdCrossing>(
            const split<type::atis, HandleDvsEvent, HandleThresholdCrossing>&) = delete;
        split<type::atis, HandleDvsEvent, HandleThresholdCrossing>(
            split<type::atis, HandleDvsEvent, HandleThresholdCrossing>&&) = default;
        split<type::atis, HandleDvsEvent, HandleThresholdCrossing>&
        operator=(const split<type::atis, HandleDvsEvent, HandleThresholdCrossing>&) = delete;
        split<type::atis, HandleDvsEvent, HandleThresholdCrossing>&
        operator=(split<type::atis, HandleDvsEvent, HandleThresholdCrossing>&&) = default;
        virtual ~split<type::atis, HandleDvsEvent, HandleThresholdCrossing>() {}

        /// operator() handles an event.
        virtual void operator()(atis_event current_atis_event) {
            if (current_atis_event.is_threshold_crossing) {
                _handle_threshold_crossing(threshold_crossing{
                    current_atis_event.t, current_atis_event.x, current_atis_event.y, current_atis_event.polarity});
            } else {
                _handle_dvs_event(dvs_event{
                    current_atis_event.t, current_atis_event.x, current_atis_event.y, current_atis_event.polarity});
            }
        }

        protected:
        HandleDvsEvent _handle_dvs_event;
        HandleThresholdCrossing _handle_threshold_crossing;
    };

    /// make_split creates a split from functors.
    template <type event_stream_type, typename HandleFirstSpecializedEvent, typename HandleSecondSpecializedEvent>
    inline split<event_stream_type, HandleFirstSpecializedEvent, HandleSecondSpecializedEvent> make_split(
        HandleFirstSpecializedEvent&& handle_first_specialized_event,
        HandleSecondSpecializedEvent&& handle_second_specialized_event) {
        return split<event_stream_type, HandleFirstSpecializedEvent, HandleSecondSpecializedEvent>(
            std::forward<HandleFirstSpecializedEvent>(handle_first_specialized_event),
            std::forward<HandleSecondSpecializedEvent>(handle_second_specialized_event));
    }

    /// handle_byte implements an event stream state machine.
    template <type event_stream_type>
    class handle_byte;

    /// handle_byte<type::generic> implements the event stream state machine for
    /// generic events.
    template <>
    class handle_byte<type::generic> {
        public:
        handle_byte() : _state(state::idle), _index(0), _bytes_size(0) {}
        handle_byte(uint16_t, uint16_t) : handle_byte() {}
        handle_byte(const handle_byte&) = default;
        handle_byte(handle_byte&&) = default;
        handle_byte& operator=(const handle_byte&) = default;
        handle_byte& operator=(handle_byte&&) = default;
        virtual ~handle_byte() {}

        /// operator() handles a byte.
        virtual bool operator()(uint8_t byte, generic_event& generic_event) {
            switch (_state) {
                case state::idle:
                    if (byte == 0b11111111) {
                        generic_event.t += 0b11111110;
                    } else if (byte != 0b11111110) {
                        generic_event.t += byte;
                        _state = state::byte0;
                    }
                    break;
                case state::byte0:
                    _bytes_size |= ((byte >> 1) << (7 * _index));
                    if ((byte & 1) == 0) {
                        generic_event.bytes.clear();
                        _index = 0;
                        if (_bytes_size == 0) {
                            _state = state::idle;
                            return true;
                        }
                        generic_event.bytes.reserve(_bytes_size);
                        _state = state::size_byte;
                    } else {
                        ++_index;
                    }
                    break;
                case state::size_byte:
                    generic_event.bytes.push_back(byte);
                    if (generic_event.bytes.size() == _bytes_size) {
                        _state = state::idle;
                        _index = 0;
                        _bytes_size = 0;
                        return true;
                    }
                    break;
            }
            return false;
        }

        /// reset initializes the state machine.
        virtual void reset() {
            _state = state::idle;
            _index = 0;
            _bytes_size = 0;
        }

        protected:
        /// state represents the current state machine's state.
        enum class state {
            idle,
            byte0,
            size_byte,
        };

        state _state;
        std::size_t _index;
        std::size_t _bytes_size;
    };

    /// handle_byte<type::dvs> implements the event stream state machine for DVS
    /// events.
    template <>
    class handle_byte<type::dvs> {
        public:
        handle_byte(uint16_t width, uint16_t height) : _width(width), _height(height), _state(state::idle) {}
        handle_byte(const handle_byte&) = default;
        handle_byte(handle_byte&&) = default;
        handle_byte& operator=(const handle_byte&) = delete;
        handle_byte& operator=(handle_byte&&) = delete;
        virtual ~handle_byte() {}

        /// operator() handles a byte.
        virtual bool operator()(uint8_t byte, dvs_event& dvs_event) {
            switch (_state) {
                case state::idle:
                    if (byte == 0b11111111) {
                        dvs_event.t += 0b1111111;
                    } else if (byte != 0b11111110) {
                        dvs_event.t += (byte >> 1);
                        dvs_event.on = ((byte & 1) == 1);
                        _state = state::byte0;
                    }
                    break;
                case state::byte0:
                    dvs_event.x = byte;
                    _state = state::byte1;
                    break;
                case state::byte1:
                    dvs_event.x |= (byte << 8);
                    if (dvs_event.x >= _width) {
                        throw coordinates_overflow();
                    }
                    _state = state::byte2;
                    break;
                case state::byte2:
                    dvs_event.y = byte;
                    _state = state::byte3;
                    break;
                case state::byte3:
                    dvs_event.y |= (byte << 8);
                    if (dvs_event.y >= _height) {
                        throw coordinates_overflow();
                    }
                    _state = state::idle;
                    return true;
            }
            return false;
        }

        /// reset initializes the state machine.
        virtual void reset() {
            _state = state::idle;
        }

        protected:
        /// state represents the current state machine's state.
        enum class state {
            idle,
            byte0,
            byte1,
            byte2,
            byte3,
        };

        const uint16_t _width;
        const uint16_t _height;
        state _state;
    };

    /// handle_byte<type::atis> implements the event stream state machine for ATIS
    /// events.
    template <>
    class handle_byte<type::atis> {
        public:
        handle_byte(uint16_t width, uint16_t height) : _width(width), _height(height), _state(state::idle) {}
        handle_byte(const handle_byte&) = default;
        handle_byte(handle_byte&&) = default;
        handle_byte& operator=(const handle_byte&) = delete;
        handle_byte& operator=(handle_byte&&) = delete;
        virtual ~handle_byte() {}

        /// operator() handles a byte.
        virtual bool operator()(uint8_t byte, atis_event& atis_event) {
            switch (_state) {
                case state::idle:
                    if ((byte & 0b11111100) == 0b11111100) {
                        atis_event.t += static_cast<uint64_t>(0b111111) * (byte & 0b11);
                    } else {
                        atis_event.t += (byte >> 2);
                        atis_event.is_threshold_crossing = ((byte & 1) == 1);
                        atis_event.polarity = ((byte & 0b10) == 0b10);
                        _state = state::byte0;
                    }
                    break;
                case state::byte0:
                    atis_event.x = byte;
                    _state = state::byte1;
                    break;
                case state::byte1:
                    atis_event.x |= (byte << 8);
                    if (atis_event.x >= _width) {
                        throw coordinates_overflow();
                    }
                    _state = state::byte2;
                    break;
                case state::byte2:
                    atis_event.y = byte;
                    _state = state::byte3;
                    break;
                case state::byte3:
                    atis_event.y |= (byte << 8);
                    if (atis_event.y >= _height) {
                        throw coordinates_overflow();
                    }
                    _state = state::idle;
                    return true;
            }
            return false;
        }

        /// reset initializes the state machine.
        virtual void reset() {
            _state = state::idle;
        }

        protected:
        /// state represents the current state machine's state.
        enum class state {
            idle,
            byte0,
            byte1,
            byte2,
            byte3,
        };

        const uint16_t _width;
        const uint16_t _height;
        state _state;
    };

    /// handle_byte<type::color> implements the event stream state machine for color
    /// events.
    template <>
    class handle_byte<type::color> {
        public:
        handle_byte(uint16_t width, uint16_t height) : _width(width), _height(height), _state(state::idle) {}
        handle_byte(const handle_byte&) = default;
        handle_byte(handle_byte&&) = default;
        handle_byte& operator=(const handle_byte&) = delete;
        handle_byte& operator=(handle_byte&&) = delete;
        virtual ~handle_byte() {}

        /// operator() handles a byte.
        virtual bool operator()(uint8_t byte, color_event& color_event) {
            switch (_state) {
                case state::idle: {
                    if (byte == 0b11111111) {
                        color_event.t += 0b11111110;
                    } else if (byte != 0b11111110) {
                        color_event.t += byte;
                        _state = state::byte0;
                    }
                    break;
                }
                case state::byte0: {
                    color_event.x = byte;
                    _state = state::byte1;
                    break;
                }
                case state::byte1: {
                    color_event.x |= (byte << 8);
                    if (color_event.x >= _width) {
                        throw coordinates_overflow();
                    }
                    _state = state::byte2;
                    break;
                }
                case state::byte2: {
                    color_event.y = byte;
                    _state = state::byte3;
                    break;
                }
                case state::byte3: {
                    color_event.y |= (byte << 8);
                    if (color_event.y >= _height) {
                        throw coordinates_overflow();
                    }
                    _state = state::byte4;
                    break;
                }
                case state::byte4: {
                    color_event.r = byte;
                    _state = state::byte5;
                    break;
                }
                case state::byte5: {
                    color_event.g = byte;
                    _state = state::byte6;
                    break;
                }
                case state::byte6: {
                    color_event.b = byte;
                    _state = state::idle;
                    return true;
                }
            }
            return false;
        }

        /// reset initializes the state machine.
        virtual void reset() {
            _state = state::idle;
        }

        protected:
        /// state represents the current state machine's state.
        enum class state {
            idle,
            byte0,
            byte1,
            byte2,
            byte3,
            byte4,
            byte5,
            byte6,
        };

        const uint16_t _width;
        const uint16_t _height;
        state _state;
    };

    /// write_to_reference converts and writes events to a non-owned byte stream.
    template <type event_stream_type>
    class write_to_reference;

    /// write_to_reference<type::generic> converts and writes generic events to a
    /// non-owned byte stream.
    template <>
    class write_to_reference<type::generic> {
        public:
        write_to_reference(std::ostream& event_stream) : _event_stream(event_stream), _previous_t(0) {
            write_header<type::generic>(_event_stream);
        }
        write_to_reference(std::ostream& event_stream, uint16_t, uint16_t) : write_to_reference(event_stream) {}
        write_to_reference(const write_to_reference&) = delete;
        write_to_reference(write_to_reference&&) = default;
        write_to_reference& operator=(const write_to_reference&) = delete;
        write_to_reference& operator=(write_to_reference&&) = delete;
        virtual ~write_to_reference() {}

        /// operator() handles an event.
        virtual void operator()(generic_event current_generic_event) {
            if (current_generic_event.t < _previous_t) {
                throw std::logic_error("the event's timestamp is smaller than the previous one's");
            }
            auto relative_t = current_generic_event.t - _previous_t;
            if (relative_t >= 0b11111110) {
                const auto number_of_overflows = relative_t / 0b11111110;
                for (std::size_t index = 0; index < number_of_overflows; ++index) {
                    _event_stream.put(static_cast<uint8_t>(0b11111111));
                }
                relative_t -= number_of_overflows * 0b11111110;
            }
            _event_stream.put(static_cast<uint8_t>(relative_t));
            for (std::size_t size = current_generic_event.bytes.size(); size > 0; size >>= 7) {
                _event_stream.put(static_cast<uint8_t>((size & 0b1111111) << 1) | ((size >> 7) > 0 ? 1 : 0));
            }
            _event_stream.write(
                reinterpret_cast<const char*>(current_generic_event.bytes.data()), current_generic_event.bytes.size());
            _previous_t = current_generic_event.t;
        }

        protected:
        std::ostream& _event_stream;
        uint64_t _previous_t;
    };

    /// write_to_reference<type::dvs> converts and writes DVS events to a non-owned
    /// byte stream.
    template <>
    class write_to_reference<type::dvs> {
        public:
        write_to_reference(std::ostream& event_stream, uint16_t width, uint16_t height) :
            _event_stream(event_stream), _width(width), _height(height), _previous_t(0) {
            write_header<type::dvs>(_event_stream, width, height);
        }
        write_to_reference(const write_to_reference&) = delete;
        write_to_reference(write_to_reference&&) = default;
        write_to_reference& operator=(const write_to_reference&) = delete;
        write_to_reference& operator=(write_to_reference&&) = delete;
        virtual ~write_to_reference() {}

        /// operator() handles an event.
        virtual void operator()(dvs_event current_dvs_event) {
            if (current_dvs_event.x >= _width || current_dvs_event.y >= _height) {
                throw coordinates_overflow();
            }
            if (current_dvs_event.t < _previous_t) {
                throw std::logic_error("the event's timestamp is smaller than the previous one's");
            }
            auto relative_t = current_dvs_event.t - _previous_t;
            if (relative_t >= 0b1111111) {
                const auto number_of_overflows = relative_t / 0b1111111;
                for (std::size_t index = 0; index < number_of_overflows; ++index) {
                    _event_stream.put(static_cast<uint8_t>(0b11111111));
                }
                relative_t -= number_of_overflows * 0b1111111;
            }
            std::array<uint8_t, 5> bytes{
                static_cast<uint8_t>((relative_t << 1) | (current_dvs_event.on ? 1 : 0)),
                static_cast<uint8_t>(current_dvs_event.x & 0b11111111),
                static_cast<uint8_t>((current_dvs_event.x & 0b1111111100000000) >> 8),
                static_cast<uint8_t>(current_dvs_event.y & 0b11111111),
                static_cast<uint8_t>((current_dvs_event.y & 0b1111111100000000) >> 8),
            };
            _event_stream.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
            _previous_t = current_dvs_event.t;
        }

        protected:
        std::ostream& _event_stream;
        const uint16_t _width;
        const uint16_t _height;
        uint64_t _previous_t;
    };

    /// write_to_reference<type::atis> converts and writes ATIS events to a
    /// non-owned byte stream.
    template <>
    class write_to_reference<type::atis> {
        public:
        write_to_reference(std::ostream& event_stream, uint16_t width, uint16_t height) :
            _event_stream(event_stream), _width(width), _height(height), _previous_t(0) {
            write_header<type::atis>(_event_stream, width, height);
        }
        write_to_reference(const write_to_reference&) = delete;
        write_to_reference(write_to_reference&&) = default;
        write_to_reference& operator=(const write_to_reference&) = delete;
        write_to_reference& operator=(write_to_reference&&) = delete;
        virtual ~write_to_reference() {}

        /// operator() handles an event.
        virtual void operator()(atis_event current_atis_event) {
            if (current_atis_event.x >= _width || current_atis_event.y >= _height) {
                throw coordinates_overflow();
            }
            if (current_atis_event.t < _previous_t) {
                throw std::logic_error("the event's timestamp is smaller than the previous one's");
            }
            auto relative_t = current_atis_event.t - _previous_t;
            if (relative_t >= 0b111111) {
                const auto number_of_overflows = relative_t / 0b111111;
                for (std::size_t index = 0; index < number_of_overflows / 0b11; ++index) {
                    _event_stream.put(static_cast<uint8_t>(0b11111111));
                }
                const auto number_of_overflows_left = number_of_overflows % 0b11;
                if (number_of_overflows_left > 0) {
                    _event_stream.put(static_cast<uint8_t>(0b11111100 | number_of_overflows_left));
                }
                relative_t -= number_of_overflows * 0b111111;
            }
            std::array<uint8_t, 5> bytes{
                static_cast<uint8_t>(
                    (relative_t << 2) | (current_atis_event.polarity ? 0b10 : 0b00)
                    | (current_atis_event.is_threshold_crossing ? 1 : 0)),
                static_cast<uint8_t>(current_atis_event.x & 0b11111111),
                static_cast<uint8_t>((current_atis_event.x & 0b1111111100000000) >> 8),
                static_cast<uint8_t>(current_atis_event.y & 0b11111111),
                static_cast<uint8_t>((current_atis_event.y & 0b1111111100000000) >> 8),
            };
            _event_stream.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
            _previous_t = current_atis_event.t;
        }

        protected:
        std::ostream& _event_stream;
        const uint16_t _width;
        const uint16_t _height;
        uint64_t _previous_t;
    };

    /// write_to_reference<type::color> converts and writes color events to a
    /// non-owned byte stream.
    template <>
    class write_to_reference<type::color> {
        public:
        write_to_reference(std::ostream& event_stream, uint16_t width, uint16_t height) :
            _event_stream(event_stream), _width(width), _height(height), _previous_t(0) {
            write_header<type::color>(_event_stream, width, height);
        }
        write_to_reference(const write_to_reference&) = delete;
        write_to_reference(write_to_reference&&) = default;
        write_to_reference& operator=(const write_to_reference&) = delete;
        write_to_reference& operator=(write_to_reference&&) = delete;
        virtual ~write_to_reference() {}

        /// operator() handles an event.
        virtual void operator()(color_event current_color_event) {
            if (current_color_event.x >= _width || current_color_event.y >= _height) {
                throw coordinates_overflow();
            }
            if (current_color_event.t < _previous_t) {
                throw std::logic_error("the event's timestamp is smaller than the previous one's");
            }
            auto relative_t = current_color_event.t - _previous_t;
            if (relative_t >= 0b11111110) {
                const auto number_of_overflows = relative_t / 0b11111110;
                for (std::size_t index = 0; index < number_of_overflows; ++index) {
                    _event_stream.put(static_cast<uint8_t>(0b11111111));
                }
                relative_t -= number_of_overflows * 0b11111110;
            }
            std::array<uint8_t, 8> bytes{
                static_cast<uint8_t>(relative_t),
                static_cast<uint8_t>(current_color_event.x & 0b11111111),
                static_cast<uint8_t>((current_color_event.x & 0b1111111100000000) >> 8),
                static_cast<uint8_t>(current_color_event.y & 0b11111111),
                static_cast<uint8_t>((current_color_event.y & 0b1111111100000000) >> 8),
                static_cast<uint8_t>(current_color_event.r),
                static_cast<uint8_t>(current_color_event.g),
                static_cast<uint8_t>(current_color_event.b),
            };
            _event_stream.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
            _previous_t = current_color_event.t;
        }

        protected:
        std::ostream& _event_stream;
        const uint16_t _width;
        const uint16_t _height;
        uint64_t _previous_t;
    };

    /// write converts and writes events to a byte stream.
    template <type event_stream_type>
    class write {
        public:
        template <type generic_type = type::generic>
        write(
            std::unique_ptr<std::ostream> event_stream,
            typename std::enable_if<event_stream_type == generic_type>::type* = nullptr) :
            write(std::move(event_stream), 0, 0) {}
        write(std::unique_ptr<std::ostream> event_stream, uint16_t width, uint16_t height) :
            _event_stream(std::move(event_stream)), _write_to_reference(*_event_stream, width, height) {}
        write(const write&) = delete;
        write(write&&) = default;
        write& operator=(const write&) = delete;
        write& operator=(write&&) = default;
        virtual ~write() {}

        /// operator() handles an event.
        virtual void operator()(event<event_stream_type> event) {
            _write_to_reference(event);
        }

        protected:
        std::unique_ptr<std::ostream> _event_stream;
        write_to_reference<event_stream_type> _write_to_reference;
    };

    /// dispatch specifies when the events are dispatched by an observable.
    enum class dispatch {
        synchronously_but_skip_offset,
        synchronously,
        as_fast_as_possible,
    };

    /// observable reads bytes from a stream and dispatches events.
    template <type event_stream_type, typename HandleEvent, typename HandleException, typename MustRestart>
    class observable {
        public:
        observable(
            std::unique_ptr<std::istream> event_stream,
            HandleEvent&& handle_event,
            HandleException&& handle_exception,
            MustRestart&& must_restart,
            dispatch dispatch_events,
            std::size_t chunk_size) :
            _event_stream(std::move(event_stream)),
            _handle_event(std::forward<HandleEvent>(handle_event)),
            _handle_exception(std::forward<HandleException>(handle_exception)),
            _must_restart(std::forward<MustRestart>(must_restart)),
            _dispatch_events(dispatch_events),
            _chunk_size(chunk_size),
            _running(true) {
            const auto header = read_header(*_event_stream);
            if (header.event_stream_type != event_stream_type) {
                throw unsupported_event_type();
            }
            _loop = std::thread([this, header]() {
                try {
                    event<event_stream_type> event = {};
                    handle_byte<event_stream_type> handle_byte(header.width, header.height);
                    std::vector<uint8_t> bytes(_chunk_size);
                    switch (_dispatch_events) {
                        case dispatch::synchronously_but_skip_offset: {
                            auto offset_skipped = false;
                            auto time_reference = std::chrono::system_clock::now();
                            uint64_t initial_t = 0;
                            uint64_t previous_t = 0;
                            while (_running.load(std::memory_order_relaxed)) {
                                _event_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                if (_event_stream->eof()) {
                                    for (auto byte_iterator = bytes.begin();
                                         byte_iterator
                                         != std::next(
                                             bytes.begin(),
                                             static_cast<
                                                 std::iterator_traits<std::vector<uint8_t>::iterator>::difference_type>(
                                                 _event_stream->gcount()));
                                         ++byte_iterator) {
                                        if (handle_byte(*byte_iterator, event)) {
                                            if (offset_skipped) {
                                                if (event.t > previous_t) {
                                                    previous_t = event.t;
                                                    std::this_thread::sleep_until(
                                                        time_reference
                                                        + std::chrono::microseconds(event.t - initial_t));
                                                }
                                            } else {
                                                offset_skipped = true;
                                                initial_t = event.t;
                                                previous_t = event.t;
                                            }
                                            _handle_event(event);
                                        }
                                    }
                                    if (_must_restart()) {
                                        _event_stream->clear();
                                        _event_stream->seekg(0, std::istream::beg);
                                        read_header(*_event_stream);
                                        offset_skipped = false;
                                        handle_byte.reset();
                                        event = {};
                                        time_reference = std::chrono::system_clock::now();
                                        continue;
                                    }
                                    throw end_of_file();
                                }
                                for (auto byte : bytes) {
                                    if (handle_byte(byte, event)) {
                                        if (offset_skipped) {
                                            if (event.t > previous_t) {
                                                previous_t = event.t;
                                                std::this_thread::sleep_until(
                                                    time_reference + std::chrono::microseconds(event.t - initial_t));
                                            }
                                        } else {
                                            offset_skipped = true;
                                            initial_t = event.t;
                                            previous_t = event.t;
                                        }
                                        _handle_event(event);
                                    }
                                }
                            }
                            break;
                        }
                        case dispatch::synchronously: {
                            auto time_reference = std::chrono::system_clock::now();
                            uint64_t previous_t = 0;
                            while (_running.load(std::memory_order_relaxed)) {
                                _event_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                if (_event_stream->eof()) {
                                    for (auto byte_iterator = bytes.begin();
                                         byte_iterator
                                         != std::next(
                                             bytes.begin(),
                                             static_cast<
                                                 std::iterator_traits<std::vector<uint8_t>::iterator>::difference_type>(
                                                 _event_stream->gcount()));
                                         ++byte_iterator) {
                                        if (handle_byte(*byte_iterator, event)) {
                                            if (event.t > previous_t) {
                                                std::this_thread::sleep_until(
                                                    time_reference + std::chrono::microseconds(event.t));
                                            }
                                            previous_t = event.t;
                                            _handle_event(event);
                                        }
                                    }
                                    if (_must_restart()) {
                                        _event_stream->clear();
                                        _event_stream->seekg(0, std::istream::beg);
                                        read_header(*_event_stream);
                                        handle_byte.reset();
                                        event = {};
                                        time_reference = std::chrono::system_clock::now();
                                        continue;
                                    }
                                    throw end_of_file();
                                }
                                for (auto byte : bytes) {
                                    if (handle_byte(byte, event)) {
                                        if (event.t > previous_t) {
                                            std::this_thread::sleep_until(
                                                time_reference + std::chrono::microseconds(event.t));
                                        }
                                        previous_t = event.t;
                                        _handle_event(event);
                                    }
                                }
                            }
                            break;
                        }
                        case dispatch::as_fast_as_possible: {
                            while (_running.load(std::memory_order_relaxed)) {
                                _event_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                if (_event_stream->eof()) {
                                    for (auto byte_iterator = bytes.begin();
                                         byte_iterator
                                         != std::next(
                                             bytes.begin(),
                                             static_cast<
                                                 std::iterator_traits<std::vector<uint8_t>::iterator>::difference_type>(
                                                 _event_stream->gcount()));
                                         ++byte_iterator) {
                                        if (handle_byte(*byte_iterator, event)) {
                                            _handle_event(event);
                                        }
                                    }
                                    if (_must_restart()) {
                                        _event_stream->clear();
                                        _event_stream->seekg(0, std::istream::beg);
                                        read_header(*_event_stream);
                                        handle_byte.reset();
                                        event = {};
                                        continue;
                                    }
                                    throw end_of_file();
                                }
                                for (auto byte : bytes) {
                                    if (handle_byte(byte, event)) {
                                        _handle_event(event);
                                    }
                                }
                            }
                            break;
                        }
                    }
                } catch (...) {
                    _handle_exception(std::current_exception());
                }
            });
        }
        observable(const observable&) = delete;
        observable(observable&&) = default;
        observable& operator=(const observable&) = delete;
        observable& operator=(observable&&) = default;
        virtual ~observable() {
            _running.store(false, std::memory_order_relaxed);
            _loop.join();
        }

        protected:
        std::unique_ptr<std::istream> _event_stream;
        HandleEvent _handle_event;
        HandleException _handle_exception;
        MustRestart _must_restart;
        dispatch _dispatch_events;
        std::size_t _chunk_size;
        std::atomic_bool _running;
        std::thread _loop;
    };

    /// make_observable creates an event stream observable from functors.
    template <
        type event_stream_type,
        typename HandleEvent,
        typename HandleException,
        typename MustRestart = decltype(&false_function)>
    inline std::unique_ptr<observable<event_stream_type, HandleEvent, HandleException, MustRestart>> make_observable(
        std::unique_ptr<std::istream> event_stream,
        HandleEvent&& handle_event,
        HandleException&& handle_exception,
        MustRestart&& must_restart = &false_function,
        dispatch dispatch_events = dispatch::synchronously_but_skip_offset,
        std::size_t chunk_size = 1 << 10) {
        return sepia::make_unique<observable<event_stream_type, HandleEvent, HandleException, MustRestart>>(
            std::move(event_stream),
            std::forward<HandleEvent>(handle_event),
            std::forward<HandleException>(handle_exception),
            std::forward<MustRestart>(must_restart),
            dispatch_events,
            chunk_size);
    }

    /// capture_exception stores an exception pointer and notifies a condition
    /// variable.
    class capture_exception {
        public:
        capture_exception() = default;
        capture_exception(const capture_exception&) = delete;
        capture_exception(capture_exception&&) = delete;
        capture_exception& operator=(const capture_exception&) = delete;
        capture_exception& operator=(capture_exception&&) = delete;
        virtual ~capture_exception() {}

        /// operator() handles an exception.
        virtual void operator()(std::exception_ptr exception) {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _exception = exception;
            }
            _condition_variable.notify_one();
        }

        /// wait blocks until the held exception is set.
        virtual void wait() {
            std::unique_lock<std::mutex> exception_lock(_mutex);
            if (_exception == nullptr) {
                _condition_variable.wait(exception_lock, [&] { return _exception != nullptr; });
            }
        }

        /// rethrow_unless raises the internally held exception unless it matches one
        /// of the given types.
        template <typename... Exceptions>
        void rethrow_unless() {
            catch_index<0, Exceptions...>();
        }

        protected:
        /// catch_index catches the n-th exception type.
        template <std::size_t index, typename... Exceptions>
            typename std::enable_if < index<sizeof...(Exceptions), void>::type catch_index() {
            using Exception = typename std::tuple_element<index, std::tuple<Exceptions...>>::type;
            try {
                catch_index<index + 1, Exceptions...>();
            } catch (const Exception&) {
                return;
            }
        }

        /// catch_index is a termination for the template loop.
        template <std::size_t index, typename... Exceptions>
        typename std::enable_if<index == sizeof...(Exceptions), void>::type catch_index() {
            std::rethrow_exception(_exception);
        }

        std::mutex _mutex;
        std::condition_variable _condition_variable;
        std::exception_ptr _exception;
    };

    /// join_observable creates an event stream observable from functors and blocks
    /// until the end of the input stream is reached.
    template <type event_stream_type, typename HandleEvent>
    inline void join_observable(
        std::unique_ptr<std::istream> event_stream,
        HandleEvent&& handle_event,
        std::size_t chunk_size = 1 << 10) {
        capture_exception capture_observable_exception;
        auto observable = make_observable<event_stream_type>(
            std::move(event_stream),
            std::forward<HandleEvent>(handle_event),
            std::ref(capture_observable_exception),
            &false_function,
            dispatch::as_fast_as_possible,
            chunk_size);
        capture_observable_exception.wait();
        capture_observable_exception.rethrow_unless<end_of_file>();
    };
}
