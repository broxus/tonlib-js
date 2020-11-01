#pragma once

#include <napi.h>
#include <td/utils/JsonBuilder.h>
#include <td/utils/SharedSlice.h>
#include <td/utils/Slice.h>
#include <td/utils/Status.h>
#include <td/utils/base64.h>
#include <td/utils/buffer.h>
#include <td/utils/format.h>
#include <td/utils/misc.h>
#include <td/utils/tl_storers.h>
#include <tl/TlObject.h>

#include <type_traits>

namespace tjs
{
struct NapiInt64 {
    int64_t value;
};

inline auto to_napi(const Napi::Env& env, const NapiInt64& data) -> Napi::Value
{
    const auto str = std::to_string(data.value);
    return Napi::String::New(env, str);
}

struct NapiVectorInt64 {
    const std::vector<int64_t>& value;
};

inline auto to_napi(const Napi::Env& env, const NapiVectorInt64& data) -> Napi::Value
{
    auto array = Napi::Array::New(env, data.value.size());
    for (size_t i = 0; i < data.value.size(); ++i) {
        array.Set(i, to_napi(env, NapiInt64{data.value[i]}));
    }
    return array;
}

struct NapiBytes {
    td::Slice value;
};

inline auto to_napi(const Napi::Env& env, const NapiBytes& data) -> Napi::Value
{
    auto array = Napi::ArrayBuffer::New(env, data.value.size());
    std::memcpy(array.Data(), data.value.begin(), data.value.size());
    return array;
}

template <typename T>
struct NapiVectorBytesImpl {
    const std::vector<T>& value;
};

template <typename T>
auto NapiVectorBytes(const std::vector<T>& value)
{
    return NapiVectorBytesImpl<T>{value};
}

template <typename T>
auto to_napi(const Napi::Env& env, const NapiVectorBytesImpl<T>& data) -> Napi::Value
{
    auto array = Napi::Array::New(env, data.value.size());
    for (size_t i = 0; i < data.value.size(); ++i) {
        array.Set(i, to_napi(env, NapiBytes{data.value[i]}));
    }
    return array;
}

template <unsigned size>
inline auto to_napi(const Napi::Env& env, const td::BitArray<size>& data) -> Napi::Value
{
    return to_napi(env, NapiBytes{td::as_slice(data)});
}

template <typename T>
auto to_napi(const Napi::Env& env, const ton::tl_object_ptr<T>& data) -> Napi::Value
{
    if (data) {
        return to_napi(env, *data);
    }
    else {
        return env.Null();
    }
}

template <typename T>
auto to_napi(const Napi::Env& env, const std::vector<T>& data) -> Napi::Value
{
    auto array = Napi::Array::New(env, data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        array.Set(i, to_napi(data[i]));
    }
    return array;
}

inline auto to_napi(const Napi::Env& env, int32_t data) -> Napi::Value
{
    return Napi::Number::New(env, data);
}

inline auto to_napi(const Napi::Env& env, bool data) -> Napi::Value
{
    return Napi::Boolean::New(env, data);
}

auto from_napi(const Napi::Value& from, int32_t& to) -> td::Status
{
    if (!from.IsNumber() && !from.IsString()) {
        return td::Status::Error("Expected number or string");
    }
    if (from.IsNumber()) {
        to = from.As<Napi::Number>().Int32Value();
    }
    else {
        auto str = from.As<Napi::String>().Utf8Value();
        TRY_RESULT_ASSIGN(to, td::to_integer_safe<int32_t>(str))
    }
    return td::Status::OK();
}

auto from_napi(const Napi::Value& from, bool& to) -> td::Status
{
    if (!from.IsBoolean()) {
        int32_t x;
        auto status = from_napi(from, x);
        if (status.is_ok()) {
            to = x != 0;
            return td::Status::OK();
        }
        return td::Status::Error("Expected bool");
    }
    to = from.As<Napi::Boolean>();
    return td::Status::OK();
}

auto from_napi(const Napi::Value& from, int64_t& to) -> td::Status
{
    if (!from.IsNumber() && !from.IsString()) {
        return td::Status::Error("Expected number or string");
    }
    if (from.IsNumber()) {
        to = from.As<Napi::Number>().Int64Value();
    }
    else {
        auto str = from.As<Napi::String>().Utf8Value();
        TRY_RESULT_ASSIGN(to, td::to_integer_safe<int64_t>(str))
    }
    return td::Status::OK();
}

auto from_napi(const Napi::Value& from, double& to) -> td::Status
{
    if (!from.IsNumber()) {
        return td::Status::Error("Expected number");
    }
    to = from.As<Napi::Number>().DoubleValue();
    return td::Status::OK();
}

auto from_napi(const Napi::Value& from, std::string& to) -> td::Status
{
    if (!from.IsString()) {
        return td::Status::Error("Expected string");
    }
    to = from.As<Napi::String>().Utf8Value();
    return td::Status::OK();
}

auto from_napi(const Napi::Value& from, td::SecureString& to) -> td::Status
{
    if (!from.IsString()) {
        return td::Status::Error("Expected string");
    }
    to = td::SecureString{from.As<Napi::String>().Utf8Value()};
    return td::Status::OK();
}

auto from_napi_bytes(const Napi::Value& from, std::string& to) -> td::Status
{
    if (!from.IsArrayBuffer()) {
        return td::Status::Error("Expected ArrayBuffer");
    }
    auto array_buffer = from.As<Napi::ArrayBuffer>();
    to.resize(array_buffer.ByteLength());
    std::memcpy(to.data(), array_buffer.Data(), array_buffer.ByteLength());
    return td::Status::OK();
}

template <unsigned size>
auto from_napi_bytes(const Napi::Value& from, td::BitArray<size>& to) -> td::Status
{
    std::string raw;
    TRY_STATUS(from_napi_bytes(from, raw))
    auto slice = to.as_slice();
    if (raw.size() != slice.size()) {
        return td::Status::Error("Wrong length for BitArray");
    }
    slice.copy_from(raw);
    return td::Status::OK();
}

template <typename T>
auto from_napi(const Napi::Value& from, std::vector<T>& to) -> td::Status
{
    if (!from.IsArray()) {
        return td::Status::Error("Expected array");
    }
    auto array = from.As<Napi::Array>();
    to = std::vector<T>(array.Length());
    for (size_t i = 0; i < array.Length(); ++i) {
        TRY_STATUS(from_napi(array.Get(i), to[i]))
    }
    return td::Status::OK();
}

template <typename T>
auto from_napi_vector_bytes(const Napi::Value& from, std::vector<T>& to) -> td::Status
{
    if (!from.IsArray()) {
        return td::Status::Error("Expected array");
    }
    auto array = from.As<Napi::Array>();
    to = std::vector<T>(array.Length());
    for (size_t i = 0; i < array.Length(); ++i) {
        TRY_STATUS(from_napi_bytes(array.Get(i), to[i]))
    }
    return td::Status::OK();
}

template <typename T>
class DowncastHelper final : public T {
public:
    explicit DowncastHelper(int32_t constructor)
        : constructor_{constructor}
    {
    }

    [[nodiscard]] auto get_id() const -> int32_t final { return constructor_; }
    void store(td::TlStorerToString& s, const char* /*field_name*/) const final {}

private:
    int32_t constructor_;
};

constexpr auto napi_constructor = "constructor";

template <typename T>
auto from_napi(Napi::Value& from, ton::tl_object_ptr<T>& to) -> td::Status
{
    if (from.IsNull()) {
        to = nullptr;
        return td::Status::OK();
    }
    if (!from.IsObject()) {
        return td::Status::Error("Expected object");
    }
    auto object = from.As<Napi::Object>();

    if constexpr (std::is_constructible_v<T>) {
        to = ton::create_tl_object<T>();
        return from_napi(object, *to);
    }
    else {
        auto constructor = object.Get(napi_constructor);
        if (!constructor.IsFunction()) {
            return td::Status::Error("Expected object with constructor");
        }
        auto constructor_type = constructor.As<Napi::Function>().Get("name");
        if (!constructor_type.IsString()) {
            return td::Status::Error("Invalid constructor name");
        }

        TRY_RESULT(type_id, tl_constructor_from_napi(to.get(), constructor_type.As<Napi::String>().Utf8Value()))

        DowncastHelper<T> helper{type_id};
        td::Status status;
        bool ok = downcast_call(static_cast<T&>(helper), [&](auto& dummy) {
            auto result = ton::create_tl_object<std::decay_t<decltype(dummy)>>();
            status = from_napi(object, *result);
            to = std::move(result);
        });
        TRY_STATUS(std::move(status))
        if (!ok) {
            return td::Status::Error(PSLICE() << "Unknown constructor " << td::format::as_hex(type_id));
        }

        return td::Status::OK();
    }
}

}  // namespace tjs
