#include <cstdlib>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/regex.hpp>

#include <cruft.h>

#if _PS_DEBUG_TUPDATER != 2 && _PS_DEBUG_TUPDATER != 3
#error
#endif

using pt_t = ::boost::property_tree::ptree;

int main(int argc, char **argv)
{
	pt_t config = cruft_config_read();

#if _PS_DEBUG_TUPDATER == 2
	boost::filesystem::path tupdater2_path(cruft_current_executable_filename());
	boost::filesystem::path tupdater2_path_old(tupdater2_path);
	tupdater2_path_old.replace_extension(".old");
	boost::filesystem::path tupdater3_path(config.get<std::string>("TUPDATER3_EXE"));
	if (!boost::filesystem::is_regular_file(tupdater2_path) ||
		!boost::filesystem::is_regular_file(tupdater3_path) ||
		!boost::regex_search(tupdater2_path.string().c_str(), boost::cmatch(), boost::regex(".exe$")) ||
		!boost::regex_search(tupdater3_path.string().c_str(), boost::cmatch(), boost::regex(".exe$")))
	{
		throw std::runtime_error("path sanity");
	}
	cruft_rename_file_file(tupdater2_path.string(), tupdater2_path_old.string());
	cruft_rename_file_file(tupdater3_path.string(), tupdater2_path.string());
	boost::process::child reexec_process(tupdater2_path);
	reexec_process.wait();
	if (reexec_process.exit_code() != 123)
		throw std::runtime_error("process exit code");
#elif _PS_DEBUG_TUPDATER == 3
	return 123;
#endif

	return EXIT_SUCCESS;
}
