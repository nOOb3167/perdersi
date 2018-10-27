#include <cstdlib>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <cruft.h>

int main(int argc, char **argv)
{
#if _PS_DEBUG_TUPDATER == 2
	boost::filesystem::path tupdater3_path(cruft_config_read().get<std::string>("TUPDATER3_EXE"));
	if (!boost::filesystem::is_regular_file(tupdater3_path) || !boost::regex_search(tupdater3_path.string().c_str(), boost::cmatch(), boost::regex(".exe$")))
		throw std::runtime_error("path sanity");
	cruft_rename_file_selfexec(tupdater3_path.string(), cruft_current_executable_filename());
	cruft_exec_file_expecting(cruft_current_executable_filename(), 123);
	return EXIT_SUCCESS;
#elif _PS_DEBUG_TUPDATER == 3
	return 123;
#endif
}
