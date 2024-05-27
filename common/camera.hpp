#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace sepia {
    /// system_timestamp returns a monotonic arbitrary time representation in nanoseconds.
    static uint64_t system_timestamp_now() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    /// camera is a common base type for buffered cameras.
    class camera {
        public:
        camera() {}
        camera(const camera&) = default;
        camera(camera&& other) = default;
        camera& operator=(const camera&) = default;
        camera& operator=(camera&& other) = default;
        virtual ~camera() {}
    };

    /// parametric_camera adds parameters update support to camera.
    template <typename Parameters>
    class parametric_camera : public virtual camera {
        public:
        parametric_camera(Parameters parameters) : _parameters(parameters), _update_required(false) {}
        parametric_camera(const parametric_camera&) = delete;
        parametric_camera(parametric_camera&& other) = delete;
        parametric_camera& operator=(const parametric_camera&) = delete;
        parametric_camera& operator=(parametric_camera&& other) = delete;
        virtual ~parametric_camera() {}

        /// update_parameters sends a bias update request.
        virtual void update_parameters(const Parameters& parameters) {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _update_required = true;
                _parameters = parameters;
            }
            _condition_variable.notify_one();
        }

        protected:
        Parameters _parameters;
        bool _update_required;
        std::mutex _mutex;
        std::condition_variable _condition_variable;
    };

    /// pop_result returns the FIFO status on pop.
    struct pop_result {
        std::size_t used;
        std::size_t size;
        bool success;
    };

    /// fifo implements a thread-safe buffer FIFO.
    class fifo {
        public:
        fifo(const std::chrono::steady_clock::duration& timeout, std::size_t fifo_size, std::function<void()> handle_drop) :
            _timeout(timeout), _handle_drop(std::move(handle_drop)) {
            _buffers.resize(fifo_size);
        }
        fifo(const fifo&) = delete;
        fifo(fifo&& other) = delete;
        fifo& operator=(const fifo&) = delete;
        fifo& operator=(fifo&& other) = delete;
        virtual ~fifo() {}

        /// pop removes and returns the next buffer.
        /// It returns false if the specified timeout is reached before a buffer is
        /// available.
        virtual pop_result pop(std::vector<uint8_t>& buffer) {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_read_index == _write_index) {
                if (!_condition_variable.wait_for(lock, _timeout, [this] { return _read_index != _write_index; })) {
                    return pop_result{
                        0,
                        _buffers.size(),
                        false,
                    };
                }
            }
            buffer.swap(_buffers[_read_index]);
            _read_index = (_read_index + 1) % _buffers.size();
            return pop_result{
                (_write_index + _buffers.size() - _read_index) % _buffers.size(),
                _buffers.size(),
                true,
            };
        }

        /// push inserts a buffer.
        virtual void push(std::vector<uint8_t>& buffer) {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                const auto next_write_index = (_write_index + 1) % _buffers.size();
                if (_read_index == next_write_index) {
                    _handle_drop();
                } else {
                    buffer.swap(_buffers[_write_index]);
                    _write_index = next_write_index;
                }
            }
            _condition_variable.notify_one();
        }

        /// push_bytes copies bytes from an iterator.
        template <typename InputIterator>
        void copy_and_push(InputIterator first, InputIterator last) {
            const auto system_timestamp = system_timestamp_now();
            {
                std::lock_guard<std::mutex> lock(_mutex);
                const auto next_write_index = (_write_index + 1) % _buffers.size();
                if (_read_index == next_write_index) {
                    _handle_drop();
                } else {
                    const auto data_size = static_cast<std::size_t>(std::distance(first, last));
                    _buffers[_write_index].resize(data_size + sizeof(system_timestamp));
                    std::copy(first, last, _buffers[_write_index].data());
                    std::copy(
                        reinterpret_cast<const uint8_t*>(&system_timestamp),
                        reinterpret_cast<const uint8_t*>(&system_timestamp) + sizeof(system_timestamp),
                        _buffers[_write_index].data() + data_size);
                    _write_index = next_write_index;
                }
            }
            _condition_variable.notify_one();
        }

        protected:
        const std::chrono::steady_clock::duration& _timeout;
        std::function<void()> _handle_drop;
        std::vector<std::vector<uint8_t>> _buffers;
        std::mutex _mutex;
        std::condition_variable _condition_variable;
        std::size_t _write_index = 0;
        std::size_t _read_index = 0;
    };

    /// buffered_camera represents a template-specialized generic camera.
    template <typename HandleBuffer, typename HandleException>
    class buffered_camera : public virtual camera {
        public:
        buffered_camera(
            HandleBuffer&& handle_buffer,
            HandleException&& handle_exception,
            const std::chrono::steady_clock::duration& timeout,
            std::size_t fifo_size,
            std::function<void()> handle_drop) :
            _handle_buffer(std::forward<HandleBuffer>(handle_buffer)),
            _handle_exception(std::forward<HandleException>(handle_exception)),
            _fifo(timeout, fifo_size, std::move(handle_drop)),
            _running(true) {
            _buffer_loop = std::thread([this]() {
                try {
                    std::vector<uint8_t> buffer;
                    while (_running.load(std::memory_order_relaxed)) {
                        const auto pop_result = _fifo.pop(buffer);
                        if (pop_result.success) {
                            _handle_buffer(buffer, pop_result.used, pop_result.size);
                        }
                    }
                } catch (...) {
                    if (_running.exchange(false)) {
                        _handle_exception(std::current_exception());
                    }
                }
            });
        }
        buffered_camera(const buffered_camera&) = delete;
        buffered_camera(buffered_camera&&) = delete;
        buffered_camera& operator=(const buffered_camera&) = delete;
        buffered_camera& operator=(buffered_camera&&) = delete;
        virtual ~buffered_camera() {
            _running.store(false, std::memory_order_relaxed);
            _buffer_loop.join();
        }

        protected:
        /// push inserts a buffer.
        virtual void push(std::vector<uint8_t>& buffer) {
            _fifo.push(buffer);
        }

        /// push_bytes copies bytes from an iterator.
        template <typename InputIterator>
        void copy_and_push(InputIterator first, InputIterator last) {
            _fifo.copy_and_push(first, last);
        }

        HandleBuffer _handle_buffer;
        HandleException _handle_exception;
        fifo _fifo;
        std::atomic_bool _running;
        std::thread _buffer_loop;
    };
}
