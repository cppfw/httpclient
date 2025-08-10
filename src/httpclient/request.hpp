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

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <httpmodel/http.hpp>
#include <httpmodel/response.hpp>
#include <utki/span.hpp>

namespace httpclient {

enum class status_code {
	undefined, // TODO: remove
	ok,
	network_error,
	timeout,
	authentication_failed
};

/**
 * @brief HTTP request.
 * The lifespan of all request objects must be limited by lifespan of init_guard singleton.
 * Otherwise the behavior is undefined.
 */
class request : public std::enable_shared_from_this<request>
{
	friend class init_guard;

	void* CURL_handle;
	void* curl_slist_handle = nullptr;

	volatile bool is_idle = true;

	httpmodel::response resp;

	std::function<void(status_code, request&)> completed_handler;

	std::function<size_t(utki::span<const uint8_t>)> data_handler;

	static size_t write_data(void* buffer, size_t size, size_t nmemb, void* userp);

	void free_headers() noexcept;

public:
	request(decltype(completed_handler)&& ch);

	request(const request&) = delete;
	request& operator=(const request&) = delete;

	request(request&&) = delete;
	request& operator=(request&&) = delete;

	~request();

	void start();

	const httpmodel::response& get_response() const
	{
		return this->resp;
	}

	/**
	 * @brief Cancel active request.
	 * @return true if request was cancelled and no completed callback was called.
	 * @return false in case the request has completed befere cancelling or was not active at all.
	 */
	bool cancel() noexcept;

	void set_method(httpmodel::method m);

	void set_url(const std::string& url);

	void set_data_handler(decltype(data_handler)&& handler);

	void set_headers(
		const std::map<std::string, std::string>& name_value,
		utki::span<const std::string> name = utki::span<const std::string>()
	);
};

} // namespace httpclient
