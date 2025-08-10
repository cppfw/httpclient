/*
MIT License

Copyright (c) 2020-2023 Ivan Gagis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* ================ LICENSE END ================ */

#include "init_guard.hpp"

#include <map>
#include <thread>

#include <curl/curl.h>
#include <nitki/queue.hpp>
#include <nitki/semaphore.hpp>

using namespace httpclient;

decltype(init_guard::instance) init_guard::instance;

// TODO: refactor to avoid non-const globals, perhaps make them static vars of some class
namespace {
std::thread thread; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic_bool quit_flag; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
nitki::queue queue; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
CURLM* multi_handle = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::map<CURL*, std::shared_ptr<request>>
	handle_to_request_map; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

namespace {
status_code curlcode_to_status(CURLcode code)
{
	switch (code) {
		case CURLE_OK:
			return status_code::ok;
		default:
			LOG([&](auto& o) {
				o << "CURLcode = " << code << std::endl;
			})
			return status_code::network_error;
	}
}
} // namespace

void init_guard::handle_completed_request(const void* curlmsg_message)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, "type erasure")
	const CURLMsg& m = *reinterpret_cast<const CURLMsg*>(curlmsg_message);
	switch (m.msg) {
		case CURLMSG_DONE:
			{
				CURL* handle = m.easy_handle;
				curl_multi_remove_handle(multi_handle, handle);
				auto i = handle_to_request_map.find(handle);
				ASSERT(i != handle_to_request_map.end())
				auto r = std::move(i->second);
				handle_to_request_map.erase(i);
				r->is_idle = true;

				long response_code{};
				curl_easy_getinfo(r->CURL_handle, CURLINFO_RESPONSE_CODE, &response_code);
				r->resp.status = httpmodel::status(response_code);

				if (r->completed_handler) {
					r->completed_handler(
						// NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access, "use of third party library libcurl")
						curlcode_to_status(m.data.result), //
						*r
					);
				}
				break;
			}
		default:
			ASSERT(false, [&](auto& o) {
				o << "m.msg = " << m.msg;
			})
			break;
	}
}

void init_guard::thread_func()
{
	while (!quit_flag.load()) {
		while (auto m = queue.pop_front()) {
			m();
		}

		int num_active_sockets{};
		curl_multi_perform(multi_handle, &num_active_sockets);

		// handle completed requests
		CURLMsg* msg = nullptr;
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while, "TODO: refactor to avoid do{}while()")
		do {
			int num_messages_left{};
			msg = curl_multi_info_read(multi_handle, &num_messages_left);
			if (msg) {
				init_guard::handle_completed_request(msg);
			}
		} while (msg);

		long timeout{};

		constexpr auto default_timeout_ms = 1000;

		curl_multi_timeout(multi_handle, &timeout);
		if (timeout < 0) { // no set timeout, use default
			timeout = default_timeout_ms;
		} else {
			using std::min;
			timeout = min(timeout, decltype(timeout)(default_timeout_ms));
		}

		CURLMcode rc = curl_multi_poll(
			multi_handle, //
			nullptr,
			0,
			int(timeout),
			nullptr
		);

		if (rc != CURLM_OK) {
			utki::log([](auto& o) {
				o << "curl_multi_poll() failed" << std::endl;
			});
			break;
		}
	}

	curl_multi_cleanup(multi_handle);
	multi_handle = nullptr;

	curl_global_cleanup();
}

void init_guard::start_request(std::shared_ptr<request> r)
{
	queue.push_back([r]() {
		curl_multi_add_handle(multi_handle, r->CURL_handle);
		handle_to_request_map.insert(std::make_pair(r->CURL_handle, std::move(r)));
	});
	curl_multi_wakeup(multi_handle);
}

bool init_guard::cancel_request(request& r)
{
	// TRACE(<< "cancelling request..." << std::endl)
	nitki::semaphore sema;
	bool ret = false;
	queue.push_back([&r, &sema, &ret]() {
		auto i = handle_to_request_map.find(r.CURL_handle);
		if (i == handle_to_request_map.end()) {
			// TRACE(<< "request is not active" << std::endl)
			ret = false;
		} else {
			curl_multi_remove_handle(multi_handle, r.CURL_handle);
			handle_to_request_map.erase(i);
			r.is_idle = true;
			ret = true;
			// TRACE(<< "request removed from handling" << std::endl)
		}
		sema.signal();
	});
	curl_multi_wakeup(multi_handle);
	sema.wait();
	// TRACE(<< "request cancelled..." << std::endl)
	return ret;
}

init_guard::init_guard(bool init_winsock)
{
	long flags = CURL_GLOBAL_SSL;
	if (init_winsock) {
		flags |= CURL_GLOBAL_WIN32;
	}
	curl_global_init(flags);

	multi_handle = curl_multi_init();

	quit_flag.store(false);
	thread = std::thread(&thread_func);
}

init_guard::~init_guard()
{
	quit_flag.store(true);
	ASSERT(multi_handle)
	curl_multi_wakeup(multi_handle);
	thread.join();
}
