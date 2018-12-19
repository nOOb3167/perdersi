#ifndef _Thr_HPP_
#define _Thr_HPP_

#include <cassert>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <stdexcept>

#include <pscruft.hpp>
#include <pscon.hpp>
#include <psgit.hpp>
#include <psupdater.hpp>

namespace ps
{

class ThrBase
{
public:
	ThrBase() :
		m_mtx(),
		m_thr(),
		m_exc(),
		m_dead(false)
	{}

	~ThrBase()
	{
		// FIXME: side-effecting destructor
		join();
	}

	void start()
	{
		m_thr = std::thread(std::bind(&ThrBase::tfunc, this));
	}

	void join()
	{
		if (m_thr.joinable())
			m_thr.join();
		assert(m_dead);
		if (m_exc)
			std::rethrow_exception(m_exc);
	}

	bool isdead()
	{
		std::lock_guard<std::mutex> l(m_mtx);
		return m_dead;
	}

	void tfunc()
	{
		try {
			run();
		} catch (std::exception &) {
			m_exc = std::current_exception();
		}
		std::lock_guard<std::mutex> l(m_mtx);
		m_dead = true;
	}

	virtual void run() = 0;

	std::mutex m_mtx;
	std::thread m_thr;
	std::exception_ptr m_exc;
	bool m_dead;
};

class Thr : public ThrBase
{
public:
	Thr(const pt_t &config, const sp<PsCon> &client) :
		ThrBase(),
		m_config(config),
		m_client(client)
	{}

	static up<Thr> create(const pt_t &config, const sp<PsCon> &client)
	{
		up<Thr> r(new Thr(config, client));
		r->start();
		return r;
	}

	virtual void run() override
	{
		unique_ptr_gitrepository repo(ns_git::repository_ensure(m_config.get<std::string>("REPO_DIR")));

		const boost::filesystem::path chkoutdir = cruft_config_get_path(m_config, "REPO_CHK_DIR");
		const boost::filesystem::path stage2path = chkoutdir / m_config.get<std::string>("UPDATER_STAGE2_EXE_RELATIVE");
		const std::string updatr = m_config.get<std::string>("UPDATER_EXE_RELATIVE");

		const shahex_t head = updater_head_get(m_client.get(), "master");

		std::cout << "repodir: " << git_repository_path(repo.get()) << std::endl;
		std::cout << "chkodir: " << chkoutdir.string() << std::endl;
		std::cout << "stage2p: " << stage2path.string() << std::endl;
		std::cout << "updatr: " << updatr << std::endl;
		std::cout << "head: " << head << std::endl;

		const std::vector<shahex_t> trees = updater_trees_get_writing_recursive(m_client.get(), repo.get(), head);
		const std::vector<shahex_t> blobs = updater_blobs_list(repo.get(), trees);
		updater_blobs_get_writing(m_client.get(), repo.get(), blobs);
		ns_git::checkout_obj(repo.get(), head, chkoutdir.string());

		updater_replace_cond(m_config.get<int>("ARG_SKIPSELFUPDATE"), repo.get(), head, updatr, cruft_current_executable_filename(), stage2path);
	}

	pt_t m_config;
	sp<PsCon> m_client;
};

}

#endif /* _Thr_HPP_ */
