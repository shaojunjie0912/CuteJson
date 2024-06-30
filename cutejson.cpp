#include <charconv>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "print.h"

using std::cout;
using std::endl;

namespace cutejson {

struct JsonObject;
using JsonList = std::vector<JsonObject>;
using JsonDict = std::unordered_map<std::string, JsonObject>;

struct JsonObject {
    // JsonObject 类型
    std::variant<std::nullptr_t, bool, int, double, std::string, JsonList, JsonDict> inner;

    // 联动 "print.h"
    void do_print() const {
        printnl(inner);
    }

    template <typename T>
    bool is() const {
        return std::holds_alternative<T>(inner);
    }

    template <typename T>
    const T& get() const {
        return std::get<T>(inner);
    }

    template <typename T>
    T& get() {
        return std::get<T>(inner);
    }
};

// 尝试解析输入中的数字
template <typename T>
std::optional<T> TryParseNum(std::string_view str) {
    T value;
    auto res = std::from_chars(str.data(), str.data() + str.size(), value);
    if (res.ec == std::errc{} && res.ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}

char UnescapedChar(char c) {
    switch (c) {
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case '0':
            return '\0';
        case 't':
            return '\t';
        case 'v':
            return '\v';
        case 'f':
            return '\f';
        case 'b':
            return '\b';
        case 'a':
            return '\a';
        default:
            return c;
    }
}

std::pair<JsonObject, size_t> Parse(std::string_view json) {
    // 如果输入字符串序列为空，则直接返回空JsonObject
    if (json.empty()) {
        return {JsonObject{std::nullptr_t{}}, 0};
    }
    // 如果输入字符串序列中包含换行、制表符...则先去除
    // 否则无法解析成功
    else if (size_t off = json.find_first_not_of(" \n\r\t\v\f\0"); off != 0 && off != json.npos) {
        // 递归去除
        auto [obj, eaten] = Parse(json.substr(off));
        return {std::move(obj), eaten + off};
    }
    // num
    else if ('0' <= json[0] && json[0] <= '9' || json[0] == '+' || json[0] == '-') {
        std::regex num_re{"[+-]?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?"};
        std::cmatch match;
        if (std::regex_search(json.data(), json.data() + json.size(), match, num_re)) {
            std::string str = match.str();
            // cout << "match str: " << str << endl;
            if (auto num = TryParseNum<int>(str)) {  // ;num.has_value()
                return {JsonObject{*num}, str.size()};
            }
            if (auto num = TryParseNum<double>(str)) {  // ;num.has_value()
                return {JsonObject{*num}, str.size()};
            }
        }
        return {JsonObject{std::nullptr_t{}}, 0};
    }
    // string
    else if (json[0] == '"') {
        std::string str;
        enum { Raw, Escaped } Phase = Raw;
        size_t i;  // 记录解析位置
        for (i = 1; i < json.size(); ++i) {
            char ch = json[i];
            if (Phase == Raw) {
                // 解析字符串字面量中的转义字符
                if (ch == '\\') {
                    Phase = Escaped;
                }
                // 遇到引号退出
                else if (ch == '"') {
                    ++i;  // 解析位置后移
                    break;
                } else {
                    str += ch;
                }
            } else if (Phase == Escaped) {
                str += UnescapedChar(ch);
                Phase = Raw;
            }
        }
        return {JsonObject{str}, i};
    }
    // list
    else if (json[0] == '[') {
        JsonList res;
        size_t i;  // 记录解析位置
        for (i = 1; i < json.size();) {
            if (json[i] == ']') {
                ++i;  // 解析位置后移
                break;
            }
            // 递归
            auto [obj, eaten] = Parse(json.substr(i));
            if (eaten == 0) {
                i = 0;  // 解析出错
                break;
            }
            res.push_back(std::move(obj));
            i += eaten;  // 移动到下一个待解析的字符
            if (json[i] == ',') {
                ++i;  // 遇到','则跳过
            }
        }
        return {JsonObject{std::move(res)}, i};
    }
    // dictionary
    else if (json[0] == '{') {
        std::unordered_map<std::string, JsonObject> res;
        size_t i;  // 记录解析位置
        for (i = 1; i < json.size();) {
            if (json[i] == '}') {  // 解析到末尾则退出
                ++i;
                break;
            }
            // 解析 key (std::string)
            auto [key_obj, key_eaten] = Parse(json.substr(i));
            if (key_eaten == 0) {  // 解析失败则退出
                i = 0;
                break;
            }
            i += key_eaten;
            // 解析到的key中不是字符串则退出
            // std::holds_alternative 适用于变体variant
            if (!std::holds_alternative<std::string>(key_obj.inner)) {
                i = 0;
                break;
            }
            if (json[i] == ':') {  // 跳过:解析后面的value
                ++i;
            }
            std::string key = std::move(std::get<std::string>(key_obj.inner));
            // 解析 value (JsonObject)
            auto [val_obj, val_eaten] = Parse(json.substr(i));
            if (val_eaten == 0) {
                i = 0;
                break;
            }
            i += val_eaten;
            res.try_emplace(std::move(key), std::move(val_obj));
            if (json[i] == ',') {  // 跳过','解析下一个字典键值对
                ++i;
            }
        }
        return {JsonObject{std::move(res)}, i};
    }
    return {JsonObject{std::nullptr_t{}}, 0};
}

//----------------- 不懂
template <class... Fs>
struct overloaded : Fs... {
    using Fs::operator()...;
};

template <class... Fs>
overloaded(Fs...) -> overloaded<Fs...>;
//----------------- 不懂

}  // namespace cutejson

int main() {
    using namespace cutejson;
    std::string_view num_json{"3,3,3"};
    std::string_view str_json{R"("")"};
    std::string_view list_json{R"([42,[222,"dasda]",15],12,3,4])"};
    std::string_view dict_json{R"(    {"num":123,"str":"996","list":[12,"da",[3,"[d]"]]})"};
    // auto [obj, eaten] = Parse(list_json);
    // auto [obj, eaten] = Parse(str_json);
    // auto [obj, eaten] = Parse(list_json);
    auto [obj, eaten] = Parse(dict_json);
    print(obj);
    // print(eaten);
    // print(Parse(str_json));
    // cout << R"(dasd\ndas)" << endl;
    // cout << std::string{"dasd\ndas"} << endl;
    return 0;
}
