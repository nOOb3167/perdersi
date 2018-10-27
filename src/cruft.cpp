#include <sstream>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/regex.hpp>

#include <cruft.h>

#ifdef _WIN32
#include <windows.h>
#endif

std::string
cruft_current_executable_filename()
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
void
cruft_rename_file_file(
	std::string src_filename,
	std::string dst_filename)
{
	BOOL ok = MoveFileEx(src_filename.c_str(), dst_filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	if (!ok)
		throw std::runtime_error("rename");
}

void
cruft_rename_file_selfexec(
	std::string src_filename,
	std::string dst_filename)
{
	if (!boost::regex_search(src_filename.c_str(), boost::cmatch(), boost::regex(".exe$")) || !boost::regex_search(dst_filename.c_str(), boost::cmatch(), boost::regex(".exe$")))
		std::runtime_error("reexec name match");
	cruft_rename_file_file(dst_filename, boost::filesystem::path(dst_filename).replace_extension(".old").string());
	cruft_rename_file_file(src_filename, dst_filename);
}

void
cruft_exec_file_expecting(std::string exec_filename,int ret_expected)
{
	boost::process::child process(exec_filename);
	process.wait();
	if (process.exit_code() != ret_expected)
		throw std::runtime_error("process exit code");
}

void
cruft_exec_file_expecting_ex(
	std::string exec_filename,
	std::string arg_opt,
	std::chrono::milliseconds wait_ms,
	int ret_expected)
{
	boost::process::child process(exec_filename, arg_opt);
	process.wait_for(wait_ms);
	if (process.running() || process.exit_code() != ret_expected)
		throw std::runtime_error("process exit code");
}

void
cruft_debug_wait()
{
	MessageBoxA(NULL, "Attach Debugger", cruft_current_executable_filename().c_str(), MB_OK);
	if (IsDebuggerPresent())
		DebugBreak();
}

boost::property_tree::ptree
cruft_config_read()
{
	if (! getenv("PS_CONFIG"))
		throw std::runtime_error("config");
	std::stringstream ss(getenv("PS_CONFIG"));
	boost::property_tree::ptree pt;
	boost::property_tree::json_parser::read_json(ss, pt);

	if (pt.get<std::string>("PS_DEBUG_WAIT", "false") != "false")
		cruft_debug_wait();

	return pt;
}
