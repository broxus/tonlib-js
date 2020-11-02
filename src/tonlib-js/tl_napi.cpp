#include "tl_napi.hpp"

namespace tjs
{

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

auto from_napi_bytes(const Napi::Value& from, td::SecureString& to) -> td::Status
{
    if (!from.IsArrayBuffer()) {
        return td::Status::Error("Expected ArrayBuffer");
    }
    auto array_buffer = from.As<Napi::ArrayBuffer>();
    td::BufferSlice dest(array_buffer.ByteLength());
    std::memcpy(dest.data(), array_buffer.Data(), array_buffer.ByteLength());
    to = td::SecureString{std::move(dest)};
    return td::Status::OK();
}

}  // namespace tjs
