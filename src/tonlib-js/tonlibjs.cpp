#include <node_version.h>

//

#include <auto/tl/tonlib_api_json.h>
#include <napi.h>
#include <node_api.h>
#include <td/utils/JsonBuilder.h>
#include <td/utils/Slice.h>
#include <td/utils/Status.h>
#include <td/utils/common.h>
#include <td/utils/format.h>
#include <td/utils/logging.h>
#include <td/utils/port/thread_local.h>
#include <tl/tl_json.h>
#include <tonlib/Client.h>

#include "gen/tonlib_napi.h"
#include "tl_napi.hpp"


namespace tjs
{
static td::Result<std::pair<tonlib_api::object_ptr<tonlib_api::Function>, std::string>> to_request(td::Slice request)
{
    auto request_str = request.str();
    TRY_RESULT(json_value, td::json_decode(request_str))
    if (json_value.type() != td::JsonValue::Type::Object) {
        return td::Status::Error("Expected an Object");
    }

    std::string extra;
    if (has_json_object_field(json_value.get_object(), "@extra")) {
        extra = td::json_encode<std::string>(get_json_object_field(json_value.get_object(), "@extra", td::JsonValue::Type::Null).move_as_ok());
    }

    tonlib_api::object_ptr<tonlib_api::Function> func;
    TRY_STATUS(from_json(func, json_value))
    return std::make_pair(std::move(func), extra);
}

static std::string from_response(const tonlib_api::Object& object, const td::string& extra)
{
    auto str = td::json_encode<td::string>(td::ToJson(object));
    CHECK(!str.empty() && str.back() == '}')
    if (!extra.empty()) {
        str.pop_back();
        str.reserve(str.size() + 11 + extra.size());
        str += ",\"@extra\":";
        str += extra;
        str += '}';
    }
    return str;
}

struct ClientHandler final : public NapiPropsBase<ClientHandler> {
public:
    static Napi::FunctionReference* constructor;

    static Napi::Object init(Napi::Env env, Napi::Object exports)
    {
        constexpr auto class_name = "TonlibClient";

        Napi::Function function = DefineClass(
            env,
            class_name,
            {
                InstanceMethod("send", &ClientHandler::send),
                InstanceAccessor("props", &ClientHandler::props, nullptr),
                InstanceMethod("receive", &ClientHandler::receive),
                StaticMethod("execute", &ClientHandler::execute),
            });

        constructor = new Napi::FunctionReference();
        *constructor = Napi::Persistent(function);
        env.SetInstanceData(constructor);

        exports.Set(class_name, function);
        return exports;
    }

    explicit ClientHandler(Napi::CallbackInfo& info)
        : NapiPropsBase<ClientHandler>{info}
    {
    }

private:
    static auto json_stringify(const Napi::Env& env, const Napi::Object& object) -> Napi::String
    {
        auto json = env.Global().Get("JSON").As<Napi::Object>();
        auto stringify = json.Get("stringify").As<Napi::Function>();
        return stringify.Call(json, {object}).As<Napi::String>();
    }

    static auto json_parse(const Napi::Env& env, const Napi::String& string) -> Napi::Object
    {
        auto json = env.Global().Get("JSON").As<Napi::Object>();
        auto stringify = json.Get("parse").As<Napi::Function>();
        return stringify.Call(json, {string}).As<Napi::Object>();
    }

    static auto execute(const Napi::CallbackInfo& info) -> Napi::Value
    {
        auto env = info.Env();
        std::string request;

        const auto length = info.Length();
        if (length > 0 && !info[0].IsObject()) {
            Napi::TypeError::New(env, "Request object expected").ThrowAsJavaScriptException();
            return Napi::Value{};
        }

        if (length > 0) {
            request = json_stringify(env, info[0].As<Napi::Object>()).Utf8Value();
        }

        auto r_request = to_request(request);
        if (r_request.is_error()) {
            const auto message = PSLICE() << "Failed to parse " << tag("request", td::format::escaped(request)) << " " << r_request.error();
            Napi::Error::New(env, message.c_str()).ThrowAsJavaScriptException();
            return env.Null();
        }

        return Napi::String::New(
            env,
            from_response(*tonlib::Client::execute(tonlib::Client::Request{0, std::move(r_request.ok_ref().first)}).object, r_request.ok().second));
    }

    void send(const Napi::CallbackInfo& info)
    {
        auto env = info.Env();
        if (const auto length = info.Length(); length < 1 || !info[0].IsObject()) {
            Napi::TypeError::New(env, "Request object expected").ThrowAsJavaScriptException();
            return;
        }

        auto request = json_stringify(env, info[0].As<Napi::Object>()).Utf8Value();

        auto r_request = to_request(request);
        if (r_request.is_error()) {
            const auto message = PSLICE() << "Failed to parse " << tag("request", td::format::escaped(request)) << " " << r_request.error();
            Napi::Error::New(env, message.c_str()).ThrowAsJavaScriptException();
            return;
        }

        std::uint64_t extra_id = extra_id_.fetch_add(1, std::memory_order_relaxed);
        if (!r_request.ok_ref().second.empty()) {
            std::lock_guard<std::mutex> guard(mutex_);
            extra_[extra_id] = std::move(r_request.ok_ref().second);
        }
        client_.send(tonlib::Client::Request{extra_id, std::move(r_request.ok_ref().first)});
    }

    auto receive(const Napi::CallbackInfo& info) -> Napi::Value
    {
        auto env = info.Env();
        if (const auto length = info.Length(); length < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "Timeout number expected").ThrowAsJavaScriptException();
            return Napi::Value{};
        }

        auto response = client_.receive(info[0].As<Napi::Number>().DoubleValue());
        if (!response.object) {
            return env.Null();
        }

        std::string extra;
        if (response.id != 0) {
            std::lock_guard<std::mutex> guard(mutex_);
            auto it = extra_.find(response.id);
            if (it != extra_.end()) {
                extra = std::move(it->second);
                extra_.erase(it);
            }
        }

        return json_parse(env, Napi::String::New(env, from_response(*response.object, extra)));
    }

    tonlib::Client client_;
    std::mutex mutex_;  // for extra_
    std::unordered_map<std::int64_t, std::string> extra_;
    std::atomic<std::uint64_t> extra_id_{1};
};

Napi::FunctionReference* ClientHandler::constructor = nullptr;

Napi::Object init(Napi::Env env, Napi::Object exports)
{
    ClientHandler::init(env, exports);
    init_napi(env, exports);
    return exports;
}

NODE_API_MODULE(tonlib_js, init)

}  // namespace tjs
