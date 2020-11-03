#pragma once

#include <auto/tl/tonlib_api.h>
#include <td/actor/actor.h>

namespace tonlib_api = ton::tonlib_api;

namespace tjs
{
class Client final {
public:
    using Request = tonlib_api::object_ptr<tonlib_api::Function>;
    using Response = tonlib_api::object_ptr<tonlib_api::Object>;

    Client();

    void send(Request&& request, td::Promise<Response>&& response);
    static Response execute(Request&& request);

    ~Client();
    Client(Client&& other) noexcept;
    Client& operator=(Client&& other) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tjs
