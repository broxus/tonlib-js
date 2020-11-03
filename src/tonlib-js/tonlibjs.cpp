#include <node_version.h>

//

#include <napi.h>
#include <node_api.h>
#include <td/utils/JsonBuilder.h>
#include <td/utils/Slice.h>
#include <td/utils/Status.h>
#include <td/utils/common.h>
#include <td/utils/format.h>
#include <td/utils/logging.h>
#include <td/utils/port/thread_local.h>

#include "client.hpp"
#include "gen/tonlib_napi.h"
#include "tl_napi.hpp"

namespace tjs
{
static td::Result<tonlib_api::object_ptr<tonlib_api::Function>> to_request(const Napi::Value& request)
{
    tonlib_api::object_ptr<tonlib_api::Function> func;
    TRY_STATUS(from_napi(request, func))
    return func;
}

struct ClientHandler final : public Napi::ObjectWrap<ClientHandler> {
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
                StaticMethod("execute", &ClientHandler::execute),
            });

        constructor = new Napi::FunctionReference();
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
    static auto execute(const Napi::CallbackInfo& info) -> Napi::Value
    {
        auto env = info.Env();

        const auto length = info.Length();
        if (length < 1 && !info[0].IsObject()) {
            Napi::TypeError::New(env, "Request object expected").ThrowAsJavaScriptException();
            return Napi::Value{};
        }

        auto r_request = to_request(info[0].As<Napi::Object>());
        if (r_request.is_error()) {
            const auto message = PSLICE() << "Failed to parse request: " << r_request.error();
            Napi::Error::New(env, message.c_str()).ThrowAsJavaScriptException();
            return env.Null();
        }

        auto result = Client::execute(r_request.move_as_ok());
        return to_napi(env, result);
    }

    auto send(const Napi::CallbackInfo& info) -> Napi::Value
    {
        auto env = info.Env();

        const auto length = info.Length();
        if (length < 1 && !info[0].IsObject()) {
            Napi::TypeError::New(env, "Request object expected").ThrowAsJavaScriptException();
            return Napi::Value{};
        }

        auto r_request = to_request(info[0].As<Napi::Object>());
        if (r_request.is_error()) {
            const auto message = PSLICE() << "Failed to parse request: " << r_request.error();
            Napi::Error::New(env, message.c_str()).ThrowAsJavaScriptException();
            return env.Null();
        }

        auto deferred = Napi::Promise::Deferred::New(env);
        Napi::AsyncContext context(env, "tonlib_async_context");

        auto promise = deferred.Promise();

        auto P = td::PromiseCreator::lambda([deferred, context = std::move(context)](td::Result<Client::Response> R) {
            if (R.is_error()) {
                const auto error = Napi::String::New(context.Env(), R.move_as_error().message().str());
                deferred.Reject(error);
            }
            else {
                const auto result = to_napi(context.Env(), R.move_as_ok());
                deferred.Resolve(result);
            }
        });
        client_.send(r_request.move_as_ok(), std::move(P));

        return promise;
    }

    Client client_;
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
