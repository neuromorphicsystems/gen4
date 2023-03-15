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
    /// camera is a common base type for buffered cameras.
    class camera {
        public:
        camera() {}
        camera(const camera&) = default;
        camera(camera&& other) = default;
        camera& operator=(const camera&) = default;
        camera& operator=(camera&& other) = default;
        virtual ~camera() {}

        /// set_drop_threshold changes the maximum size of the fifo.
        virtual void set_drop_threshold(std::size_t drop_threshold) = 0;
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

    /// fifo implements a thread-safe buffer FIFO.
    class fifo {
        public:
        fifo(const std::chrono::steady_clock::duration& timeout, std::function<void(std::size_t)> handle_drop) :
            _timeout(timeout), _handle_drop(std::move(handle_drop)), _drop_threshold(0) {}
        fifo(const fifo&) = delete;
        fifo(fifo&& other) = delete;
        fifo& operator=(const fifo&) = delete;
        fifo& operator=(fifo&& other) = delete;
        virtual ~fifo() {}

        /// pop removes and returns the next buffer.
        /// It returns false if the specified timeout is reached before a buffer is
        /// available.
        virtual bool pop(std::vector<uint8_t>& buffer) {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_buffers.empty()) {
                if (!_condition_variable.wait_for(lock, _timeout, [this] { return !_buffers.empty(); })) {
                    return false;
                }
            }
            buffer.swap(_buffers.front());
            _buffers.pop_front();
            return true;
        }

        /// push inserts a buffer.
        virtual void push(std::vector<uint8_t>& buffer) {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                const auto drop_threshold = _drop_threshold.load(std::memory_order_acquire);
                if (drop_threshold > 0 && _buffers.size() > drop_threshold) {
                    std::size_t dropped_bytes = 0;
                    for (const auto& buffer : _buffers) {
                        dropped_bytes += buffer.size();
                    }
                    _buffers.clear();
                    _handle_drop(dropped_bytes);
                }
                _buffers.emplace_back();
                buffer.swap(_buffers.back());
            }
            _condition_variable.notify_one();
        }

        /// push_bytes copies bytes from an iterator.
        template <typename InputIterator>
        void copy_and_push(InputIterator first, InputIterator last) {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                const auto drop_threshold = _drop_threshold.load(std::memory_order_acquire);
                if (drop_threshold > 0 && _buffers.size() > drop_threshold) {
                    std::size_t dropped_bytes = 0;
                    for (const auto& buffer : _buffers) {
                        dropped_bytes += buffer.size();
                    }
                    _buffers.clear();
                    _handle_drop(dropped_bytes);
                }
                _buffers.emplace_back();
                _buffers.back().resize(static_cast<std::size_t>(std::distance(first, last)));
                std::copy(first, last, _buffers.back().data());
            }
            _condition_variable.notify_one();
        }

        /// set_drop_threshold changes the maximum size of the fifo.
        virtual void set_drop_threshold(std::size_t drop_threshold) {
            _drop_threshold.store(drop_threshold, std::memory_order_release);
        }

        protected:
        const std::chrono::steady_clock::duration& _timeout;
        std::function<void(std::size_t)> _handle_drop;
        std::atomic<std::size_t> _drop_threshold;
        std::deque<std::vector<uint8_t>> _buffers;
        std::mutex _mutex;
        std::condition_variable _condition_variable;
    };

    /// buffered_camera represents a template-specialized generic camera.
    template <typename HandleBuffer, typename HandleException>
    class buffered_camera : public virtual camera {
        public:
        buffered_camera(
            HandleBuffer&& handle_buffer,
            HandleException&& handle_exception,
            const std::chrono::steady_clock::duration& timeout,
            std::function<void(std::size_t)> handle_drop) :
            _handle_buffer(std::forward<HandleBuffer>(handle_buffer)),
            _handle_exception(std::forward<HandleException>(handle_exception)),
            _fifo(timeout, std::move(handle_drop)),
            _running(true) {
            _buffer_loop = std::thread([this]() {
                try {
                    std::vector<uint8_t> buffer;
                    while (_running.load(std::memory_order_relaxed)) {
                        if (_fifo.pop(buffer)) {
                            _handle_buffer(buffer);
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

        virtual void set_drop_threshold(std::size_t drop_threshold) override {
            _fifo.set_drop_threshold(drop_threshold);
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
