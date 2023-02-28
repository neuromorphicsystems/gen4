#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// pontella is a command  line parser.
namespace pontella {

    /// command contains parsed arguments, options and flags.
    struct command {
        /// arguments contains the positionnal arguments given to the program.
        std::vector<std::string> arguments;

        /// options contains the named options and the associated parameter given to the program.
        std::unordered_map<std::string, std::string> options;

        /// flags contains the named flags given to the program.
        std::unordered_set<std::string> flags;
    };

    /// label represents an option or flag name, and its aliases.
    struct label {
        std::string name;
        std::unordered_set<std::string> aliases;
    };

    /// validate throws if the given string does not match the criterions for being an option or flag.
    inline void validate(const std::string& name_or_alias, bool is_option, bool is_name) {
        if (name_or_alias.empty()) {
            throw std::logic_error(
                std::string(is_option ? "An option" : "A flag") + " " + (is_name ? "name " : "alias") + " is empty");
        }
        const auto prefix = std::string("The ") + (is_option ? "option" : "flag") + " " + (is_name ? "name" : "alias")
                            + " '" + name_or_alias + "' ";
        if (name_or_alias[0] == '-') {
            throw std::logic_error(prefix + "starts with the charcater '-'");
        }
        for (auto character : name_or_alias) {
            if (isspace(character)) {
                throw std::logic_error(prefix + "contains white-space characters");
            }
            if (character == '=') {
                throw std::logic_error(prefix + "contains the character '='");
            }
        }
    }

    /// parse turns argc and argv into parsed arguments and options.
    /// If number_of_arguments is negative, the number of arguments is unlimited.
    template <typename OptionIterator, typename FlagIterator>
    inline command parse(
        int argc,
        char* argv[],
        int64_t number_of_arguments,
        OptionIterator options_begin,
        OptionIterator options_end,
        FlagIterator flags_begin,
        FlagIterator flags_end) {
        std::unordered_map<std::string, bool> name_to_is_option;
        std::unordered_map<std::string, std::string> alias_to_name;
        for (; options_begin != options_end; ++options_begin) {
            validate(options_begin->name, true, true);
            if (!name_to_is_option.insert(std::make_pair(options_begin->name, true)).second) {
                throw std::logic_error("Duplicated name '" + options_begin->name + "'");
            }
            for (const auto& alias : options_begin->aliases) {
                validate(alias, true, false);
                if (name_to_is_option.find(alias) != name_to_is_option.end()) {
                    throw std::logic_error("Duplicated name and alias '" + alias + "'");
                }
                if (!alias_to_name.insert(std::make_pair(alias, options_begin->name)).second) {
                    throw std::logic_error("Duplicated alias '" + alias + "'");
                }
            }
        }
        for (; flags_begin != flags_end; ++flags_begin) {
            validate(flags_begin->name, false, true);
            if (!name_to_is_option.insert(std::make_pair(flags_begin->name, false)).second) {
                throw std::logic_error("Duplicated name '" + flags_begin->name + "'");
            }
            for (const auto& alias : flags_begin->aliases) {
                validate(alias, false, false);
                if (name_to_is_option.find(alias) != name_to_is_option.end()) {
                    throw std::logic_error("Duplicated name and alias '" + alias + "'");
                }
                if (!alias_to_name.insert(std::make_pair(alias, flags_begin->name)).second) {
                    throw std::logic_error("Duplicated alias '" + alias + "'");
                }
            }
        }
        command command;
        for (auto index = 1; index < argc; ++index) {
            const std::string element(argv[index]);
            if (element[0] == '-') {
                std::string name_or_alias_and_parameter;
                if (element.size() == 1) {
                    throw std::runtime_error("Unexpected character '-' without an associated name or alias");
                } else {
                    if (element[1] == '-') {
                        if (element.size() == 2) {
                            throw std::runtime_error("Unexpected characters '--' without an associated name or alias");
                        } else {
                            name_or_alias_and_parameter = element.substr(2);
                        }
                    } else {
                        name_or_alias_and_parameter = element.substr(1);
                    }
                }
                auto name_and_is_option = name_to_is_option.end();
                std::string parameter;
                auto has_equal = false;
                {
                    std::string name_or_alias;
                    for (auto character_iterator = name_or_alias_and_parameter.begin();
                         character_iterator != name_or_alias_and_parameter.end();
                         ++character_iterator) {
                        if (*character_iterator == '=') {
                            has_equal = true;
                            name_or_alias = std::string(name_or_alias_and_parameter.begin(), character_iterator);
                            parameter = std::string(std::next(character_iterator), name_or_alias_and_parameter.end());
                            break;
                        }
                    }
                    if (!has_equal) {
                        name_or_alias = name_or_alias_and_parameter;
                        if (index < argc - 1) {
                            parameter = std::string(argv[index + 1]);
                        }
                    }
                    const auto name_and_is_option_candidate = name_to_is_option.find(name_or_alias);
                    if (name_and_is_option_candidate == name_to_is_option.end()) {
                        const auto alias_and_name_candidate = alias_to_name.find(name_or_alias);
                        if (alias_and_name_candidate == alias_to_name.end()) {
                            throw std::runtime_error("Unknown option name or alias '" + name_or_alias + "'");
                        }
                        name_and_is_option = name_to_is_option.find(alias_and_name_candidate->second);
                    } else {
                        name_and_is_option = name_and_is_option_candidate;
                    }
                }

                if (name_and_is_option->second) {
                    if (!has_equal) {
                        if (index == argc - 1) {
                            throw std::runtime_error(
                                "The option '" + name_and_is_option->first + "' requires a parameter");
                        }
                        ++index;
                    }
                    command.options.insert(std::make_pair(name_and_is_option->first, parameter));
                } else {
                    if (has_equal) {
                        throw std::runtime_error(
                            "The flag '" + name_and_is_option->first + "' does not take a parameter");
                    }
                    command.flags.insert(name_and_is_option->first);
                }
            } else {
                if (number_of_arguments >= 0 && static_cast<int64_t>(command.arguments.size()) >= number_of_arguments) {
                    throw std::runtime_error(
                        "Too many arguments (" + std::to_string(number_of_arguments) + " expected)");
                }
                command.arguments.push_back(element);
            }
        }
        if (number_of_arguments >= 0 && static_cast<int64_t>(command.arguments.size()) < number_of_arguments) {
            throw std::runtime_error("Not enough arguments (" + std::to_string(number_of_arguments) + " expected)");
        }
        return command;
    }
    template <typename OptionIterator>
    inline command parse(
        int argc,
        char* argv[],
        int64_t number_of_arguments,
        OptionIterator options_begin,
        OptionIterator options_end,
        std::initializer_list<label> flags) {
        return parse(argc, argv, number_of_arguments, options_begin, options_end, flags.begin(), flags.end());
    }
    template <typename FlagIterator>
    inline command parse(
        int argc,
        char* argv[],
        int64_t number_of_arguments,
        std::initializer_list<label> options,
        FlagIterator flags_begin,
        FlagIterator flags_end) {
        return parse(argc, argv, number_of_arguments, options.begin(), options.end(), flags_begin, flags_end);
    }
    inline command parse(
        int argc,
        char* argv[],
        int64_t number_of_arguments,
        std::initializer_list<label> options,
        std::initializer_list<label> flags) {
        return parse(argc, argv, number_of_arguments, options.begin(), options.end(), flags.begin(), flags.end());
    }

    /// test determines wether the given flag was used.
    /// It can be used to hide the error message when a specific flag is present.
    inline bool test(int argc, char* argv[], const label& flag) {
        std::unordered_set<std::string> patterns;
        validate(flag.name, false, true);
        patterns.insert(std::string("-") + flag.name);
        patterns.insert(std::string("--") + flag.name);
        for (const auto& alias : flag.aliases) {
            validate(alias, false, false);
            patterns.insert(std::string("-") + alias);
            patterns.insert(std::string("--") + alias);
        }
        return std::any_of(patterns.begin(), patterns.end(), [&](const std::string& pattern) {
            for (auto index = 1; index < argc; ++index) {
                if (pattern == std::string(argv[index])) {
                    return true;
                }
            }
            return false;
        });
    }

    /// main wraps error handling and message display.
    template <typename HandleCommand>
    inline int main(
        std::initializer_list<std::string> lines,
        int argc,
        char* argv[],
        int64_t number_of_arguments,
        std::initializer_list<label> options,
        std::initializer_list<label> flags,
        HandleCommand handle_command) {
        const label help{"help", {"h"}};
        try {
            std::vector<label> flags_with_help(flags);
            flags_with_help.push_back(help);
            const auto command =
                parse(argc, argv, number_of_arguments, options, flags_with_help.begin(), flags_with_help.end());
            if (command.flags.find("help") == command.flags.end()) {
                handle_command(command);
                return 0;
            }
        } catch (const std::exception& exception) {
            if (!test(argc, argv, help)) {
                std::cerr << exception.what() << "\n";
            }
        }
        for (const auto& line : lines) {
            std::cerr << line << "\n";
        }
        return 1;
    }
}
