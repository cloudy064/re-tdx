#include "tdx/common.hpp"
#include "tdx/registry.hpp"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
void help() {
    std::cout <<
        "tdx-tool 0.2.0 - pure C++ TongDaXin research toolkit\n\n"
        "Usage:\n"
        "  tdx-tool <command> [options]\n"
        "  tdx-tool help [command]\n\n"
        "Implemented native commands:\n";
    for (const auto& item : tdx::command_registry())
        std::cout << "  " << item.name
                  << std::string(item.name.size() < 22 ? 22 - item.name.size() : 1, ' ')
                  << item.title << '\n';
    std::cout << "\nNo Python interpreter or script forwarding is used.\n";
}

std::vector<std::string> tail(const std::vector<std::string>& argv, std::size_t begin) {
    std::vector<std::string> result;
    for (std::size_t index = begin; index < argv.size(); ++index) result.push_back(argv[index]);
    return result;
}

int run(const std::vector<std::string>& argv) {
    try {
        if (argv.size() < 2 || argv[1] == "--help" || argv[1] == "help") {
            if (argv.size() >= 3) {
                std::string key = argv[2];
                if (argv.size() >= 4) key += " " + argv[3];
                if (const auto* command = tdx::find_command(key))
                    return command->handler({"--help"});
            }
            help();
            return 0;
        }
        if (argv[1] == "--version" || argv[1] == "version") {
            std::cout << "tdx-tool 0.2.0 native-cpp\n";
            return 0;
        }
        std::string key = argv[1];
        std::size_t argument_begin = 2;
        if (argv.size() >= 3) {
            const std::string nested = key + " " + argv[2];
            if (tdx::find_command(nested)) {
                key = nested;
                argument_begin = 3;
            }
        }
        const auto* command = tdx::find_command(key);
        if (!command) {
            throw tdx::Error("unknown or not-yet-native command: " + key);
        }
        return command->handler(tail(argv, argument_begin));
    } catch (const std::exception& error) {
        std::cerr << "tdx-tool: " << error.what() << '\n';
        return 2;
    }
}
}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    try {
        for (int index = 0; index < argc; ++index)
            arguments.push_back(tdx::wide_to_utf8(argv[index]));
        return run(arguments);
    } catch (const std::exception& error) {
        std::cerr << "tdx-tool: " << error.what() << '\n';
        return 2;
    }
}
#else
int main(int argc, char** argv) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) arguments.emplace_back(argv[index]);
    return run(arguments);
}
#endif
