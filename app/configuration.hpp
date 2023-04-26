#pragma once

#include "../common/evk4.hpp"
#include "../common/psee413.hpp"
#include "json.hpp"
#include <filesystem>
#include <optional>

namespace gen4 {
    struct configuration {
        std::string recordings;
        std::optional<std::string> serial;
        sepia::evk4::parameters evk4_parameters;
        sepia::psee413::parameters psee413_parameters;

        static configuration from_path(const std::filesystem::path& path) {
            auto input = sepia::filename_to_ifstream(path.string());
            const auto data = nlohmann::json::parse(*input);
            configuration result;
            result.recordings = data["recordings"];
            if (!data["serial"].is_null()) {
                result.serial = data["serial"];
            }
            result.evk4_parameters.biases.pr = data["evk4"]["biases"]["pr"];
            result.evk4_parameters.biases.fo = data["evk4"]["biases"]["fo"];
            result.evk4_parameters.biases.hpf = data["evk4"]["biases"]["hpf"];
            result.evk4_parameters.biases.diff_on = data["evk4"]["biases"]["diff_on"];
            result.evk4_parameters.biases.diff = data["evk4"]["biases"]["diff"];
            result.evk4_parameters.biases.diff_off = data["evk4"]["biases"]["diff_off"];
            result.evk4_parameters.biases.inv = data["evk4"]["biases"]["inv"];
            result.evk4_parameters.biases.refr = data["evk4"]["biases"]["refr"];
            result.evk4_parameters.biases.reqpuy = data["evk4"]["biases"]["reqpuy"];
            result.evk4_parameters.biases.reqpux = data["evk4"]["biases"]["reqpux"];
            result.evk4_parameters.biases.sendreqpdy = data["evk4"]["biases"]["sendreqpdy"];
            result.evk4_parameters.biases.unknown_1 = data["evk4"]["biases"]["unknown_1"];
            result.evk4_parameters.biases.unknown_2 = data["evk4"]["biases"]["unknown_2"];
            result.psee413_parameters.biases.pr = data["psee413"]["biases"]["pr"];
            result.psee413_parameters.biases.fo_p = data["psee413"]["biases"]["fo_p"];
            result.psee413_parameters.biases.fo_n = data["psee413"]["biases"]["fo_n"];
            result.psee413_parameters.biases.hpf = data["psee413"]["biases"]["hpf"];
            result.psee413_parameters.biases.diff_on = data["psee413"]["biases"]["diff_on"];
            result.psee413_parameters.biases.diff = data["psee413"]["biases"]["diff"];
            result.psee413_parameters.biases.diff_off = data["psee413"]["biases"]["diff_off"];
            result.psee413_parameters.biases.refr = data["psee413"]["biases"]["refr"];
            result.psee413_parameters.biases.reqpuy = data["psee413"]["biases"]["reqpuy"];
            result.psee413_parameters.biases.blk = data["psee413"]["biases"]["blk"];
            return result;
        }
    };
}
