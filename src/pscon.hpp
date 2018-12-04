#ifndef _PSCON_HPP_
#define _PSCON_HPP_

#include <string>
#include <vector>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/beast.hpp>
#include <boost/regex.hpp>

#include <psasio.hpp>
#include <psmisc.hpp>

using tcp = ::boost::asio::ip::tcp;
namespace http = ::boost::beast::http;

using res_t = ::http::response<http::string_body>;

class PsConExc : public std::runtime_error
{
public:
	inline PsConExc() :
		std::runtime_error("error")
	{}
};

class PsCon
{
public:
	inline PsCon(const std::string &host, const std::string &port) :
		m_host(host),
		m_port(port),
		m_host_http(host + ":" + port),
		m_ioc(),
		m_resolver(m_ioc),
		m_resolver_r(m_resolver.resolve(host, port)),
		m_socket(new tcp::socket(m_ioc))
	{
		boost::asio::connect(*m_socket, m_resolver_r.begin(), m_resolver_r.end());
	};

	inline ~PsCon()
	{
		m_socket->shutdown(tcp::socket::shutdown_both);
	}

	inline void _reconnect()
	{
		m_socket.reset(new tcp::socket(m_ioc));
		boost::asio::connect(*m_socket, m_resolver_r.begin(), m_resolver_r.end());
	}

	inline res_t reqPost(const std::string &path, const std::string &data)
	{
		http::request<http::string_body> req(http::verb::post, path, 11);
		req.set(http::field::host, m_host_http);
		req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
		http::write(*m_socket, req);
		boost::beast::flat_buffer buffer;
		http::response<http::string_body> res;
		http::read(*m_socket, buffer, res);
		// https://github.com/boostorg/beast/issues/927
		//   Repeated calls to an URL (repeated http::write calls without remaking the socket)
		//     - needs http::response::keep_alive() true
		//     - (for which http::response::version() must be 11 (HTTP 1.1))
		// https://www.reddit.com/r/flask/comments/634i5u/make_flask_return_header_response_with_http11/
		//   You can't. Flask's dev server does not implement the HTTP 1.1 spec
		//     - flask does not support HTTP 1.1, remake socket if necessary
		if (!res.keep_alive())
			_reconnect();
		return res;
	}

	inline virtual res_t reqPost_(const std::string &path, const std::string &data)
	{
		res_t res = reqPost(path, data);
		if (res.result_int() != 200)
			throw PsConExc();
		return res;
	}

	std::string m_host;
	std::string m_port;
	std::string m_host_http;
	boost::asio::io_context m_ioc;
	tcp::resolver m_resolver;
	tcp::resolver::results_type m_resolver_r;
	sp<tcp::socket> m_socket;
};

class PsConTest : public PsCon
{
public:
	inline PsConTest(const std::string &host, const std::string &port) :
		PsCon(host, port)
	{};

	inline res_t reqPost_(const std::string &path, const std::string &data) override
	{
		boost::cmatch what;
		if (boost::regex_search(path.c_str(), what, boost::regex("/objects/([[:xdigit:]]{2})/([[:xdigit:]]{38})"), boost::match_continuous))
			m_objects_requested.push_back(boost::algorithm::to_lower_copy(what[1].str() + what[2].str()));
		return PsCon::reqPost_(path, data);
	}

	std::vector<shahex_t> m_objects_requested;
};

#endif /* _PSCON_HPP_ */
