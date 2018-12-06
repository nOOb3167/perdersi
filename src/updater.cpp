#include <cstdlib>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>
#include <git2.h>
#include <SFML/Window.hpp>

#include <pscruft.hpp>
#include <psmisc.hpp>
#include <pscon.hpp>
#include <psgit.hpp>
#include <psupdater.hpp>

using namespace ps;

class PsThr
{
public:
	PsThr(const pt_t &config, bool arg_skipselfupdate, const sp<PsConTest> &client) :
		m_thr(),
		m_config(config),
		m_arg_skipselfupdate(arg_skipselfupdate),
		m_client(client)
	{}

	void start()
	{
		m_thr = std::thread(std::bind(&PsThr::tfunc, this));
	}

	void join()
	{
		m_thr.join();
		if (m_exc)
			std::rethrow_exception(m_exc);
	}

	void tfunc()
	{
		try {
			run();
		} catch (std::exception &) {
			m_exc = std::current_exception();
		}
	}

	void run()
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

		updater_replace_cond(m_arg_skipselfupdate, repo.get(), head, updatr, cruft_current_executable_filename(), stage2path);
	}

	std::thread m_thr;
	std::exception_ptr m_exc;
	pt_t m_config;
	bool m_arg_skipselfupdate;
	sp<PsConTest> m_client;
};

int main(int argc, char **argv)
{
	git_libgit2_init();

	const auto [arg_tryout, arg_skipselfupdate] = updater_argv_parse(argc, argv);

	if (arg_tryout)
		return 123;

	pt_t config = cruft_config_read();

	sp<PsConTest> client(new PsConTest(config.get<std::string>("ORIGIN_DOMAIN_API"), config.get<std::string>("LISTEN_PORT"), ""));
	PsThr thr(config, arg_skipselfupdate, client);
	thr.start();
	thr.join();

	return EXIT_SUCCESS;
}
