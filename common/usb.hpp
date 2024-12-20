#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include "libusb/libusb.h"
#else
#include <libusb-1.0/libusb.h>
#endif

namespace sepia {
    namespace usb {
        /// device_busy is thrown on failed interface capture.
        class device_busy : public std::runtime_error {
            public:
            device_busy() : std::runtime_error("the device is busy") {}
        };

        /// no_device_available is thrown if no device with the given PID/VID is
        /// available.
        class no_device_available : public std::runtime_error {
            public:
            no_device_available(const std::string& device_name) :
                std::runtime_error("no " + device_name + " is available") {}
        };

        /// serial_not_found is thrown if the device with the given serial is not
        /// available.
        class serial_not_available : public std::runtime_error {
            public:
            serial_not_available(const std::string& device_name, const std::string& serial) :
                std::runtime_error("the " + device_name + " with serial '" + serial + "' is not available") {}
        };

        /// device_disconnected is thrown when the communication with a device is
        /// broken.
        class device_disconnected : public std::runtime_error {
            public:
            device_disconnected(const std::string& device_name) : std::runtime_error(device_name + " disconnected") {}
        };

        /// error is thrown when a USB transfer fails.
        class error : public std::runtime_error {
            public:
            error(const std::string& message) : std::runtime_error(message) {}
        };

        /// throw_on_error throws if the given value is not zero.
        static void throw_on_error(const std::string& message, int error_code) {
            if (error_code < 0) {
                throw error(message + " failed: " + libusb_strerror(static_cast<libusb_error>(error_code)));
            }
        }

        /// make_context creates a managed libusb_context.
        inline std::shared_ptr<libusb_context> make_context() {
            libusb_context* context;
            throw_on_error("initializing the USB context", libusb_init(&context));
            return std::shared_ptr<libusb_context>(context, [](libusb_context* context) { libusb_exit(context); });
        }

        /// device_speed lists known USB speeds.
        enum class device_speed {
            unknown,
            low,
            full,
            high,
            super,
            super_plus,
        };

        std::string device_speed_to_string(device_speed speed) {
            switch (speed) {
                case device_speed::unknown:
                    return "USB Unknown speed";
                case device_speed::low:
                    return "USB 1.0 Low Speed (1.5 Mb/s)";
                case device_speed::full:
                    return "USB 1.1 Full Speed (12 Mb/s)";
                case device_speed::high:
                    return "USB 2.0 High Speed (480 Mb/s)";
                case device_speed::super:
                    return "USB 3.0 SuperSpeed (5.0 Gb/s)";
                case device_speed::super_plus:
                    return "USB 3.1 SuperSpeed+ (10.0 Gb/s)";
            }
            return "USB Unknown speed";
        }

        /// interface manages a libusb_device_handle.
        class interface {
            public:
            interface() = default;
            interface(std::shared_ptr<libusb_context> context, libusb_device* device) :
                _context(std::move(context)), _device(device), _claimed_interface(true) {
                libusb_device_handle* handle;
                throw_on_error("opening the device", libusb_open(device, &handle));
                _handle.reset(handle);
                throw_on_error("claiming the interface", libusb_claim_interface(_handle.get(), 0));
                const auto reset_result = libusb_reset_device(_handle.get());
                if (reset_result < 0) {
                    libusb_release_interface(_handle.get(), 0);
                    throw error(
                        std::string("resetting the device failed: ")
                        + libusb_strerror(static_cast<libusb_error>(reset_result)));
                }
            }
            interface(const interface&) = delete;
            interface(interface&& other) :
                _context(std::move(other._context)),
                _device(other._device),
                _claimed_interface(other._claimed_interface),
                _handle(std::move(other._handle)) {
                other._claimed_interface = false;
            }
            interface& operator=(const interface&) = delete;
            interface& operator=(interface&& other) {
                _context = std::move(other._context);
                _device = other._device;
                _claimed_interface = other._claimed_interface;
                _handle = std::move(other._handle);
                other._claimed_interface = false;
                return *this;
            }
            virtual ~interface() {
                if (_handle && _claimed_interface) {
                    libusb_release_interface(_handle.get(), 0);
                    _claimed_interface = false;
                }
            }

            virtual std::shared_ptr<libusb_context> context() {
                return _context;
            }

            /// operator bool returns true if interface manages an active handle.
            explicit operator bool() const {
                return _handle.operator bool();
            }

            /// descriptor returns the device descriptor.
            virtual libusb_device_descriptor descriptor() {
                libusb_device_descriptor result;
                throw_on_error("retrieving the descriptor", libusb_get_device_descriptor(_device, &result));
                return result;
            }

            /// halt wraps libusb_clear_halt.
            virtual void halt(uint8_t endpoint) {
                throw_on_error("halting the device", libusb_clear_halt(_handle.get(), endpoint));
            }

            /// get_device_speed wraps libusb_get_device_speed.
            virtual device_speed get_device_speed() {
                const auto raw_device_speed = libusb_get_device_speed(_device);
                switch (raw_device_speed) {
                    case LIBUSB_SPEED_UNKNOWN:
                        return device_speed::unknown;
                    case LIBUSB_SPEED_LOW:
                        return device_speed::low;
                    case LIBUSB_SPEED_FULL:
                        return device_speed::full;
                    case LIBUSB_SPEED_HIGH:
                        return device_speed::high;
                    case LIBUSB_SPEED_SUPER:
                        return device_speed::super;
                    case LIBUSB_SPEED_SUPER_PLUS:
                        return device_speed::super_plus;
                }
                return device_speed::unknown;
            }

            /// fill_bulk_transfer wraps libusb_fill_bulk_transfer.
            virtual void fill_bulk_transfer(
                libusb_transfer* transfer,
                uint8_t endpoint,
                std::vector<uint8_t>& buffer,
                libusb_transfer_cb_fn callback,
                void* user_data,
                uint32_t timeout = 0) {
                libusb_fill_bulk_transfer(
                    transfer,
                    _handle.get(),
                    endpoint,
                    buffer.data(),
                    static_cast<int32_t>(buffer.size()),
                    callback,
                    user_data,
                    timeout);
            }

            /// unchecked_control_transfer wraps libusb_control_transfer.
            /// The number of read bytes is not checked.
            virtual int32_t unchecked_control_transfer(
                const std::string& message,
                uint8_t bm_request_type,
                uint8_t b_request,
                uint16_t w_value,
                uint16_t w_index,
                std::vector<uint8_t>& buffer,
                uint32_t timeout = 0) {
                const auto bytes_transferred = libusb_control_transfer(
                    _handle.get(),
                    bm_request_type,
                    b_request,
                    w_value,
                    w_index,
                    buffer.data(),
                    static_cast<uint16_t>(buffer.size()),
                    timeout);
                throw_on_error(message, bytes_transferred);
                return bytes_transferred;
            }

            /// control_transfer wraps libusb_control_transfer.
            virtual void control_transfer(
                const std::string& message,
                uint8_t bm_request_type,
                uint8_t b_request,
                uint16_t w_value,
                uint16_t w_index,
                std::vector<uint8_t>& buffer,
                uint32_t timeout = 0) {
                const auto bytes_transferred = unchecked_control_transfer(
                    message,
                    bm_request_type,
                    b_request,
                    w_value,
                    w_index,
                    buffer,
                    timeout
                );
                if (bytes_transferred != static_cast<int32_t>(buffer.size())) {
                    throw std::runtime_error(
                        message + " failed: non-matching data and transfer sizes (expected "
                        + std::to_string(buffer.size()) + " bytes, got " + std::to_string(bytes_transferred) + ")");
                }
            }
            virtual std::vector<uint8_t> control_transfer(
                const std::string& message,
                uint8_t bm_request_type,
                uint8_t b_request,
                uint16_t w_value,
                uint16_t w_index,
                std::vector<uint8_t>&& buffer,
                uint32_t timeout = 0) {
                control_transfer(message, bm_request_type, b_request, w_value, w_index, buffer, timeout);
                return std::move(buffer);
            }

            /// checked_control_transfer performs a control transfer and validates the
            /// returned buffer value.
            virtual void checked_control_transfer(
                const std::string& message,
                uint8_t bm_request_type,
                uint8_t b_request,
                uint16_t w_value,
                uint16_t w_index,
                const std::vector<uint8_t>& expected_buffer,
                uint32_t timeout = 0) {
                auto buffer = expected_buffer;
                control_transfer(message, bm_request_type, b_request, w_value, w_index, buffer);
                for (std::size_t index = 0; index < buffer.size(); ++index) {
                    if (buffer[index] != expected_buffer[index]) {
                        throw error(message + " failed: unexpected response");
                    }
                }
            };

            /// get_string_descriptor_ascii wraps libusb_get_string_descriptor_ascii.
            virtual std::string get_string_descriptor_ascii(uint8_t descriptor_index, uint8_t size = 255) {
                std::vector<uint8_t> buffer(size);
                auto bytes_read =
                    libusb_get_string_descriptor_ascii(_handle.get(), descriptor_index, buffer.data(), size);
                throw_on_error("getting the descriptor", bytes_read);
                return std::string(buffer.begin(), std::next(buffer.begin(), bytes_read));
            }

            /// bulk_transfer wraps libusb_bulk_transfer.
            /// For 'read' bulk transfers (device to host), don't forget to resize the
            /// buffer before calling this function.
            virtual void bulk_transfer(
                const std::string& message,
                uint8_t endpoint,
                std::vector<uint8_t>& buffer,
                uint32_t timeout = 0) {
                int32_t actual_size;
                throw_on_error(
                    message,
                    libusb_bulk_transfer(
                        _handle.get(),
                        endpoint,
                        buffer.data(),
                        static_cast<int32_t>(buffer.size()),
                        &actual_size,
                        timeout));
                buffer.resize(actual_size);
            }
            virtual std::vector<uint8_t> bulk_transfer(
                const std::string& message,
                uint8_t endpoint,
                std::vector<uint8_t>&& buffer,
                uint32_t timeout = 0) {
                bulk_transfer(message, endpoint, buffer, timeout);
                return std::move(buffer);
            }

            /// bulk_transfer_accept_timeout wraps libusb_bulk_transfer, and does not
            /// throw on timeout.
            virtual void bulk_transfer_accept_timeout(
                const std::string& message,
                uint8_t endpoint,
                std::vector<uint8_t>& buffer,
                uint32_t timeout) {
                int32_t actual_size;
                const auto error = libusb_bulk_transfer(
                    _handle.get(), endpoint, buffer.data(), static_cast<int32_t>(buffer.size()), &actual_size, timeout);
                if (error != LIBUSB_ERROR_TIMEOUT) {
                    throw_on_error(message, error);
                }
                buffer.resize(actual_size);
            }

            protected:
            /// handle_deleter is a custom deleter for a libusb_device_handle.
            struct handle_deleter {
                void operator()(libusb_device_handle* handle) {
                    libusb_release_interface(handle, 0);
                    libusb_close(handle);
                }
            };

            std::shared_ptr<libusb_context> _context;
            libusb_device* _device;
            bool _claimed_interface;
            std::unique_ptr<libusb_device_handle, handle_deleter> _handle;
        };

        /// identity describes vendor and product IDs.
        struct identity {
            uint16_t vendor;
            uint16_t product;
        };

        /// any_of loops over connected devices with the specified identities.
        template <typename HandleDevice>
        bool any_of(
            std::initializer_list<identity> identities,
            std::shared_ptr<libusb_context> context,
            HandleDevice handle_device) {
            libusb_device** raw_devices;
            const auto count = libusb_get_device_list(context.get(), &raw_devices);
            auto devices_deleter = [](libusb_device** devices) { libusb_free_device_list(devices, 1); };
            std::unique_ptr<libusb_device*, decltype(devices_deleter)> devices(raw_devices, devices_deleter);
            for (ssize_t index = 0; index < count; ++index) {
                libusb_device_descriptor descriptor;
                if (libusb_get_device_descriptor(devices.get()[index], &descriptor) == 0) {
                    for (const auto identity : identities) {
                        if (descriptor.idVendor == identity.vendor && descriptor.idProduct == identity.product) {
                            if (handle_device(devices.get()[index])) {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }

        /// device_properties represents a device's serial and speed.
        struct device_properties {
            uint32_t type;
            std::string serial;
            device_speed speed;
        };

        /// available_devices returns a list of connected devices serials and USB speeds.
        template <typename GetTypeAndSerial>
        inline std::vector<device_properties> available_devices(
            std::initializer_list<identity> identities,
            GetTypeAndSerial get_type_and_serial,
            std::shared_ptr<libusb_context> context = {}) {
            if (!context) {
                context = make_context();
            }
            std::vector<device_properties> devices_properties;
            any_of(identities, context, [&](libusb_device* device) {
                try {
                    interface usb_interface(context, device);
                    const auto [type, serial] = get_type_and_serial(usb_interface);
                    devices_properties.push_back({type, serial, usb_interface.get_device_speed()});
                } catch (const std::runtime_error& error) {}
                return false;
            });
            return devices_properties;
        }

        /// open creates an interface from a VID/PID.
        inline interface open(
            const std::string& device_name,
            std::initializer_list<identity> identities,
            std::shared_ptr<libusb_context> context = {}) {
            if (!context) {
                context = make_context();
            }
            interface usb_interface;
            if (!any_of(identities, context, [&](libusb_device* device) {
                    try {
                        usb_interface = interface(context, device);
                        return true;
                    } catch (const std::runtime_error&) {
                    }
                    return false;
                })) {
                throw no_device_available(device_name);
            }
            return usb_interface;
        }

        /// open creates an interface from an identity and a serial.
        template <typename GetTypeAndSerial>
        inline interface open(
            const std::string& device_name,
            std::initializer_list<identity> identities,
            GetTypeAndSerial get_type_and_serial,
            const std::string& serial = "",
            uint64_t type = 0,
            std::shared_ptr<libusb_context> context = {}) {
            if (serial.empty() && type == 0) {
                return open(device_name, identities, context);
            }
            if (!context) {
                context = make_context();
            }
            interface usb_interface;
            if (!any_of(identities, context, [&](libusb_device* device) {
                    try {
                        usb_interface = interface(context, device);
                        const auto [device_type, device_serial] = get_type_and_serial(usb_interface);
                        if (type == 0) {
                            return device_serial == serial;
                        }
                        if (serial.empty()) {
                            return device_type == type;
                        }
                        return device_serial == serial && device_type == type;
                    } catch (const std::runtime_error&) {
                    }
                    return false;
                })) {
                throw serial_not_available(device_name, serial);
            }
            return usb_interface;
        }
    }
}
