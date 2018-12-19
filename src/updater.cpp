#include <cassert>
#include <cstdlib>
#include <exception>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>
#include <git2.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <pscruft.hpp>
#include <psmisc.hpp>
#include <pscon.hpp>
#include <pssfml.hpp>
#include <psthr.hpp>
#include <psupdater.hpp>

using namespace ps;

int main(int argc, char **argv)
{
	git_libgit2_init();

	const auto [arg_tryout, arg_skipselfupdate, arg_fsmode] = updater_argv_parse(argc, argv);

	pt_t config = cruft_config_read();
	config.put("ARG_TRYOUT", (int)arg_tryout);
	config.put("ARG_SKIPSELFUPDATE", (int)arg_skipselfupdate);
	config.put("ARG_FSMODE", arg_fsmode);

	sp<PsCon> client;

	if (config.get<int>("ARG_TRYOUT"))
		return 123;

	client = config.get<std::string>("ARG_FSMODE") != "" ?
		sp<PsCon>(new PsConFs(config.get<std::string>("ARG_FSMODE"))) :
		sp<PsCon>(new PsConNet(config.get<std::string>("ORIGIN_DOMAIN_API"), config.get<std::string>("LISTEN_PORT"), ""));

	sp<Thr> thr(Thr::create(config, client));
	SfWin win(thr);

	win.run();

	return EXIT_SUCCESS;
}
