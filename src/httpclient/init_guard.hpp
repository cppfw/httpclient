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

#include <utki/singleton.hpp>

#include "request.hpp"

namespace httpclient {

class init_guard : public utki::intrusive_singleton<init_guard>
{
	friend class utki::intrusive_singleton<init_guard>;
	static init_guard::instance_type instance;

	friend class request;

	void start_request(std::shared_ptr<request> r);
	bool cancel_request(request& r);

	static void thread_func();
	static void handle_completed_request(const void* curlmsg_message);

public:
	init_guard(bool init_winsock = true);

	init_guard(const init_guard&) = delete;
	init_guard& operator=(const init_guard&) = delete;

	init_guard(init_guard&&) = delete;
	init_guard& operator=(init_guard&&) = delete;

	~init_guard() override;
};

} // namespace httpclient
