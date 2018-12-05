#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <git2.h>
#include <miniz.h>

#include <pscruft.hpp>
#include <psmisc.hpp>
#include <pscon.hpp>
#include <psgit.hpp>
#include <psupdater.hpp>

using namespace ps;

int main(int argc, char **argv)
{
	git_libgit2_init();

	const auto [arg_tryout, arg_skipselfupdate] = updater_argv_parse(argc, argv);

	if (arg_tryout)
		return 123;

	pt_t config = cruft_config_read();
	boost::filesystem::path chkoutdir = cruft_config_get_path(config, "REPO_CHK_DIR");
	boost::filesystem::path stage2path = chkoutdir / config.get<std::string>("UPDATER_STAGE2_EXE_RELATIVE");
	std::string updatr = config.get<std::string>("UPDATER_EXE_RELATIVE");

	unique_ptr_gitrepository repo(ns_git::repository_ensure(config.get<std::string>("REPO_DIR")));

	PsConTest client(config.get<std::string>("ORIGIN_DOMAIN_API"), config.get<std::string>("LISTEN_PORT"), "");
	shahex_t head = updater_head_get(&client, "master");

	std::cout << "repodir: " << git_repository_path(repo.get()) << std::endl;
	std::cout << "chkodir: " << chkoutdir.string() << std::endl;
	std::cout << "stage2p: " << stage2path.string() << std::endl;
	std::cout << "updatr: " << updatr << std::endl;
	std::cout << "head: " << head << std::endl;

	std::vector<shahex_t> trees = updater_trees_get_writing_recursive(&client, repo.get(), head);
	std::vector<shahex_t> blobs = updater_blobs_list(repo.get(), trees);
	updater_blobs_get_writing(&client, repo.get(), blobs);
	ns_git::checkout_obj(repo.get(), head,	chkoutdir.string());

	if (!arg_skipselfupdate && updater_running_exe_content_file_replace_ensure(updater_tree_entry_blob_content(repo.get(), head, updatr), cruft_current_executable_filename()))
		cruft_exec_file_lowlevel(cruft_current_executable_filename(), { "--skipselfupdate" }, std::chrono::milliseconds(0));
	else
		cruft_exec_file_lowlevel(stage2path.string(), {}, std::chrono::milliseconds(0));

	return EXIT_SUCCESS;
}
