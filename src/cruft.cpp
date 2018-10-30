#include <cassert>
#include <sstream>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <cruft.h>
#include <ps_config_updater.h>

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
//    Interdrive move attempt causes copy which is unable to displace a/the running executable.
void
cruft_rename_file_file(
	std::string src_filename,
	std::string dst_filename)
{
	BOOL ok = MoveFileEx(src_filename.c_str(), dst_filename.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
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

int
cruft_exec_file_lowlevel(
	const std::string &exec_filename,
	const std::vector<std::string> &args,
	const std::chrono::milliseconds &wait_ms)
{
	std::stringstream ss;
	ss << "\"" << exec_filename << "\"";
	for (const auto &arg : args)
		ss << " " << "\"" << arg << "\"";
	ss.write("\0", 1);

	PROCESS_INFORMATION pi = {};
	STARTUPINFO si = {};
	si.cb = sizeof si;

	std::unique_ptr<HANDLE, void (*)(HANDLE *)> pt(&pi.hThread, [](HANDLE *p) { if (*p && !CloseHandle(*p)) assert(0); });
	std::unique_ptr<HANDLE, void (*)(HANDLE *)> pp(&pi.hProcess, [](HANDLE *p) { if (*p && !CloseHandle(*p)) assert(0); });

	if (!CreateProcess(exec_filename.c_str(), (LPSTR) ss.str().data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		throw std::runtime_error("process exec file");
	if (!wait_ms.count())
		return 0;
	DWORD exitcode = 0;
	if (WaitForSingleObject(pi.hProcess, (DWORD)(wait_ms.count() == 0xFFFFFFFF ? INFINITE : wait_ms.count())) != WAIT_OBJECT_0 ||
		GetExitCodeProcess(pi.hProcess, &exitcode) == 0)
	{
		throw std::runtime_error("process exec code");
	}
	return exitcode;
}

void
cruft_exec_file_expecting(std::string exec_filename, int ret_expected)
{
	if (cruft_exec_file_lowlevel(exec_filename, {}, std::chrono::milliseconds(0xFFFFFFFF)) != ret_expected)
		throw std::runtime_error("process exec code check");
}

void
cruft_exec_file_expecting_ex(
	std::string exec_filename,
	std::string arg_opt,
	std::chrono::milliseconds wait_ms,
	int ret_expected)
{
	if (cruft_exec_file_lowlevel(exec_filename, { arg_opt }, wait_ms) != ret_expected)
		throw std::runtime_error("process exec code check");
}

void
cruft_debug_wait()
{
	MessageBoxA(NULL, "Attach Debugger", cruft_current_executable_filename().c_str(), MB_OK);
	if (IsDebuggerPresent())
		__debugbreak();
}

boost::property_tree::ptree
cruft_config_read()
{
	std::stringstream ss(getenv("PS_CONFIG") ? getenv("PS_CONFIG") : std::string(g_ps_config, sizeof g_ps_config));

	boost::property_tree::ptree pt;
	boost::property_tree::json_parser::read_json(ss, pt);

	if (pt.get<std::string>("DEBUG_WAIT", "OFF") != "OFF")
		cruft_debug_wait();

	return pt;
}
