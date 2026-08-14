#include "tdx/common.hpp"
#include "tdx/formulas_extractor.hpp"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void help() {
    std::cout <<
        "tdx-formula-extractor - optional offline TCalc.dll evidence tool\n\n"
        "Usage:\n"
        "  tdx-formula-extractor extract [options]\n"
        "  tdx-formula-extractor icons [options]\n\n"
        "This executable parses a DLL as inert bytes and is not required by tdx-tool.\n";
}

int run(const std::vector<std::string>& argv) {
    try {
        if (argv.size() < 2 || argv[1] == "--help" || argv[1] == "help") {
            help();
            return 0;
        }
        std::vector<std::string> arguments;
        for (std::size_t index = 2; index < argv.size(); ++index)
            arguments.push_back(argv[index]);
        if (argv[1] == "extract")
            return tdx::command_tcalc_formulas_extract(arguments);
        if (argv[1] == "icons")
            return tdx::command_tcalc_formulas_icons(arguments);
        throw tdx::Error("unknown extractor command: " + argv[1]);
    } catch (const std::exception& error) {
        std::cerr << "tdx-formula-extractor: " << error.what() << '\n';
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
    for (int index = 0; index < argc; ++index)
        arguments.push_back(tdx::wide_to_utf8(argv[index]));
    return run(arguments);
}
#else
int main(int argc, char** argv) {
    return run(std::vector<std::string>(argv, argv + argc));
}
#endif
