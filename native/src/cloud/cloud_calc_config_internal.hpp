#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::cloud_calc_detail {

struct Column {
    std::string code;
    std::string name;
    std::string datatype;
    std::string calc;
    std::vector<std::string> refs;
    std::string syscol;
    std::string refzqdm;
    int calctype{};
    int calcflag{};
    std::size_t ordinal{};
};

struct Unit {
    std::string id;
    std::string file;
    std::string refunit;
    std::vector<Column> columns;
};

struct Config {
    std::filesystem::path path;
    std::vector<Unit> units;
};

bool parse_double(std::string_view source, double& value);
int parse_int(std::string_view source, int fallback = 0);

Config parse_config(const std::filesystem::path& path);
std::vector<std::filesystem::path> config_paths(
    const std::filesystem::path& root_or_cfg);

}  // namespace tdx::cloud_calc_detail
