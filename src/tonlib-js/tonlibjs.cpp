#include "tonlibjs.hpp"

#include <napi.h>
#include <nan.h>

namespace tjs
{
Napi::String hello(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    return Napi::String::New(env, "world");
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports.Set(Napi::String::New(env, "hello"), Napi::Function::New(env, hello));
    return exports;
}

}  // namespace tjs

NODE_API_MODULE(tonlib_js, tjs::Init)
