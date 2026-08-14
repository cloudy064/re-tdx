#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tdx {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json, std::less<>>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Json() noexcept;
    Json(std::nullptr_t) noexcept;
    Json(bool value);
    Json(int value);
    Json(std::int64_t value);
    Json(std::uint64_t value);
    Json(double value);
    Json(const char* value);
    Json(std::string value);
    Json(Array value);
    Json(Object value);

    static Json array();
    static Json object();
    static Json parse(std::string_view text);

    bool is_null() const noexcept;
    bool is_bool() const noexcept;
    bool is_number() const noexcept;
    bool is_string() const noexcept;
    bool is_array() const noexcept;
    bool is_object() const noexcept;

    bool as_bool() const;
    double as_number() const;
    const std::string& as_string() const;
    const Array& as_array() const;
    const Object& as_object() const;
    Array& as_array();
    Object& as_object();

    Json& operator[](std::string key);
    const Json& at(std::string_view key) const;
    void push_back(Json value);
    std::size_t size() const noexcept;
    std::string dump(int indent = 2) const;

private:
    Value value_;
};

}  // namespace tdx
