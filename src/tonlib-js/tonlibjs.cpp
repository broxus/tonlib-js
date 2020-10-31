// clang-format off
#include <node_version.h>
// clang-format on

#include <napi.h>
#include <node_api.h>
#include <td/utils/Slice.h>
#include <tonlib/ClientJson.h>

namespace tjs
{
struct ClientHandler final : public Napi::ObjectWrap<ClientHandler> {
public:
    static Napi::Object init(Napi::Env env, Napi::Object exports)
    {
        constexpr auto class_name = "TonlibClient";

        Napi::Function function = DefineClass(
            env,
            class_name,
            {
                InstanceMethod("send", &ClientHandler::send),
                InstanceMethod("receive", &ClientHandler::receive),
                StaticMethod("execute", &ClientHandler::execute),
            });

        auto* constructor = new Napi::FunctionReference();
        *constructor = Napi::Persistent(function);
        env.SetInstanceData(constructor);

        exports.Set(class_name, function);
        return exports;
    }

    explicit ClientHandler(Napi::CallbackInfo& info)
        : Napi::ObjectWrap<ClientHandler>{info}
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

        auto slice = tonlib::ClientJson::execute(td::Slice{request.data(), request.size()});
        if (slice.empty()) {
            return env.Null();
        }

        return Napi::String::New(env, slice.data(), slice.size());
    }

    void send(const Napi::CallbackInfo& info)
    {
        auto env = info.Env();
        if (const auto length = info.Length(); length < 1 || !info[0].IsObject()) {
            Napi::TypeError::New(env, "Request object expected").ThrowAsJavaScriptException();
            return;
        }

        auto request = json_stringify(env, info[0].As<Napi::Object>()).Utf8Value();
        client_json_.send(td::Slice{request.data(), request.size()});
    }

    auto receive(const Napi::CallbackInfo& info) -> Napi::Value
    {
        auto env = info.Env();
        if (const auto length = info.Length(); length < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "Timeout number expected").ThrowAsJavaScriptException();
            return Napi::Value{};
        }

        auto slice = client_json_.receive(info[0].As<Napi::Number>().DoubleValue());
        if (slice.empty()) {
            return env.Null();
        }

        return json_parse(env, Napi::String::New(env, slice.data(), slice.size()));
    }

    tonlib::ClientJson client_json_;
};

Napi::Object init(Napi::Env env, Napi::Object exports)
{
    ClientHandler::init(env, exports);
    return exports;
}

NODE_API_MODULE(tonlib_js, init)

}  // namespace tjs
