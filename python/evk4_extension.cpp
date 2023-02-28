#define NOMINMAX
#include "date.hpp"
#include <Python.h>
#include <cstring>
#include <sstream>
#include <structmember.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#if defined(HAVE_SSIZE_T)
#define _SSIZE_T_DEFINED
#endif
#include "third_party/roo/psee413.hpp"
#include <filesystem>
#include <numpy/arrayobject.h>

static std::string now() {
    return date::format("%FT%TZ", date::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
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

struct slice {
    uint64_t end_t;
    std::vector<float> on_events;
    std::vector<float> off_events;
};

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
    std::unique_ptr<std::ofstream> jsonl_log;
    std::vector<slice> slices;
    std::size_t active_slice_index;
    std::unique_ptr<sepia::psee413::base_camera> base_camera;
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
        auto parameters = sepia::psee413::default_parameters;
        parameters.biases.pr = read_bias(biases_dict, "pr");
        parameters.biases.fo_p = read_bias(biases_dict, "fo_p");
        parameters.biases.fo_n = read_bias(biases_dict, "fo_n");
        parameters.biases.hpf = read_bias(biases_dict, "hpf");
        parameters.biases.diff_on = read_bias(biases_dict, "diff_on");
        parameters.biases.diff = read_bias(biases_dict, "diff");
        parameters.biases.diff_off = read_bias(biases_dict, "diff_off");
        parameters.biases.refr = read_bias(biases_dict, "refr");
        parameters.biases.reqpuy = read_bias(biases_dict, "reqpuy");
        parameters.biases.blk = read_bias(biases_dict, "blk");
        current->data->base_camera->update_parameters(parameters);
        Py_RETURN_NONE;
    } catch (const std::exception& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
        return nullptr;
    }
    return nullptr;
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
    std::size_t file_duration;
    std::size_t file_size;
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

static PyArrayObject* check_events(PyObject* object) {
    if (!PyArray_Check(object)) {
        throw std::runtime_error("events must be a numpy array");
    }
    auto events = reinterpret_cast<PyArrayObject*>(object);
    if (PyArray_NDIM(events) != 1) {
        throw std::runtime_error("events must be a 1-dimensional array");
    }
    if (!PyArray_ISONESEGMENT(events)) {
        throw std::runtime_error("events' memory must be contiguous");
    }
    return events;
}

static PyArrayObject* check_counts(PyObject* object) {
    if (!PyArray_Check(object)) {
        throw std::runtime_error("counts must be a numpy array");
    }
    auto counts = reinterpret_cast<PyArrayObject*>(object);
    if (PyArray_NDIM(counts) != 1) {
        throw std::runtime_error("counts must be a 1-dimensional array");
    }
    if (*PyArray_SHAPE(counts) != 5) {
        throw std::runtime_error("counts must have 5 elements");
    }
    if (!PyArray_ISONESEGMENT(counts)) {
        throw std::runtime_error("counts' memory must be contiguous");
    }
    auto description = PyArray_DESCR(counts);
    if (description->kind != 'u' || description->elsize != 8) {
        throw std::runtime_error("counts' dtype must be numpy.uint64");
    }
    return counts;
}

static PyObject* update_points(PyObject* self, PyObject* args) {
    PyObject* on_events_object;
    PyObject* off_events_object;
    PyObject* counts_object;
    if (!PyArg_ParseTuple(args, "OOO", &on_events_object, &off_events_object, &counts_object)) {
        return nullptr;
    }
    auto current = reinterpret_cast<camera*>(self);
    auto data = current->data;
    try {
        auto on_events = check_events(on_events_object);
        auto off_events = check_events(off_events_object);
        auto counts = check_counts(counts_object);
        const auto slices_count = static_cast<std::size_t>(*reinterpret_cast<uint64_t*>(PyArray_GETPTR1(counts, 0)));
        if (slices_count >= data->slices.size()) {
            throw std::runtime_error(
                "the number of selected slices must be strictly smaller than the total number of slices");
        }
        npy_intp on_events_size = 0;
        npy_intp off_events_size = 0;
        auto begin_t_set = false;
        uint32_t begin_t = std::numeric_limits<uint32_t>::max();
        uint32_t end_t = 0;
        std::exception_ptr exception;
        while (data->accessing_camera.test_and_set(std::memory_order_acquire)) {
        }
        if (current->data->exception) {
            exception = current->data->exception;
        } else {
            for (std::size_t index =
                     (data->active_slice_index + data->slices.size() - slices_count) % data->slices.size();
                 index != data->active_slice_index;
                 index = (index + 1) % data->slices.size()) {
                on_events_size += static_cast<npy_intp>(data->slices[index].on_events.size());
                off_events_size += static_cast<npy_intp>(data->slices[index].off_events.size());
                if (!begin_t_set) {
                    if (data->slices[index].on_events.size() >= 3) {
                        begin_t = static_cast<uint32_t>(data->slices[index].on_events[2]);
                        begin_t_set = true;
                    }
                    if (data->slices[index].off_events.size() >= 3) {
                        begin_t = std::min(begin_t, static_cast<uint32_t>(data->slices[index].off_events[2]));
                        begin_t_set = true;
                    }
                }
                end_t = static_cast<uint32_t>(data->slices[index].end_t);
            }
            *reinterpret_cast<uint64_t*>(PyArray_GETPTR1(counts, 1)) =
                std::min(static_cast<uint64_t>(on_events_size / 3), static_cast<uint64_t>(*PyArray_SHAPE(on_events)));
            *reinterpret_cast<uint64_t*>(PyArray_GETPTR1(counts, 2)) =
                std::min(static_cast<uint64_t>(off_events_size / 3), static_cast<uint64_t>(*PyArray_SHAPE(off_events)));
            *reinterpret_cast<uint64_t*>(PyArray_GETPTR1(counts, 3)) = (begin_t & 0xffffffffu);
            *reinterpret_cast<uint64_t*>(PyArray_GETPTR1(counts, 4)) = (end_t & 0xffffffffu);
            std::size_t on_to_skip = 0;
            if ((*PyArray_SHAPE(on_events)) < on_events_size / 3) {
                on_to_skip = (on_events_size / 3 - (*PyArray_SHAPE(on_events))) * 3;
            }
            std::size_t off_to_skip = 0;
            if ((*PyArray_SHAPE(off_events)) < off_events_size / 3) {
                off_to_skip = (off_events_size / 3 - (*PyArray_SHAPE(off_events))) * 3;
            }
            auto on_events_data = reinterpret_cast<float*>(PyArray_DATA(on_events));
            auto off_events_data = reinterpret_cast<float*>(PyArray_DATA(off_events));
            std::size_t on_offset = 0;
            std::size_t off_offset = 0;
            for (std::size_t index =
                     (data->active_slice_index + data->slices.size() - slices_count) % data->slices.size();
                 index != data->active_slice_index;
                 index = (index + 1) % data->slices.size()) {
                if (on_to_skip >= data->slices[index].on_events.size()) {
                    on_to_skip -= data->slices[index].on_events.size();
                } else if (on_to_skip > 0) {
                    std::memcpy(
                        on_events_data + on_offset,
                        data->slices[index].on_events.data() + on_to_skip,
                        (data->slices[index].on_events.size() - on_to_skip) * sizeof(float));
                    on_offset += (data->slices[index].on_events.size() - on_to_skip);
                    on_to_skip = 0;
                } else {
                    std::memcpy(
                        on_events_data + on_offset,
                        data->slices[index].on_events.data(),
                        data->slices[index].on_events.size() * sizeof(float));
                    on_offset += data->slices[index].on_events.size();
                }
                if (off_to_skip >= data->slices[index].off_events.size()) {
                    off_to_skip -= data->slices[index].off_events.size();
                } else if (off_to_skip > 0) {
                    std::memcpy(
                        off_events_data + off_offset,
                        data->slices[index].off_events.data() + off_to_skip,
                        (data->slices[index].off_events.size() - off_to_skip) * sizeof(float));
                    off_offset += (data->slices[index].off_events.size() - off_to_skip);
                    off_to_skip = 0;
                } else {
                    std::memcpy(
                        off_events_data + off_offset,
                        data->slices[index].off_events.data(),
                        data->slices[index].off_events.size() * sizeof(float));
                    off_offset += data->slices[index].off_events.size();
                }
            }
        }
        current->data->accessing_camera.clear(std::memory_order_release);
        if (exception) {
            std::rethrow_exception(exception);
        }
        Py_RETURN_NONE;
    } catch (const std::exception& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
        return nullptr;
    }
    return nullptr;
}

static PyMethodDef camera_methods[] = {
    {"set_parameters", set_parameters, METH_VARARGS, nullptr},
    {"record_to", record_to, METH_VARARGS, nullptr},
    {"recording_status", recording_status, METH_NOARGS, nullptr},
    {"update_points", update_points, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};
static int camera_init(PyObject* self, PyObject* args, PyObject*) {
    auto current = reinterpret_cast<camera*>(self);
    PyObject* recordings_path;
    PyObject* log_path;
    uint64_t slice_duration;
    uint64_t slices_count;
    uint64_t slice_initial_capacity;
    if (!PyArg_ParseTuple(
            args, "OOKKK", &recordings_path, &log_path, &slice_duration, &slices_count, &slice_initial_capacity)) {
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
        data->jsonl_log.reset(
            new std::ofstream(python_path_to_string(log_path), std::ios::binary | std::ios::app | std::ios::out));
        data->slices = std::vector<slice>(slices_count);
        for (auto& slice : data->slices) {
            slice.on_events.reserve(slice_initial_capacity * 3);
            slice.off_events.reserve(slice_initial_capacity * 3);
        }
        data->active_slice_index = 0;
        data->slices[data->active_slice_index].end_t = slice_duration;
        data->base_camera = sepia::psee413::make_camera(
            [slice_duration, slices_count, data](sepia::dvs_event event) {
                if (event.t >= data->slices[data->active_slice_index].end_t) {
                    while (data->accessing_camera.test_and_set(std::memory_order_acquire)) {
                    }
                    while (event.t >= data->slices[data->active_slice_index].end_t) {
                        const auto end_t = data->slices[data->active_slice_index].end_t;
                        data->active_slice_index = (data->active_slice_index + 1) % slices_count;
                        data->slices[data->active_slice_index].end_t = end_t + slice_duration;
                        data->slices[data->active_slice_index].on_events.clear();
                        data->slices[data->active_slice_index].off_events.clear();
                    }
                    data->accessing_camera.clear(std::memory_order_release);
                }
                if (event.on) {
                    data->slices[data->active_slice_index].on_events.push_back(static_cast<float>(event.x));
                    data->slices[data->active_slice_index].on_events.push_back(static_cast<float>(event.y));
                    data->slices[data->active_slice_index].on_events.push_back(static_cast<float>(event.t & 0xffffffffu));
                } else {
                    data->slices[data->active_slice_index].off_events.push_back(static_cast<float>(event.x));
                    data->slices[data->active_slice_index].off_events.push_back(static_cast<float>(event.y));
                    data->slices[data->active_slice_index].off_events.push_back(static_cast<float>(event.t & 0xffffffffu));
                }
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
            [](sepia::psee413::trigger_event) {},
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
                        sepia::filename_to_ofstream(data->file_name), sepia::psee413::width, sepia::psee413::height);
                    const auto utc = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                                                        std::chrono::system_clock::now().time_since_epoch())
                                                        .count());
                    std::stringstream message;
                    message << "{\"timestamp\":" << now() << ",\"type\":\"clap\",\"utc\":" << utc << ",\"filename\":\""
                            << data->file_name << "\"}\n";
                    const std::string message_string = message.str();
                    data->jsonl_log->write(message_string.data(), message_string.size());
                    data->jsonl_log->flush();
                }
                data->accessing_camera.clear(std::memory_order_release);
            },
            [=](std::exception_ptr exception) {
                while (data->accessing_camera.test_and_set(std::memory_order_acquire)) {
                }
                data->exception = exception;
                data->accessing_camera.clear(std::memory_order_release);
            },
            sepia::psee413::default_parameters,
            "",
            std::chrono::milliseconds(100),
            [=](std::size_t dropped_bytes) {
                std::stringstream message;
                message << "{\"timestamp\":" << now() << ",\"type\":\"drop\",\"bytes\":" << dropped_bytes << "}\n";
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

static PyMethodDef psee413_3d_extension_methods[] = {{nullptr, nullptr, 0, nullptr}};
static struct PyModuleDef psee413_3d_extension_definition = {
    PyModuleDef_HEAD_INIT,
    "psee413_3d_extension",
    "psee413_3d_extension reads events from a Prophesee Gen 4 dev kit 1.3 (Denebola)",
    -1,
    psee413_3d_extension_methods};
PyMODINIT_FUNC PyInit_psee413_3d_extension() {
    auto module = PyModule_Create(&psee413_3d_extension_definition);
    import_array();
    camera_type.tp_name = "psee413_3d_extension.Camera";
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
