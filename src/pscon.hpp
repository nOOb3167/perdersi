#ifndef _Con_HPP_
#define _Con_HPP_

#include <cassert>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <psasio.hpp>
#include <pscruft.hpp>
#include <psgit.hpp>
#include <psmisc.hpp>

using tcp = ::boost::asio::ip::tcp;
namespace http = ::boost::beast::http;

using res_t = ::http::response<http::string_body>;

namespace ps
{

class ConExc : public std::runtime_error
{
public:
	inline ConExc() :
		std::runtime_error("error")
	{}
};

class con_tag_ratio_t {};

class ConEst
{
public:
	inline ConEst(float a, float b, con_tag_ratio_t) :
		m_r_a(a),
		m_r_b(b)
	{}

	float m_r_a, m_r_b;
};

class ConProgress
{
public:
	inline void setRepo(const sp<git_repository> &repo) { m_repo = repo; }

	inline void setObjectsList(const std::vector<shahex_t> &objs)
	{
		assert(m_repo);
		std::set<shahex_t> all;
		for (const auto &obj : objs)
			all.insert(obj);
		std::set<shahex_t> mis;
		for (const auto &obj : objs)
			if (!git_odb_exists(odb_from_repo(m_repo.get()).get(), git_hex2bin(obj)))
				mis.insert(obj);
		m_objects_all = std::move(all);
		m_objects_missing = std::move(mis);
	}

	inline ConEst doEstimate()
	{
		return ConEst(m_objects_missing.size(), m_objects_all.size(), con_tag_ratio_t());
	}

	inline void onRequest(const std::string &path, const std::string &data)
	{
		std::lock_guard<std::mutex> l(m_mtx);
		boost::cmatch what;
		if (boost::regex_search(path.c_str(), what, boost::regex("/objects/([[:xdigit:]]{2})/([[:xdigit:]]{38})"), boost::match_default))
			m_objects_requested.push_back(boost::algorithm::to_lower_copy(what[1].str() + what[2].str()));
	}


	std::mutex m_mtx;
	sp<git_repository> m_repo;
	std::set<shahex_t> m_objects_all;
	std::set<shahex_t> m_objects_missing;
	std::vector<shahex_t> m_objects_requested;
};

class Con
{
public:
	inline virtual ~Con() {};
	inline virtual res_t reqPost(const std::string &path, const std::string &data) = 0;
	
	ConProgress m_prog;
};

class ConNet : public Con
{
public:
	inline ConNet(const std::string &host, const std::string &port, const std::string &host_http_rootpath) :
		Con(),
		m_host(host),
		m_port(port),
		m_host_http(host + ":" + port),
		m_host_http_rootpath(host_http_rootpath),
		m_ioc(),
		m_resolver(m_ioc),
		m_resolver_r(m_resolver.resolve(host, port)),
		m_socket(new tcp::socket(m_ioc))
	{
		boost::asio::connect(*m_socket, m_resolver_r.begin(), m_resolver_r.end());
	};

	inline ~ConNet()
	{
		m_socket->shutdown(tcp::socket::shutdown_both);
	}

	inline void _reconnect()
	{
		m_socket.reset(new tcp::socket(m_ioc));
		boost::asio::connect(*m_socket, m_resolver_r.begin(), m_resolver_r.end());
	}

	inline res_t reqPost_(const std::string &path, const std::string &data)
	{
		http::request<http::string_body> req(http::verb::post, m_host_http_rootpath + path, 11);
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

	inline virtual res_t reqPost(const std::string &path, const std::string &data) override
	{
		m_prog.onRequest(path, data);
		res_t res = reqPost_(path, data);
		if (res.result_int() != 200)
			throw ConExc();
		return res;
	}

	std::string m_host;
	std::string m_port;
	std::string m_host_http;
	std::string m_host_http_rootpath;
	boost::asio::io_context m_ioc;
	tcp::resolver m_resolver;
	tcp::resolver::results_type m_resolver_r;
	sp<tcp::socket> m_socket;
};

class ConFs : public Con
{
public:
	inline ConFs(const std::string &gitdir) :
		Con(),
		m_gitdir(gitdir),
		m_objdir(m_gitdir / "objects"),
		m_refdir(m_gitdir / "refs")
	{
		if (!boost::filesystem::exists(m_gitdir) ||
			!boost::filesystem::exists(m_objdir) ||
			!boost::filesystem::exists(m_refdir))
		{
			throw ConExc();
		}
	};

	inline ~ConFs()
	{
	}

	inline virtual res_t reqPost(const std::string &path, const std::string &data) override
	{
		m_prog.onRequest(path, data);
		return res_t(boost::beast::http::status::ok, 11, ps::cruft_file_read(m_gitdir / path));
	}

	boost::filesystem::path m_gitdir;
	boost::filesystem::path m_objdir;
	boost::filesystem::path m_refdir;
};

}

#endif /* _Con_HPP_ */
