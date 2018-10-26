#include <cstdlib>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/regex.hpp>

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>

std::string current_executable_filename()
{
	std::string fname(1024, '\0');

	DWORD LenFileName = GetModuleFileName(NULL, (char *) fname.data(), (DWORD)fname.size());
	if (!(LenFileName != 0 && LenFileName < fname.size()))
		throw std::runtime_error("current executable filename");
	fname.resize(LenFileName);

	return fname;
}

// https://stackoverflow.com/questions/3156841/boostfilesystemrename-cannot-create-a-file-when-that-file-already-exists
// https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-movefileexa
//    If the destination is on another drive, you must set the MOVEFILE_COPY_ALLOWED flag in dwFlags.
//    (Running executable cannot replace itself by copy)
void rename_file_file(
	std::string src_filename,
	std::string dst_filename)
{
	BOOL ok = MoveFileEx(src_filename.c_str(), dst_filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	if (!ok)
		throw std::runtime_error("rename");
}
#endif

#if _PS_DEBUG_TUPDATER != 2 && _PS_DEBUG_TUPDATER != 3
#error
#endif

using pt_t = ::boost::property_tree::ptree;

pt_t readconfig()
{
	if (!getenv("PS_CONFIG"))
		throw std::runtime_error("config");
	std::stringstream ss(getenv("PS_CONFIG"));
	boost::property_tree::ptree pt;
	boost::property_tree::json_parser::read_json(ss, pt);
	return pt;
}

int main(int argc, char **argv)
{
	pt_t config = readconfig();

#if _PS_DEBUG_TUPDATER == 2
	boost::filesystem::path tupdater2_path(current_executable_filename());
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
	rename_file_file(tupdater2_path.string(), tupdater2_path_old.string());
	rename_file_file(tupdater3_path.string(), tupdater2_path.string());
	boost::process::child reexec_process(tupdater2_path);
	reexec_process.wait();
	if (reexec_process.exit_code() != 123)
		throw std::runtime_error("process exit code");
#elif _PS_DEBUG_TUPDATER == 3
	return 123;
#endif

	return EXIT_SUCCESS;
}
