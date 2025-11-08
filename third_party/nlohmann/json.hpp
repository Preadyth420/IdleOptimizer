#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <istream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nlohmann {

class json {
public:
    using array_t = std::vector<json>;
    using object_t = std::map<std::string, json>;

    json() : value_(nullptr) {}

    static json parse(const std::string &text) {
        parser p(text);
        json result = p.parse_value();
        p.skip_ws();
        if (!p.at_end()) {
            throw std::runtime_error("Unexpected trailing characters in JSON input");
        }
        return result;
    }

    static json parse(std::istream &in) {
        std::ostringstream oss;
        oss << in.rdbuf();
        return parse(oss.str());
    }

    friend std::istream &operator>>(std::istream &in, json &j) {
        j = parse(in);
        return in;
    }

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool is_boolean() const { return std::holds_alternative<bool>(value_); }
    bool is_number() const { return std::holds_alternative<long long>(value_) || std::holds_alternative<double>(value_); }
    bool is_number_integer() const { return std::holds_alternative<long long>(value_); }
    bool is_number_float() const { return std::holds_alternative<double>(value_); }
    bool is_string() const { return std::holds_alternative<std::string>(value_); }
    bool is_array() const { return std::holds_alternative<array_t>(value_); }
    bool is_object() const { return std::holds_alternative<object_t>(value_); }

    const json *find(const std::string &key) const {
        if (!is_object()) {
            return nullptr;
        }
        const auto &obj = std::get<object_t>(value_);
        auto it = obj.find(key);
        if (it == obj.end()) {
            return nullptr;
        }
        return &it->second;
    }

    bool contains(const std::string &key) const { return find(key) != nullptr; }

    const array_t &as_array() const {
        if (!is_array()) {
            throw std::runtime_error("JSON value is not an array");
        }
        return std::get<array_t>(value_);
    }

    const object_t &as_object() const {
        if (!is_object()) {
            throw std::runtime_error("JSON value is not an object");
        }
        return std::get<object_t>(value_);
    }

    array_t::const_iterator begin() const {
        if (is_array()) {
            return std::get<array_t>(value_).begin();
        }
        return empty_array().end();
    }

    array_t::const_iterator end() const {
        if (is_array()) {
            return std::get<array_t>(value_).end();
        }
        return empty_array().end();
    }

    std::size_t size() const {
        if (is_array()) {
            return std::get<array_t>(value_).size();
        }
        if (is_object()) {
            return std::get<object_t>(value_).size();
        }
        return 0;
    }

    template <typename T>
    T get() const {
        if constexpr (std::is_same_v<T, bool>) {
            if (!is_boolean()) {
                throw std::runtime_error("JSON value is not a boolean");
            }
            return std::get<bool>(value_);
        } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            if (is_number_integer()) {
                long long v = std::get<long long>(value_);
                if (v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max()) {
                    throw std::out_of_range("JSON integer out of range");
                }
                return static_cast<T>(v);
            }
            if (is_number_float()) {
                double v = std::get<double>(value_);
                if (!std::isfinite(v) || v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max()) {
                    throw std::out_of_range("JSON number out of range for integral conversion");
                }
                return static_cast<T>(std::llround(v));
            }
            throw std::runtime_error("JSON value is not an integer");
        } else if constexpr (std::is_floating_point_v<T>) {
            if (is_number_float()) {
                return static_cast<T>(std::get<double>(value_));
            }
            if (is_number_integer()) {
                return static_cast<T>(std::get<long long>(value_));
            }
            throw std::runtime_error("JSON value is not a number");
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (!is_string()) {
                throw std::runtime_error("JSON value is not a string");
            }
            return std::get<std::string>(value_);
        } else {
            static_assert(!sizeof(T), "Unsupported json::get<T>() type");
        }
    }

private:
    class parser {
    public:
        explicit parser(const std::string &text) : text_(text), pos_(0) {}

        bool at_end() const { return pos_ >= text_.size(); }

        void skip_ws() {
            while (!at_end() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }

        json parse_value() {
            skip_ws();
            if (at_end()) {
                throw std::runtime_error("Unexpected end of JSON input");
            }
            char c = text_[pos_];
            switch (c) {
            case 'n':
                return parse_null();
            case 't':
                return parse_true();
            case 'f':
                return parse_false();
            case '"':
                return parse_string();
            case '[':
                return parse_array();
            case '{':
                return parse_object();
            default:
                if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
                    return parse_number();
                }
                throw std::runtime_error("Invalid character in JSON input");
            }
        }

        json parse_null() {
            expect_literal("null");
            return json(nullptr);
        }

        json parse_true() {
            expect_literal("true");
            return json(true);
        }

        json parse_false() {
            expect_literal("false");
            return json(false);
        }

        json parse_string() {
            if (!consume('"')) {
                throw std::runtime_error("Expected opening quote for JSON string");
            }
            std::string result;
            while (!at_end()) {
                char c = text_[pos_++];
                if (c == '"') {
                    return json(std::move(result));
                }
                if (c == '\\') {
                    if (at_end()) {
                        throw std::runtime_error("Invalid escape sequence in JSON string");
                    }
                    char esc = text_[pos_++];
                    switch (esc) {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    case 'u': {
                        unsigned int cp = parse_hex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (!(pos_ + 6 <= text_.size() && text_[pos_] == '\\' && text_[pos_ + 1] == 'u')) {
                                throw std::runtime_error("Invalid unicode surrogate pair");
                            }
                            pos_ += 2;
                            unsigned int low = parse_hex4();
                            if (low < 0xDC00 || low > 0xDFFF) {
                                throw std::runtime_error("Invalid unicode surrogate pair");
                            }
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        }
                        append_utf8(result, cp);
                        break;
                    }
                    default:
                        throw std::runtime_error("Invalid escape character in JSON string");
                    }
                } else {
                    result.push_back(c);
                }
            }
            throw std::runtime_error("Unterminated JSON string");
        }

        json parse_array() {
            consume('[');
            array_t arr;
            skip_ws();
            if (consume(']')) {
                return json(std::move(arr));
            }
            while (true) {
                arr.push_back(parse_value());
                skip_ws();
                if (consume(']')) {
                    break;
                }
                if (!consume(',')) {
                    throw std::runtime_error("Expected ',' in JSON array");
                }
            }
            return json(std::move(arr));
        }

        json parse_object() {
            consume('{');
            object_t obj;
            skip_ws();
            if (consume('}')) {
                return json(std::move(obj));
            }
            while (true) {
                skip_ws();
                if (at_end() || text_[pos_] != '"') {
                    throw std::runtime_error("Expected string key in JSON object");
                }
                std::string key = parse_string().get<std::string>();
                skip_ws();
                if (!consume(':')) {
                    throw std::runtime_error("Expected ':' after key in JSON object");
                }
                json value = parse_value();
                obj.emplace(std::move(key), std::move(value));
                skip_ws();
                if (consume('}')) {
                    break;
                }
                if (!consume(',')) {
                    throw std::runtime_error("Expected ',' in JSON object");
                }
            }
            return json(std::move(obj));
        }

        json parse_number() {
            std::size_t start = pos_;
            if (consume('-')) {
                if (at_end() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                    throw std::runtime_error("Invalid number format");
                }
            }
            if (consume('0')) {
                if (!at_end() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                    throw std::runtime_error("Leading zeros are not allowed in JSON numbers");
                }
            } else {
                if (!std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                    throw std::runtime_error("Invalid number format");
                }
                while (!at_end() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                    ++pos_;
                }
            }
            bool is_integer = true;
            if (consume('.')) {
                is_integer = false;
                if (at_end() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                    throw std::runtime_error("Invalid number format");
                }
                while (!at_end() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                    ++pos_;
                }
            }
            if (!at_end() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
                is_integer = false;
                ++pos_;
                if (!at_end() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                    ++pos_;
                }
                if (at_end() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                    throw std::runtime_error("Invalid exponent in JSON number");
                }
                while (!at_end() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                    ++pos_;
                }
            }
            std::string_view view(text_.data() + start, pos_ - start);
            if (is_integer) {
                long long val = 0;
                try {
                    size_t idx = 0;
                    val = std::stoll(std::string(view), &idx, 10);
                    if (idx != view.size()) {
                        throw std::runtime_error("Invalid integer format");
                    }
                } catch (const std::exception &) {
                    throw std::runtime_error("Failed to parse integer value");
                }
                return json(val);
            }
            double d = 0.0;
            try {
                d = std::stod(std::string(view));
            } catch (const std::exception &) {
                throw std::runtime_error("Failed to parse floating point value");
            }
            return json(d);
        }

    private:
        bool consume(char expected) {
            if (!at_end() && text_[pos_] == expected) {
                ++pos_;
                return true;
            }
            return false;
        }

        void expect_literal(const char *literal) {
            while (*literal) {
                if (at_end() || text_[pos_] != *literal) {
                    throw std::runtime_error("Invalid literal in JSON input");
                }
                ++pos_;
                ++literal;
            }
        }

        unsigned int parse_hex4() {
            if (pos_ + 4 > text_.size()) {
                throw std::runtime_error("Invalid unicode escape in JSON string");
            }
            unsigned int value = 0;
            for (int i = 0; i < 4; ++i) {
                char c = text_[pos_++];
                value <<= 4;
                if (c >= '0' && c <= '9') value |= static_cast<unsigned int>(c - '0');
                else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned int>(c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned int>(c - 'a' + 10);
                else throw std::runtime_error("Invalid hexadecimal digit in unicode escape");
            }
            return value;
        }

        static void append_utf8(std::string &out, unsigned int cp) {
            if (cp <= 0x7F) {
                out.push_back(static_cast<char>(cp));
            } else if (cp <= 0x7FF) {
                out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp <= 0xFFFF) {
                out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp <= 0x10FFFF) {
                out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                throw std::runtime_error("Invalid unicode codepoint");
            }
        }

        const std::string &text_;
        std::size_t pos_;
    };

    explicit json(std::nullptr_t) : value_(nullptr) {}
    explicit json(bool b) : value_(b) {}
    explicit json(long long i) : value_(i) {}
    explicit json(double d) : value_(d) {}
    explicit json(std::string s) : value_(std::move(s)) {}
    explicit json(array_t a) : value_(std::move(a)) {}
    explicit json(object_t o) : value_(std::move(o)) {}

    static const array_t &empty_array() {
        static const array_t arr{};
        return arr;
    }

    std::variant<std::nullptr_t, bool, long long, double, std::string, array_t, object_t> value_;
};

} // namespace nlohmann

