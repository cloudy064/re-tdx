#include "tdx/recon.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

int command_doctor(const std::vector<std::string>& values) {
    Args args(values);
    const std::string root_name = args.take_option("--root");
    const bool json = args.take_flag("--json");
    args.require_empty();
    Json report = Json::object();
    bool ok = true;
    try {
        const fs::path root = find_tdx_root(
            root_name.empty() ? fs::path{} : fs::u8path(root_name));
        report["tdx_root"] = path_utf8(root);
        report["tdxw"] = fs::exists(root / "TdxW.exe") || fs::exists(root / "tdxw.exe");
        report["cloud_cfg"] = fs::is_directory(root / "T0002" / "cloud_cfg");
        report["vipdoc"] = fs::is_directory(root / "vipdoc");
    } catch (const std::exception& error) {
        ok = false;
        report["error"] = error.what();
    }
    report["native"] = true;
    report["python_runtime"] = false;
    report["ok"] = ok;
    if (json) {
        std::cout << report.dump(2) << '\n';
    } else {
        std::cout << "tdx-tool native doctor: " << (ok ? "OK" : "FAILED") << '\n';
        if (ok) std::cout << "TDX root: " << report.at("tdx_root").as_string() << '\n';
        else std::cout << report.at("error").as_string() << '\n';
        std::cout << "Python runtime: not used\n";
    }
    return ok ? 0 : 1;
}

}  // namespace tdx
