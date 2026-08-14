#include "tdx/formulas.hpp"
#include "tdx/formulas_extractor.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

const tdx::Json& formula(const tdx::Json& document, const std::string& code) {
    for (const auto& item : document.at("formulas").as_array())
        if (item.at("code").as_string() == code) return item;
    throw tdx::Error("formula not found in fixture DLL: " + code);
}

void put_u16(tdx::Bytes& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void put_u32(tdx::Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        bytes[offset + static_cast<std::size_t>(shift / 8)] =
            static_cast<std::uint8_t>(value >> shift);
}

void put_text(tdx::Bytes& bytes, std::size_t offset, std::string_view value) {
    for (std::size_t index = 0; index < value.size(); ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value[index]);
}

fs::path write_user_formula_fixture() {
    constexpr std::size_t header_size = 53;
    constexpr std::size_t index_size = 10;
    constexpr std::size_t record_size = 5072;
    constexpr std::size_t record_offset = header_size + index_size;
    tdx::Bytes bytes(record_offset + record_size, 0);
    bytes[0] = 5;
    put_u32(bytes, 1, 1);
    put_u32(bytes, 5, static_cast<std::uint32_t>(record_offset));
    put_u32(bytes, 9, static_cast<std::uint32_t>(bytes.size()));
    put_u32(bytes, 13, 0);
    put_u16(bytes, 19, 1);  // selection kind count
    put_u16(bytes, record_offset, 107);
    bytes[record_offset + 2] = 1;
    put_text(bytes, record_offset + 3, "TESTUSER");
    put_text(bytes, record_offset + 17, "Fixture user formula");
    bytes[record_offset + 67] = 5;
    bytes[record_offset + 2185] = 1;
    put_text(bytes, record_offset + 2186, "SELECT");
    put_u32(bytes, record_offset + 5068, 2);
    const auto path = fs::temp_directory_path() /
        "tdx-formulas-tests-prigs-v5.dat";
    tdx::atomic_write_bytes(path, bytes);
    return path;
}

}  // namespace

int main() {
    try {
        const auto dll = fs::path(TDX_TEST_ROOT) / "ida" / "TCalc.dll";
        const auto document = tdx::extract_formulas_document(dll, "all");
        require(document.at("schema_version").as_number() == 4,
                "formula source schema version");
        require(document.at("formulas").size() == 379, "all formula count");
        require(document.at("source_text_count").as_number() == 379 &&
                    document.at("embedded_source_text_count").as_number() == 361 &&
                    document.at("recovered_source_text_count").as_number() == 18,
                "recoverable source-text count");
        require(document.at("source_text_count").as_number() == 379 &&
                    document.at("source_text_missing_count").as_number() == 0,
                "unavailable source-text count");

        const auto& dmi = formula(document, "DMI");
        require(dmi.at("source_text_available").as_bool(), "DMI source availability");
        const auto dmi_text = dmi.at("source_text").as_string();
        require(dmi_text.find("MTR:=SUM(MAX(MAX(HIGH-LOW") != std::string::npos &&
                    dmi_text.find("ADXR:(ADX+REF(ADX,M))/2") != std::string::npos,
                "DMI source body");
        require(dmi.at("source_rva").as_number() == 0x1156D0,
                "DMI source RVA");

        const auto& dkx = formula(document, "DKX");
        require(dkx.at("source_text").as_string().find("REF(MID,20))/210") !=
                    std::string::npos,
                "DKX system formula preserves the offset-20 term");
        const auto& asi = formula(document, "ASI");
        require(asi.at("source_text_available").as_bool() &&
                    asi.at("source_text_origin").as_string() == "native-recovered" &&
                    asi.at("source_rva").is_null(),
                "recovered ASI source provenance");
        const auto& kdj_tdx = formula(document, "KDJ-TDX");
        require(kdj_tdx.at("source_text_origin").as_string() == "native-recovered" &&
                    kdj_tdx.at("source_rva").is_null() &&
                    kdj_tdx.at("source_text").as_string().find("TDXKDJ") != std::string::npos,
                "KDJ-TDX recovered native body provenance");
        const auto& vty = formula(document, "VTY");
        require(vty.at("source_text_origin").as_string() == "native-recovered" &&
                    vty.at("source_text").as_string().find("TDXVTY") != std::string::npos,
                "VTY recovered native body provenance");
        const auto& msi = formula(document, "MSI");
        require(msi.at("source_text_origin").as_string() == "native-recovered" &&
                    msi.at("source_text").as_string().find("TDXMSI") != std::string::npos,
                "MSI recovered native body provenance");
        const auto& mcst = formula(document, "MCST");
        require(mcst.at("source_text_origin").as_string() == "native-recovered" &&
                    mcst.at("source_text").as_string().find("TDXMCST") != std::string::npos,
                "MCST recovered native body provenance");
        const auto& ssrp = formula(document, "SSRP");
        require(ssrp.at("source_text_origin").as_string() == "native-recovered" &&
                    ssrp.at("source_text").as_string().find("TDXSSRP") != std::string::npos,
                "SSRP recovered native body provenance");
        const auto& pav = formula(document, "PAV");
        const auto& pave = formula(document, "PAVE");
        require(pav.at("source_text_origin").as_string() == "native-recovered" &&
                    pav.at("source_text").as_string().find("TDXPAV") != std::string::npos &&
                    pave.at("source_text_origin").as_string() == "native-recovered" &&
                    pave.at("source_text").as_string().find("TDXPAVE") != std::string::npos,
                "PAV/PAVE recovered native body provenance");
        const auto& ndb = formula(document, "NDB");
        require(ndb.at("source_text_origin").as_string() == "native-recovered" &&
                    ndb.at("source_text").as_string().find("TDXNDB") != std::string::npos,
                "NDB recovered native body provenance");
        const auto& sc = formula(document, "SC");
        require(sc.at("source_text_origin").as_string() == "native-recovered" &&
                    sc.at("source_text").as_string().find("TDXSC") != std::string::npos,
                "SC recovered native body provenance");
        const auto& xlpl = formula(document, "XLPL");
        require(xlpl.at("source_text_origin").as_string() == "native-recovered" &&
                    xlpl.at("source_text").as_string().find("TDXXLPLBASE") != std::string::npos &&
                    xlpl.at("source_text").as_string().find("BACKSET") != std::string::npos,
                "XLPL recovered native body provenance");
        const auto& zxnh = formula(document, "ZXNH");
        require(zxnh.at("source_text_origin").as_string() == "native-recovered" &&
                    zxnh.at("source_text").as_string().find("TDXZXNH") != std::string::npos,
                "ZXNH recovered native body provenance");

        const auto user_fixture = write_user_formula_fixture();
        const auto user_document =
            tdx::extract_user_formulas_document(user_fixture, "all");
        require(user_document.at("schema").as_string() ==
                    "tdx-formula-user-library-v1" &&
                    user_document.at("formulas").size() == 1 &&
                    user_document.at("private_user_data").as_bool(),
                "PriGS user-library envelope");
        const auto& test_user = formula(user_document, "TESTUSER");
        require(test_user.at("kind_key").as_string() == "selection" &&
                    test_user.at("index").as_number() == 107 &&
                    test_user.at("is_custom").as_bool() &&
                    test_user.at("source_text_origin").as_string() ==
                        "unavailable" &&
                    test_user.at("outputs").as_array().front().as_string() ==
                        "SELECT" &&
                    user_document.at("native")
                            .at("formula_record_size_bytes").as_number() ==
                        5072 &&
                    user_document.at("native")
                            .at("network_requests").as_number() == 0,
                "PriGS v5 fixed record and read-only boundary");
        fs::remove(user_fixture);

        const auto icons = tdx::extract_formula_icon_sprite(dll);
        require(icons.bitmap.size() == 97254 && icons.bitmap[0] == 'B' &&
                    icons.bitmap[1] == 'M' && icons.png.size() > 100 &&
                    icons.png[0] == 0x89 && icons.png[1] == 'P' &&
                    icons.manifest.at("schema").as_string() ==
                        "tdx-formula-icon-sprite-v1" &&
                    icons.manifest.at("resource_id").as_number() == 2060 &&
                    icons.manifest.at("width").as_number() == 1800 &&
                    icons.manifest.at("height").as_number() == 18 &&
                    icons.manifest.at("cell_count").as_number() == 100 &&
                    icons.manifest.at("official_type_max").as_number() == 51,
                "TCalc RT_BITMAP/2060 is recovered as the native DRAWICON sprite");
        std::cout << "formula source extraction tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
