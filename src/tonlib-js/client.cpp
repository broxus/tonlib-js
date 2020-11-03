#include "client.hpp"

#include "tonlib/TonlibCallback.h"
#include "tonlib/TonlibClient.h"

namespace tjs
{
class Client::Impl final {
public:
    using OutputQueue = td::MpscPollableQueue<Client::Response>;
    Impl()
    {
        class Callback final : public tonlib::TonlibCallback {
        public:
            explicit Callback() = default;
            void on_result(std::uint64_t id, tonlib_api::object_ptr<tonlib_api::Object> result) final {}
            void on_error(std::uint64_t id, tonlib_api::object_ptr<tonlib_api::error> error) final {}
            Callback(const Callback&) = delete;
            Callback& operator=(const Callback&) = delete;
            Callback(Callback&&) = delete;
            Callback& operator=(Callback&&) = delete;
        };

        scheduler_.run_in_context([&] {
            tonlib_ = td::actor::create_actor<tonlib::TonlibClient>(td::actor::ActorOptions().with_name("Tonlib").with_poll(), td::make_unique<Callback>());
        });
        scheduler_thread_ = td::thread([&] { scheduler_.run(); });
    }

    void send(Client::Request request, td::Promise<Client::Response>&& promise)
    {
        if (request == nullptr) {
            promise.set_error(td::Status::Error("Invalid request"));
            return;
        }

        scheduler_.run_in_context_external(
            [&] { td::actor::send_closure(tonlib_, &tonlib::TonlibClient::request_async, std::move(request), std::move(promise)); });
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
    ~Impl()
    {
        LOG(ERROR) << "~Impl";
        scheduler_.run_in_context_external([&] { tonlib_.reset(); });
        LOG(ERROR) << "Stop";
        scheduler_.run_in_context_external([] { td::actor::SchedulerContext::get()->stop(); });
        LOG(ERROR) << "join";
        scheduler_thread_.join();
        LOG(ERROR) << "join - done";
    }

private:
    bool is_closed_{false};

    td::actor::Scheduler scheduler_{{1}};
    td::thread scheduler_thread_;
    td::actor::ActorOwn<tonlib::TonlibClient> tonlib_;
};

Client::Client()
    : impl_(std::make_unique<Impl>())
{
}

void Client::send(Client::Request&& request, td::Promise<Response>&& response)
{
    impl_->send(std::move(request), std::move(response));
}

Client::Response Client::execute(Client::Request&& request)
{
    return tonlib::TonlibClient::static_request(std::move(request));
}

Client::~Client() = default;
Client::Client(Client&& other) noexcept = default;
Client& Client::operator=(Client&& other) noexcept = default;

}  // namespace tjs
