#define NOMINMAX
#include <Python.h>
#include <cstring>
#include <sstream>
#include <structmember.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#if defined(HAVE_SSIZE_T)
#define _SSIZE_T_DEFINED
#endif
#include "../common/evk4.hpp"
#include <filesystem>
#include <numpy/arrayobject.h>

std::string utc_timestamp() {
    std::timespec timespec;
    std::timespec_get(&timespec, TIME_UTC);
    std::stringstream timestamp;
    timestamp << std::put_time(std::gmtime(&timespec.tv_sec), "%FT%T.") << std::setfill('0') << std::setw(6)
              << (timespec.tv_nsec / 1000) << "Z";
    return timestamp.str();
}

/// python_path_to_string converts a path-like object to a string.
static std::string python_path_to_string(PyObject* path) {
    if (PyUnicode_Check(path)) {
        return reinterpret_cast<const char*>(PyUnicode_DATA(path));
    }
    {
        const auto characters = PyBytes_AsString(path);
        if (characters) {
            return characters;
        } else {
            PyErr_Clear();
        }
    }
    auto string_or_bytes = PyObject_CallMethod(path, "__fspath__", nullptr);
    if (string_or_bytes) {
        if (PyUnicode_Check(string_or_bytes)) {
            return reinterpret_cast<const char*>(PyUnicode_DATA(string_or_bytes));
        }
        const auto characters = PyBytes_AsString(string_or_bytes);
        if (characters) {
            return characters;
        } else {
            PyErr_Clear();
        }
    }
    throw std::runtime_error("path must be a string, bytes, or a path-like object");
}

/// description represents a named type with an offset.
struct description {
    std::string name;
    NPY_TYPES type;
};

/// descriptions returns the fields names, scalar types and offsets associated with an event type.
template <sepia::type event_stream_type>
std::vector<description> get_descriptions();
template <>
std::vector<description> get_descriptions<sepia::type::generic>() {
    return {{"t", NPY_UINT64}, {"bytes", NPY_OBJECT}};
}
template <>
std::vector<description> get_descriptions<sepia::type::dvs>() {
    return {{"t", NPY_UINT64}, {"x", NPY_UINT16}, {"y", NPY_UINT16}, {"on", NPY_BOOL}};
}
template <>
std::vector<description> get_descriptions<sepia::type::atis>() {
    return {{"t", NPY_UINT64}, {"x", NPY_UINT16}, {"y", NPY_UINT16}, {"exposure", NPY_BOOL}, {"polarity", NPY_BOOL}};
}
template <>
std::vector<description> get_descriptions<sepia::type::color>() {
    return {
        {"t", NPY_UINT64}, {"x", NPY_UINT16}, {"y", NPY_UINT16}, {"r", NPY_UINT8}, {"g", NPY_UINT8}, {"b", NPY_UINT8}};
}

/// offsets calculates the packed offsets from the description types.
template <sepia::type event_stream_type>
static std::vector<uint8_t> get_offsets() {
    auto descriptions = get_descriptions<event_stream_type>();
    std::vector<uint8_t> offsets(descriptions.size(), 0);
    for (std::size_t index = 1; index < descriptions.size(); ++index) {
        switch (descriptions[index - 1].type) {
            case NPY_BOOL:
            case NPY_UINT8:
                offsets[index] = offsets[index - 1] + 1;
                break;
            case NPY_UINT16:
                offsets[index] = offsets[index - 1] + 2;
                break;
            case NPY_UINT64:
                offsets[index] = offsets[index - 1] + 8;
                break;
            default:
                throw std::runtime_error("unknown type for offset calculation");
        }
    }
    return offsets;
}

/// event_type_to_dtype returns a PyArray_Descr object.
template <sepia::type event_stream_type>
static PyArray_Descr* event_type_to_dtype() {
    const auto descriptions = get_descriptions<event_stream_type>();
    auto python_names_and_types = PyList_New(static_cast<Py_ssize_t>(descriptions.size()));
    for (Py_ssize_t index = 0; index < static_cast<Py_ssize_t>(descriptions.size()); ++index) {
        if (PyList_SetItem(
                python_names_and_types,
                index,
                PyTuple_Pack(
                    2,
                    PyUnicode_FromString(descriptions[index].name.c_str()),
                    PyArray_TypeObjectFromType(descriptions[index].type)))
            < 0) {
            throw std::logic_error("PyList_SetItem failed");
        }
    }
    PyArray_Descr* dtype;
    if (PyArray_DescrConverter(python_names_and_types, &dtype) == NPY_FAIL) {
        throw std::logic_error("PyArray_DescrConverter failed");
    }
    return dtype;
}

/// allocate_array returns a structured array with the required length to accomodate the given stream.
template <sepia::type event_stream_type>
static PyArrayObject* allocate_array(npy_intp size) {
    return reinterpret_cast<PyArrayObject*>(PyArray_NewFromDescr(
        &PyArray_Type, event_type_to_dtype<event_stream_type>(), 1, &size, nullptr, nullptr, 0, nullptr));
}

/// events_to_array converts a buffer of events to a numpy array.
template <sepia::type event_stream_type>
PyObject*
events_to_array(const std::vector<sepia::event<event_stream_type>>& buffer, const std::vector<uint8_t>& offsets);
template <>
PyObject* events_to_array(const std::vector<sepia::generic_event>& buffer, const std::vector<uint8_t>& offsets) {
    auto events = allocate_array<sepia::type::generic>(buffer.size());
    for (npy_intp index = 0; index < static_cast<npy_intp>(buffer.size()); ++index) {
        const auto generic_event = buffer[index];
        auto payload = reinterpret_cast<uint8_t*>(PyArray_GETPTR1(events, index));
        *reinterpret_cast<uint64_t*>(payload + offsets[0]) = generic_event.t;
        *reinterpret_cast<PyObject**>(payload + offsets[1]) = PyBytes_FromStringAndSize(
            reinterpret_cast<const char*>(generic_event.bytes.data()), generic_event.bytes.size());
    }
    return reinterpret_cast<PyObject*>(events);
}
template <>
PyObject* events_to_array(const std::vector<sepia::dvs_event>& buffer, const std::vector<uint8_t>& offsets) {
    auto events = allocate_array<sepia::type::dvs>(buffer.size());
    for (npy_intp index = 0; index < static_cast<npy_intp>(buffer.size()); ++index) {
        const auto dvs_event = buffer[index];
        auto payload = reinterpret_cast<uint8_t*>(PyArray_GETPTR1(events, index));
        *reinterpret_cast<uint64_t*>(payload + offsets[0]) = dvs_event.t;
        *reinterpret_cast<uint16_t*>(payload + offsets[1]) = dvs_event.x;
        *reinterpret_cast<uint16_t*>(payload + offsets[2]) = dvs_event.y;
        *reinterpret_cast<bool*>(payload + offsets[3]) = dvs_event.on;
    }
    return reinterpret_cast<PyObject*>(events);
}
template <>
PyObject* events_to_array(const std::vector<sepia::atis_event>& buffer, const std::vector<uint8_t>& offsets) {
    auto events = allocate_array<sepia::type::atis>(buffer.size());
    for (npy_intp index = 0; index < static_cast<npy_intp>(buffer.size()); ++index) {
        const auto atis_event = buffer[index];
        auto payload = reinterpret_cast<uint8_t*>(PyArray_GETPTR1(events, index));
        *reinterpret_cast<uint64_t*>(payload + offsets[0]) = atis_event.t;
        *reinterpret_cast<uint16_t*>(payload + offsets[1]) = atis_event.x;
        *reinterpret_cast<uint16_t*>(payload + offsets[2]) = atis_event.y;
        *reinterpret_cast<bool*>(payload + offsets[3]) = atis_event.is_threshold_crossing;
        *reinterpret_cast<bool*>(payload + offsets[4]) = atis_event.polarity;
    }
    return reinterpret_cast<PyObject*>(events);
}
template <>
PyObject* events_to_array(const std::vector<sepia::color_event>& buffer, const std::vector<uint8_t>& offsets) {
    auto events = allocate_array<sepia::type::color>(buffer.size());
    for (npy_intp index = 0; index < static_cast<npy_intp>(buffer.size()); ++index) {
        const auto color_event = buffer[index];
        auto payload = reinterpret_cast<uint8_t*>(PyArray_GETPTR1(events, index));
        *reinterpret_cast<uint64_t*>(payload + offsets[0]) = color_event.t;
        *reinterpret_cast<uint16_t*>(payload + offsets[1]) = color_event.x;
        *reinterpret_cast<uint16_t*>(payload + offsets[2]) = color_event.y;
        *reinterpret_cast<uint8_t*>(payload + offsets[3]) = color_event.r;
        *reinterpret_cast<uint8_t*>(payload + offsets[4]) = color_event.g;
        *reinterpret_cast<uint8_t*>(payload + offsets[5]) = color_event.b;
    }
    return reinterpret_cast<PyObject*>(events);
}

/// read_bias extracts a bias from a Python dict.
static uint8_t read_bias(PyObject* biases_dict, const char* key) {
    auto value = PyDict_GetItemString(biases_dict, key);
    if (!value) {
        throw std::runtime_error(std::string("parameters.biases must have a ") + key + " key");
    }
    if (!PyLong_Check(value)) {
        throw std::runtime_error(std::string("parameters.biases.") + key + " must be an int");
    }
    auto result = PyLong_AsLong(value);
    PyErr_Clear();
    if (result < 0 || result > 255) {
        throw std::runtime_error(std::string("parameters.biases.") + key + " must be in the range [0, 255]");
    }
    return static_cast<uint8_t>(result);
}

/// camera reads events from a Prophesee Gen 4 dev kit 1.3 (Denebola).
struct camera_data {
    std::atomic_flag accessing_camera;
    std::string recordings_directory;
    std::string target_recording_name;
    std::string recording_name;
    std::unique_ptr<sepia::write<sepia::type::dvs>> write_event;
    uint64_t first_t;
    uint64_t previous_t;
    uint64_t last_size_read_t;
    std::string file_name;
    std::size_t file_duration;
    std::size_t file_size;
    std::exception_ptr exception;
    std::vector<sepia::dvs_event> buffer;
    std::deque<std::vector<sepia::dvs_event>>* buffers;
    std::vector<uint8_t> dvs_offsets;
    std::unique_ptr<std::ofstream> jsonl_log;
    std::unique_ptr<sepia::evk4::base_camera> base_camera;
};
struct camera {
    PyObject_HEAD camera_data* data;
};
static void camera_dealloc(PyObject* self) {
    auto current = reinterpret_cast<camera*>(self);
    if (current->data) {
        delete current->data;
        current->data = nullptr;
    }
    Py_TYPE(self)->tp_free(self);
}
static PyObject* camera_new(PyTypeObject* type, PyObject*, PyObject*) {
    return type->tp_alloc(type, 0);
}
static PyMemberDef camera_members[] = {
    {nullptr, 0, 0, 0, nullptr},
};
static PyObject* next_packet(PyObject* self, PyObject* args) {
    auto current = reinterpret_cast<camera*>(self);
    try {
        std::exception_ptr exception;
        std::vector<sepia::dvs_event> events;
        while (current->data->accessing_camera.test_and_set(std::memory_order_acquire)) {
        }
        if (current->data->exception) {
            exception = current->data->exception;
        } else if (!current->data->buffers->empty()) {
            events.swap(current->data->buffers->front());
            current->data->buffers->pop_front();
        }
        current->data->accessing_camera.clear(std::memory_order_release);
        if (exception) {
            std::rethrow_exception(exception);
        }
        return events_to_array<sepia::type::dvs>(events, current->data->dvs_offsets);
    } catch (const std::exception& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
        return nullptr;
    }
    return nullptr;
}
static PyObject* all_packets(PyObject* self, PyObject* args) {
    auto current = reinterpret_cast<camera*>(self);
    try {
        std::exception_ptr exception;
        std::vector<sepia::dvs_event> events;
        std::size_t total = 0;
        while (current->data->accessing_camera.test_and_set(std::memory_order_acquire)) {
        }
        const auto size = current->data->buffers->size();
        for (const auto& buffer : (*current->data->buffers)) {
            total += buffer.size();
        }
        if (current->data->exception) {
            exception = current->data->exception;
        }
        current->data->accessing_camera.clear(std::memory_order_release);
        if (exception) {
            std::rethrow_exception(exception);
        }
        if (size == 0) {
            return events_to_array<sepia::type::dvs>(events, current->data->dvs_offsets);
        }
        auto events_array = allocate_array<sepia::type::dvs>(total);
        std::size_t offset = 0;
        for (std::size_t buffer_index = 0; buffer_index < size; ++buffer_index) {
            while (current->data->accessing_camera.test_and_set(std::memory_order_acquire)) {
            }
            events.swap(current->data->buffers->front());
            current->data->buffers->pop_front();
            current->data->accessing_camera.clear(std::memory_order_release);
            for (npy_intp index = 0; index < static_cast<npy_intp>(events.size()); ++index) {
                const auto dvs_event = events[index];
                auto payload = reinterpret_cast<uint8_t*>(PyArray_GETPTR1(events_array, index + offset));
                *reinterpret_cast<uint64_t*>(payload + current->data->dvs_offsets[0]) = dvs_event.t;
                *reinterpret_cast<uint16_t*>(payload + current->data->dvs_offsets[1]) = dvs_event.x;
                *reinterpret_cast<uint16_t*>(payload + current->data->dvs_offsets[2]) = dvs_event.y;
                *reinterpret_cast<bool*>(payload + current->data->dvs_offsets[3]) = dvs_event.on;
            }
            offset += events.size();
        }
        return reinterpret_cast<PyObject*>(events_array);
    } catch (const std::exception& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
        return nullptr;
    }
    return nullptr;
}
static PyObject* set_parameters(PyObject* self, PyObject* args) {
    auto current = reinterpret_cast<camera*>(self);
    PyObject* parameters_dict;
    if (!PyArg_ParseTuple(args, "O", &parameters_dict)) {
        return nullptr;
    }
    try {
        auto biases_dict = PyDict_GetItemString(parameters_dict, "biases");
        if (!biases_dict) {
            throw std::runtime_error("parameters must have a biases key");
        }
        if (!PyDict_Check(biases_dict)) {
            throw std::runtime_error("parameters.biases must be a dict");
        }
        auto parameters = sepia::evk4::default_parameters;
        parameters.biases.pr = read_bias(biases_dict, "pr");
        parameters.biases.fo = read_bias(biases_dict, "fo");
        parameters.biases.hpf = read_bias(biases_dict, "hpf");
        parameters.biases.diff_on = read_bias(biases_dict, "diff_on");
        parameters.biases.diff = read_bias(biases_dict, "diff");
        parameters.biases.diff_off = read_bias(biases_dict, "diff_off");
        parameters.biases.inv = read_bias(biases_dict, "inv");
        parameters.biases.refr = read_bias(biases_dict, "refr");
        parameters.biases.reqpuy = read_bias(biases_dict, "reqpuy");
        parameters.biases.reqpux = read_bias(biases_dict, "reqpux");
        parameters.biases.sendreqpdy = read_bias(biases_dict, "sendreqpdy");
        parameters.biases.unknown_1 = read_bias(biases_dict, "unknown_1");
        parameters.biases.unknown_2 = read_bias(biases_dict, "unknown_2");
        current->data->base_camera->update_parameters(parameters);
        Py_RETURN_NONE;
    } catch (const std::exception& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
        return nullptr;
    }
    return nullptr;
}
static PyObject* backlog(PyObject* self, PyObject* args) {
    auto current = reinterpret_cast<camera*>(self);
    std::size_t total = 0;
    while (current->data->accessing_camera.test_and_set(std::memory_order_acquire)) {
    }
    for (const auto& buffer : (*current->data->buffers)) {
        total += buffer.size();
    }
    current->data->accessing_camera.clear(std::memory_order_release);
    return PyLong_FromSsize_t(total);
}
static PyObject* clear_backlog(PyObject* self, PyObject* args) {
    auto current = reinterpret_cast<camera*>(self);
    while (current->data->accessing_camera.test_and_set(std::memory_order_acquire)) {
    }
    current->data->buffers->clear();
    current->data->accessing_camera.clear(std::memory_order_release);
    Py_RETURN_NONE;
}
static PyObject* record_to(PyObject* self, PyObject* args) {
    auto current = reinterpret_cast<camera*>(self);
    const char* raw_name;
    if (!PyArg_ParseTuple(args, "s", &raw_name)) {
        return nullptr;
    }
    auto name = std::string(raw_name);
    while (current->data->accessing_camera.test_and_set(std::memory_order_acquire)) {
    }
    current->data->target_recording_name.swap(name);
    current->data->accessing_camera.clear(std::memory_order_release);
    Py_RETURN_NONE;
}

static PyObject* recording_status(PyObject* self, PyObject* args) {
    auto current = reinterpret_cast<camera*>(self);
    std::string file_name;
    std::size_t file_duration = 0;
    std::size_t file_size = 0;
    while (current->data->accessing_camera.test_and_set(std::memory_order_acquire)) {
    }
    file_name = current->data->file_name;
    file_duration = current->data->file_duration;
    file_size = current->data->file_size;
    current->data->accessing_camera.clear(std::memory_order_release);
    PyObject* status = PyTuple_New(3);
    PyTuple_SET_ITEM(status, 0, PyUnicode_FromString(file_name.c_str()));
    PyTuple_SET_ITEM(status, 1, PyLong_FromUnsignedLongLong(file_duration));
    PyTuple_SET_ITEM(status, 2, PyLong_FromUnsignedLongLong(file_size));
    return status;
}

static PyMethodDef camera_methods[] = {
    {"next_packet", next_packet, METH_VARARGS, nullptr},
    {"all_packets", all_packets, METH_VARARGS, nullptr},
    {"set_parameters", set_parameters, METH_VARARGS, nullptr},
    {"backlog", backlog, METH_NOARGS, nullptr},
    {"clear_backlog", clear_backlog, METH_NOARGS, nullptr},
    {"record_to", record_to, METH_VARARGS, nullptr},
    {"recording_status", recording_status, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};
static int camera_init(PyObject* self, PyObject* args, PyObject*) {
    auto current = reinterpret_cast<camera*>(self);
    PyObject* recordings_path;
    PyObject* log_path;
    if (!PyArg_ParseTuple(args, "OO", &recordings_path, &log_path)) {
        return -1;
    }
    try {
        current->data = new camera_data;
        auto data = current->data;
        data->accessing_camera.clear(std::memory_order_release);
        data->recordings_directory = python_path_to_string(recordings_path);
        data->first_t = 0;
        data->previous_t = 0;
        data->last_size_read_t = 0;
        data->file_duration = 0;
        data->file_size = 0;
        data->buffers = new std::deque<std::vector<sepia::dvs_event>>;
        data->dvs_offsets = get_offsets<sepia::type::dvs>();
        data->jsonl_log.reset(
            new std::ofstream(python_path_to_string(log_path), std::ios::binary | std::ios::app | std::ios::out));
        data->base_camera = sepia::evk4::make_camera(
            [=](sepia::dvs_event event) {
                data->buffer.push_back(event);
                if (data->write_event) {
                    data->write_event->operator()({
                        event.t - data->first_t,
                        event.x,
                        event.y,
                        event.on,
                    });
                }
                data->previous_t = event.t;
            },
            [=](sepia::evk4::trigger_event trigger_event) {
                if (data->write_event) {
                    std::stringstream message;
                    message << "{\"timestamp\":\"" << utc_timestamp() << "\",\"type\":\"trigger\",\"file_name\":\""
                            << data->file_name << "\",\"t\":" << (trigger_event.t - data->first_t)
                            << ",\"system_timestamp\":" << trigger_event.system_timestamp
                            << ",\"id\":" << static_cast<int32_t>(trigger_event.id)
                            << ",\"rising\":" << (trigger_event.rising ? "true" : "false") << "}\n";
                    const std::string message_string = message.str();
                    data->jsonl_log->write(message_string.data(), message_string.size());
                    data->jsonl_log->flush();
                }
                data->previous_t = trigger_event.t;
            },
            [=]() {},
            [=]() {
                while (data->accessing_camera.test_and_set(std::memory_order_acquire)) {
                }
                if (data->write_event) {
                    if (data->target_recording_name.empty() || data->target_recording_name != data->recording_name) {
                        data->recording_name.clear();
                        data->write_event.reset();
                        data->file_name.clear();
                        data->file_duration = 0;
                        data->file_size = 0;
                    } else {
                        data->file_duration = data->previous_t - data->first_t;
                        if (data->previous_t - data->last_size_read_t > 1e6) {
                            data->last_size_read_t = data->previous_t;
                            data->file_size = std::filesystem::file_size(data->file_name);
                        }
                    }
                }
                if (!data->write_event && !data->target_recording_name.empty()) {
                    data->recording_name = data->target_recording_name;
                    data->file_name = data->recordings_directory + "/" + data->target_recording_name;
                    data->file_duration = 0;
                    data->file_size = 0;
                    data->first_t = data->previous_t;
                    data->last_size_read_t = 0;
                    data->write_event = std::make_unique<sepia::write<sepia::type::dvs>>(
                        sepia::filename_to_ofstream(data->file_name), sepia::evk4::width, sepia::evk4::height);
                    const auto utc = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                                                        std::chrono::system_clock::now().time_since_epoch())
                                                        .count());
                    std::stringstream message;
                    message << "{\"timestamp\":" << utc_timestamp() << ",\"type\":\"clap\",\"utc\":" << utc
                            << ",\"filename\":\"" << data->file_name << "\"}\n";
                    const std::string message_string = message.str();
                    data->jsonl_log->write(message_string.data(), message_string.size());
                    data->jsonl_log->flush();
                }
                data->buffers->emplace_back();
                data->buffer.swap(data->buffers->back());
                data->accessing_camera.clear(std::memory_order_release);
            },
            [=](std::exception_ptr exception) {
                while (data->accessing_camera.test_and_set(std::memory_order_acquire)) {
                }
                data->exception = exception;
                data->accessing_camera.clear(std::memory_order_release);
            },
            sepia::evk4::default_parameters,
            "",
            std::chrono::milliseconds(100),
            64,
            [=](std::size_t dropped_bytes) {
                std::stringstream message;
                message << "{\"timestamp\":" << utc_timestamp() << ",\"type\":\"drop\",\"bytes\":" << dropped_bytes
                        << "}\n";
                const std::string message_string = message.str();
                data->jsonl_log->write(message_string.data(), message_string.size());
                data->jsonl_log->flush();
            });
    } catch (const std::exception& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
        return -1;
    }
    return 0;
}
static PyTypeObject camera_type = {PyVarObject_HEAD_INIT(nullptr, 0)};

static PyObject* system_timestamp_now(PyObject*, PyObject*) {
    return PyLong_FromUnsignedLongLong(sepia::system_timestamp_now());
}

static PyMethodDef evk4_extension_methods[] = {
    {"system_timestamp_now",
     system_timestamp_now,
     METH_NOARGS,
     "returns a monotonic arbitrary time representation in nanoseconds"},
    {nullptr, nullptr, 0, nullptr}};
static struct PyModuleDef evk4_extension_definition = {
    PyModuleDef_HEAD_INIT,
    "evk4_extension",
    "evk4_extension reads events from a Prophesee Gen 4 dev kit 1.3 (Denebola)",
    -1,
    evk4_extension_methods};
PyMODINIT_FUNC PyInit_evk4_extension() {
    auto module = PyModule_Create(&evk4_extension_definition);
    import_array();
    camera_type.tp_name = "evk4_extension.Camera";
    camera_type.tp_basicsize = sizeof(camera);
    camera_type.tp_dealloc = camera_dealloc;
    camera_type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    camera_type.tp_methods = camera_methods;
    camera_type.tp_members = camera_members;
    camera_type.tp_new = camera_new;
    camera_type.tp_init = camera_init;
    PyType_Ready(&camera_type);
    PyModule_AddObject(module, "Camera", (PyObject*)&camera_type);
    return module;
}
