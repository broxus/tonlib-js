#include <td/tl/tl_config.h>
#include <td/tl/tl_generate.h>
#include <td/tl/tl_simple.h>
#include <td/utils/Slice.h>
#include <td/utils/StringBuilder.h>
#include <td/utils/buffer.h>
#include <td/utils/filesystem.h>
#include <td/utils/logging.h>

#include <utility>

namespace tjs
{
namespace
{
constexpr auto tl_name = "tonlib_api";


auto gen_js_field_name(const std::string& name) -> std::string
{
    std::string result;
    bool next_to_upper = false;
    for (char i : name) {
        if (!td::is_alnum(i)) {
            next_to_upper = true;
            continue;
        }
        if (next_to_upper) {
            result += td::to_upper(i);
            next_to_upper = false;
        }
        else {
            result += i;
        }
    }
    return result;
}

auto gen_basic_js_class_name(const std::string& name) -> std::string
{
    std::string result;
    bool next_to_upper = true;
    for (char i : name) {
        if (!td::is_alnum(i)) {
            next_to_upper = true;
            continue;
        }
        if (next_to_upper) {
            result += td::to_upper(i);
            next_to_upper = false;
        }
        else {
            result += i;
        }
    }
    return result;
}

}  // namespace

template <class T>
void gen_to_napi_constructor(td::StringBuilder& sb, const T* constructor, bool is_header)
{
    sb << "auto to_napi(const Napi::Env& env, "
       << "const " << tl_name << "::" << td::tl::simple::gen_cpp_name(constructor->name) << "& data)"
       << " -> Napi::Value";
    if (is_header) {
        sb << ";\n";
        return;
    }
    sb << " {\n";
    sb << "  auto jo = jv.enter_object();\n";
    sb << R"(  jo << ctie("@type", ")" << gen_basic_js_class_name(constructor->name) << "\");\n";
    for (auto& arg : constructor->args) {
        auto field = td::tl::simple::gen_cpp_field_name(arg.name);
        // TODO: or as null
        bool is_custom = arg.type->type == td::tl::simple::Type::Custom;

        if (is_custom) {
            sb << "  if (object." << field << ") {\n  ";
        }
        auto object = PSTRING() << "object." << td::tl::simple::gen_cpp_field_name(arg.name);
        if (arg.type->type == td::tl::simple::Type::Bytes || arg.type->type == td::tl::simple::Type::SecureBytes) {
            object = PSTRING() << "JsonBytes{" << object << "}";
        }
        else if (arg.type->type == td::tl::simple::Type::Int64) {
            object = PSTRING() << "JsonInt64{" << object << "}";
        }
        else if (
            arg.type->type == td::tl::simple::Type::Vector &&
            (arg.type->vector_value_type->type == td::tl::simple::Type::Bytes || arg.type->vector_value_type->type == td::tl::simple::Type::SecureBytes)) {
            object = PSTRING() << "JsonVectorBytes(" << object << ")";
        }
        else if (arg.type->type == td::tl::simple::Type::Vector && arg.type->vector_value_type->type == td::tl::simple::Type::Int64) {
            object = PSTRING() << "JsonVectorInt64{" << object << "}";
        }
        sb << "  jo << ctie(\"" << arg.name << "\", ToJson(" << object << "));\n";
        if (is_custom) {
            sb << "  }\n";
        }
    }
    sb << "}\n";
}

void gen_to_napi(td::StringBuilder& sb, const td::tl::simple::Schema& schema, bool is_header)
{
    for (auto* custom_type : schema.custom_types) {
        if (custom_type->constructors.size() > 1) {
            auto type_name = td::tl::simple::gen_cpp_name(custom_type->name);
            sb << "auto to_napi(const Napi::Env& env, const " << tl_name << "::" << type_name << "& data) -> Napi::Value";
            if (is_header) {
                sb << ";\n";
            }
            else {
                sb << "\n{\n"
                   << "  " << tl_name << "::downcast_call(const_cast<" << tl_name << "::" << type_name
                   << "&>(data), [&env](const auto &data) { to_napi(env, data); });\n"
                   << "}\n";
            }
        }
        for (auto* constructor : custom_type->constructors) {
            gen_to_napi_constructor(sb, constructor, is_header);
        }
    }
    for (auto* function : schema.functions) {
        gen_to_napi_constructor(sb, function, is_header);
    }

    if (is_header) {
        sb << "inline auto to_json(const Napi::Env& env, const ton::" << tl_name << "::Object& data) -> Napi::Value\n"
           << "{\n"
           << "  ton::" << tl_name << "::downcast_call(const_cast<ton::" << tl_name
           << "::Object&>(data), [&env](const auto &data) { "
              "to_napi(env, data); });\n"
           << "}\n";

        sb << "inline auto to_json(const Napi::Env& env, const ton::" << tl_name << "::Function& object) -> Napi::Value\n"
           << "{\n"
           << "  ton::" << tl_name << "::downcast_call(const_cast<ton::" << tl_name
           << "::Function&>(data), [&env](const auto &data) { "
              "to_napi(env, data); });\n"
           << "}\n";
    }
}

template <class T>
void gen_from_napi_constructor(td::StringBuilder& sb, const T* constructor, bool is_header)
{
    sb << "auto from_napi(const Napi::Value& from, " << tl_name << "::" << td::tl::simple::gen_cpp_name(constructor->name) << " &to) -> td::Status";
    if (is_header) {
        sb << ";\n";
    }
    else {
        sb << "\n{\n";
        for (auto& arg : constructor->args) {
            sb << "  {\n";
            sb << "    auto value = from.Get(\"" << gen_js_field_name(arg.name) << "\")\n";
            sb << "    if (!value.IsNull()) {\n";
            if (arg.type->type == td::tl::simple::Type::Bytes || arg.type->type == td::tl::simple::Type::SecureBytes) {
                sb << "      TRY_STATUS(from_napi_bytes(value, to." << td::tl::simple::gen_cpp_field_name(arg.name) << "))\n";
            }
            else if (
                arg.type->type == td::tl::simple::Type::Vector &&
                (arg.type->vector_value_type->type == td::tl::simple::Type::Bytes || arg.type->vector_value_type->type == td::tl::simple::Type::SecureBytes)) {
                sb << "      TRY_STATUS(from_napi_vector_bytes(value, to." << td::tl::simple::gen_cpp_field_name(arg.name) << "));\n";
            }
            else {
                sb << "      TRY_STATUS(from_napi(value, to." << td::tl::simple::gen_cpp_field_name(arg.name) << "))\n";
            }
            sb << "    }\n";
            sb << "  }\n";
        }
        sb << "  return Status::OK();\n";
        sb << "}\n";
    }
}

void gen_from_json(td::StringBuilder& sb, const td::tl::simple::Schema& schema, bool is_header)
{
    for (auto* custom_type : schema.custom_types) {
        for (auto* constructor : custom_type->constructors) {
            gen_from_napi_constructor(sb, constructor, is_header);
        }
    }
    for (auto* function : schema.functions) {
        gen_from_napi_constructor(sb, function, is_header);
    }
}

using Vec = std::vector<std::pair<int32_t, std::string>>;
void gen_tl_constructor_from_string(td::StringBuilder& sb, td::Slice name, const Vec& vec, bool is_header)
{
    sb << "auto tl_constructor_from_string(" << tl_name << "::" << name << "* object, const std::string& str) -> td::Result<int32>";
    if (is_header) {
        sb << ";\n";
        return;
    }
    sb << "\n{\n";
    sb << "  static const std::unordered_map<Slice, int32, SliceHash> m = {\n";

    bool is_first = true;
    for (auto& p : vec) {
        if (is_first) {
            is_first = false;
        }
        else {
            sb << ",\n";
        }
        sb << "    {\"" << gen_basic_js_class_name(p.second) << "\", " << p.first << "}";
    }
    sb << "\n  };\n";
    sb << "  auto it = m.find(str);\n";
    sb << "  if (it == m.end()) {\n"
       << "    return td::Status::Error(str + \"Unknown class\");\n"
       << "  }\n"
       << "  return it->second;\n";
    sb << "}\n";
}

void gen_tl_constructor_from_string(td::StringBuilder& sb, const td::tl::simple::Schema& schema, bool is_header)
{
    Vec vec_for_nullable;
    for (auto* custom_type : schema.custom_types) {
        Vec vec;
        for (auto* constructor : custom_type->constructors) {
            vec.emplace_back(std::make_pair(constructor->id, constructor->name));
            vec_for_nullable.emplace_back(vec.back());
        }

        if (vec.size() > 1) {
            gen_tl_constructor_from_string(sb, td::tl::simple::gen_cpp_name(custom_type->name), vec, is_header);
        }
    }
    gen_tl_constructor_from_string(sb, "Object", vec_for_nullable, is_header);

    Vec vec_for_function;
    for (auto* function : schema.functions) {
        vec_for_function.push_back(std::make_pair(function->id, function->name));
    }
    gen_tl_constructor_from_string(sb, "Function", vec_for_function, is_header);
}

void gen_napi_converter_file(const td::tl::simple::Schema& schema, const std::string& file_name_base, bool is_header)
{
    auto file_name = is_header ? file_name_base + ".h" : file_name_base + ".cpp";
    // file_name = "auto/" + file_name;
    auto old_file_content = [&] {
        auto r_content = td::read_file(file_name);
        if (r_content.is_error()) {
            return td::BufferSlice();
        }
        return r_content.move_as_ok();
    }();

    std::string buf(2000000, ' ');
    td::StringBuilder sb(td::MutableSlice{buf});

    if (is_header) {
        sb << "#pragma once\n\n";

        sb << "#include \"auto/tl/" << tl_name << ".h\"\n\n";
        sb << "#include \"auto/tl/" << tl_name << ".hpp\"\n\n";

        sb << "#include \"td/utils/JsonBuilder.h\"\n";
        sb << "#include \"td/utils/Status.h\"\n\n";

        sb << "#include \"crypto/common/bitstring.h\"\n";
    }
    else {
        sb << "#include \"" << file_name_base << ".h\"\n\n";

        sb << "#include \"auto/tl/" << tl_name << ".h\"\n";
        sb << "#include \"auto/tl/" << tl_name << ".hpp\"\n\n";

        sb << "#include \"tl/tl_json.h\"\n\n";

        sb << "#include \"td/utils/base64.h\"\n";
        sb << "#include \"td/utils/common.h\"\n";
        sb << "#include \"td/utils/Slice.h\"\n\n";

        sb << "#include <unordered_map>\n\n";
    }
    sb << "namespace ton {\n";
    sb << "namespace " << tl_name << "{\n";
    sb << "  using namespace td;\n";
    gen_tl_constructor_from_string(sb, schema, is_header);
    gen_from_json(sb, schema, is_header);
    gen_to_napi(sb, schema, is_header);
    sb << "}  // namespace " << tl_name << "\n";
    sb << "}  // namespace ton\n";

    CHECK(!sb.is_error());
    buf.resize(sb.as_cslice().size());
    auto new_file_content = std::move(buf);
    if (new_file_content != old_file_content.as_slice()) {
        td::write_file(file_name, new_file_content).ensure();
    }
}

void gen_napi_converter(const td::tl::tl_config& config, const std::string& file_name)
{
    td::tl::simple::Schema schema(config);
    gen_napi_converter_file(schema, file_name, true);
    gen_napi_converter_file(schema, file_name, false);
}

}  // namespace tjs

auto main() -> int
{
    tjs::gen_napi_converter(td::tl::read_tl_config_from_file("../../extlibs/ton/tl/generate/scheme/tonlib_api.tlo"), "test");
    return 0;
}
