#include <tst/set.hpp>
#include <tst/check.hpp>

#include <utki/string.hpp>

#include <nitki/semaphore.hpp>

#include <thread>
#include <optional>

#include "../../src/httpclient/request.hpp"
#include "../../src/httpclient/init_guard.hpp"

namespace{
const tst::set set("basic", [](tst::suite& suite){
    suite.add(
        "basic_request",
        [](){
            nitki::semaphore completed;

            std::vector<uint8_t> data;

            httpclient::status_code req_status = httpclient::status_code::undefined;

            auto r = std::make_shared<httpclient::request>([&](httpclient::status_code sc, httpclient::request& r){
                // this callback is run from separate thread, so do not use tst::check() inside
                req_status = sc;
                data = std::move(r.get_response().body);
                completed.signal();
            });

            r->set_url("http://izdelie.icu");

            r->start();

            completed.wait();

            tst::check(req_status == httpclient::status_code::ok, SL) << "status = " << unsigned(req_status);

            LOG([&](auto&o){o << "body.size() = " << data.size() << std::endl;});
            LOG([&](auto&o){o << utki::make_string(data) << std::endl;});
        }
    );

    suite.add(
        "cancel_non_active_request",
        [](){
            auto r = std::make_shared<httpclient::request>([](httpclient::status_code sc, httpclient::request& r){});

            r->set_url("http://izdelie.icu");

            tst::check(!r->cancel(), SL);
        }
    );

    suite.add(
        "cancel_active_request",
        [](){
            bool completed = false;

            nitki::semaphore sema;

            httpclient::status_code req_status = httpclient::status_code::undefined;

            auto r = std::make_shared<httpclient::request>([&](httpclient::status_code sc, httpclient::request& req){
                completed = true;
                auto r = req.get_response();
                utki::log_debug([&](auto&o){
                    o << "HTTP request completed, status = " << unsigned(sc);
                    o << ", http code = " << unsigned(r.status) << std::endl;
                });
                req_status = sc;
                sema.signal();
            });

            r->set_url("http://212.183.159.230/1GB.zip");

            bool sema_signalled = false;

            r->set_data_handler([&sema, &sema_signalled](utki::span<const uint8_t> d) -> size_t {
                utki::log_debug([&](auto&o){
                    o << "first data chunk received, " << d.size() << " bytes, cancelling the request..." << std::endl;
                    o << utki::make_string(d) << std::endl;
                });
                if(!sema_signalled){
                    sema_signalled = true;
                    sema.signal();
                }
                return d.size();
            });

            utki::log_debug([](auto&o){o << "running cancel test..." << std::endl;});
            r->start();

            sema.wait();

            constexpr auto delay_ms = 50;
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

            tst::check(!completed, SL) << "req_status = " << unsigned(req_status);
            tst::check(r->cancel(), SL);
            tst::check(!completed, SL) << "req_status = " << unsigned(req_status);

            sema_signalled = false; // allow signalling on data received

            utki::log_debug([](auto&o){o << "waiting 1 second to check the request was cancelled and no more data is received" << std::endl;});
            constexpr auto wait_timeout_ms = 1000;
            auto wait_res = sema.wait(wait_timeout_ms);
            tst::check(!wait_res, SL);
            utki::log_debug([](auto&o){o << "cancel test completed" << std::endl;});

            // since we are cancelling the requet, we expect that request completed handler was not called,
            // so the request status should remain undefined
            tst::check(req_status == httpclient::status_code::undefined, SL) << "req_status = " << unsigned(req_status);
        }
    );

    suite.add(
        "set_headers",
        [](){
            nitki::semaphore completed;

            std::vector<uint8_t> data;

            auto req_status = httpclient::status_code::undefined;
            std::optional<httpmodel::status> resp_code;

            auto r = std::make_shared<httpclient::request>([&](httpclient::status_code sc, httpclient::request& r){
                req_status = sc;
                resp_code = r.get_response().status;
                data = std::move(r.get_response().body);
                completed.signal();
            });

            r->set_url("https://postman-echo.com/headers");

            std::string custom_header_value = "Lorem ipsum dolor sit amet";

            r->set_headers({
                    {"my-sample-header", custom_header_value},
                    {"Accept", "application/json"},
                    {"Content-Type", "application/json"}
                });

            r->start();

            completed.wait();

            tst::check(req_status == httpclient::status_code::ok, SL) << "req_status = " << unsigned(req_status);
            tst::check(resp_code.has_value(), SL);
            tst::check(resp_code.value() == httpmodel::status::http_200_ok, SL) << "http_code = " << unsigned(resp_code.value());

            auto resp_str = utki::make_string(data);

            tst::check_ne(resp_str.find(custom_header_value), std::string::npos, SL)
                    << "expected string not found in response body: " << resp_str;

            LOG([&](auto&o){o << "body.size() = " << data.size() << std::endl;});
            LOG([&](auto&o){o << resp_str << std::endl;});
        }
    );
});
}
