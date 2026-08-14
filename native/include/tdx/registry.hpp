#pragma once

#include <functional>
#include <string>
#include <vector>

namespace tdx {

using CommandHandler = std::function<int(const std::vector<std::string>&)>;

struct CommandSpec {
    std::string name;
    std::string title;
    std::string category;
    std::string description;
    bool uses_network{};
    std::string api_endpoint;
    CommandHandler handler;
};

const std::vector<CommandSpec>& command_registry();
const CommandSpec* find_command(const std::string& name);

}  // namespace tdx
