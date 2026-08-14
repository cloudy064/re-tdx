#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace tdx {

struct JsnTable {
    std::vector<std::string> headers;
    std::vector<Json::Array> rows;
};

class JsnIndex {
public:
    explicit JsnIndex(const std::filesystem::path& input);
    ~JsnIndex();
    JsnIndex(JsnIndex&&) noexcept;
    JsnIndex& operator=(JsnIndex&&) noexcept;
    JsnIndex(const JsnIndex&) = delete;
    JsnIndex& operator=(const JsnIndex&) = delete;

    Json query_security(const std::string& market,
                        const std::string& code,
                        int max_results = 1000) const;
    Json catalog() const;
    std::size_t resource_count() const noexcept;
    std::size_t security_key_count() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::vector<JsnTable> load_jsn_tables(const std::filesystem::path& path);
std::string jsn_scalar_text(const Json& value);
Json query_jsn_security_document(const std::filesystem::path& input,
                                 const std::string& market,
                                 const std::string& code,
                                 int max_results = 1000);
Json catalog_jsn_document(const std::filesystem::path& input);
int command_jsn_catalog(const std::vector<std::string>& args);
int command_jsn_query(const std::vector<std::string>& args);

}  // namespace tdx
