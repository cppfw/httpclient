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

#include "util.hpp"

#include <cstring>

#include <curl/curl.h>
#include <utki/util.hpp>

using namespace httpclient;

std::string httpclient::escape(const std::string& str)
{
	// TODO: implement conversion of any size strings
	if (str.size() > size_t(std::numeric_limits<int>::max())) {
		// curl_easy_escape() only supports length as int.
		throw std::logic_error("httpclient::escape(str): string too long");
	}

	auto curl = curl_easy_init();
	utki::scope_exit curl_scope_exit([curl]() {
		curl_easy_cleanup(curl);
	});

	auto encoded = curl_easy_escape(
		curl, //
		str.data(),
		int(str.size())
	);
	utki::scope_exit encoded_scope_exit([encoded]() {
		curl_free(encoded);
	});

	std::string ret(encoded, strlen(encoded));

	return ret;
}
