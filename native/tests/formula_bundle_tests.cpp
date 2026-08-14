#include "tdx/formula_engine.hpp"
#include "tdx/formulas.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json sample(std::size_t count) {
    tdx::Json document = tdx::Json::object();
    document["market"] = "sz";
    document["code"] = "000001";
    document["period"] = "day";
    tdx::Json bars = tdx::Json::array();
    for (std::size_t index = 0; index < count; ++index) {
        const double close = 10.0 + static_cast<double>(index);
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-08-" +
            std::string(index + 1 < 10 ? "0" : "") +
            std::to_string(index + 1);
        bar["time"] = "15:00";
        bar["open"] = close - 0.5;
        bar["high"] = close + 1.0;
        bar["low"] = close - 1.0;
        bar["close"] = close;
        bar["amount"] = close * 100000.0;
        bar["volume"] = 100000.0;
        bars.push_back(std::move(bar));
    }
    document["bars"] = std::move(bars);
    return document;
}

const tdx::Json& formula_by_code(const tdx::Json& library,
                                 const std::string& code) {
    for (const auto& formula : library.at("formulas").as_array())
        if (formula.at("code").as_string() == code) return formula;
    throw tdx::Error("bundled formula not found: " + code);
}

}  // namespace

int main() {
    try {
        const auto directory = tdx::locate_formula_bundle();
        require(fs::is_regular_file(directory / "bundle-manifest.json"),
                "bundle directory resolution");

        const auto library = tdx::load_bundled_formula_library_document(directory);
        require(library.at("formulas").size() == 379,
                "bundled formula count");
        require(library.at("source_text_count").as_number() == 379.0 &&
                    library.at("source_text_missing_count").as_number() == 0.0 &&
                    library.at("recovered_source_text_count").as_number() == 18.0,
                "bundled source-text count");
        require(library.at("runtime_library_origin").as_string() ==
                    "bundled-snapshot" &&
                !library.at("runtime_dll_accessed").as_bool() &&
                !library.at("runtime_dll_loaded").as_bool() &&
                library.at("runtime_network_requests").as_number() == 0.0,
                "bundled library has no runtime DLL/network dependency");

        const auto& ma = formula_by_code(library, "MA");
        require(ma.at("source_text").is_string(),
                "bundled MA source is available");
        const auto evaluation = tdx::evaluate_formula_document(
            sample(20), ma, {});
        require(evaluation.at("engine").as_string() ==
                    "tdx-source-interpreter-v1" &&
                evaluation.at("count").as_number() == 20.0,
                "bundled formula executes through the native interpreter");

        const auto icons = tdx::load_bundled_formula_icon_sprite(directory);
        require(icons.bitmap.size() == 97254 && icons.png.size() == 34990,
                "bundled icon byte counts");
        require(icons.bitmap.size() >= 2 && icons.bitmap[0] == 'B' &&
                    icons.bitmap[1] == 'M',
                "bundled bitmap signature");
        require(icons.png.size() >= 8 && icons.png[0] == 0x89 &&
                    icons.png[1] == 'P' && icons.png[2] == 'N' &&
                    icons.png[3] == 'G',
                "bundled PNG signature");
        require(!icons.manifest.at("runtime_dll_accessed").as_bool() &&
                    icons.manifest.at("runtime_asset_origin").as_string() ==
                        "bundled-snapshot",
                "bundled icons have no runtime DLL dependency");

        const auto root_without_dll = fs::temp_directory_path() /
            "tdx-formula-bundle-no-dll-root";
        const auto with_missing_user =
            tdx::load_bundled_installed_formula_library_document(
                root_without_dll, true, directory);
        require(with_missing_user.at("formulas").size() == 379 &&
                    with_missing_user.at("private_user_data").as_bool() &&
                    !with_missing_user.at("user_library").at("file_exists").as_bool(),
                "missing optional PriGS does not reintroduce a TCalc dependency");

        bool rejected = false;
        try {
            (void)tdx::locate_formula_bundle(
                root_without_dll / "missing-formula-assets");
        } catch (const tdx::Error&) {
            rejected = true;
        }
        require(rejected, "invalid explicit bundle directory is rejected");

        std::cout << "standalone formula bundle tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
