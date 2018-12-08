#ifndef _PS_CRUFT_HPP_
#define _PS_CRUFT_HPP_

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/regex.hpp>
#include <chrono>

#include <psasio.hpp>
#include <ps_config_updater.h>

using pt_t = ::boost::property_tree::ptree;

namespace ps
{

inline std::string
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
inline void
cruft_rename_file_file(
	const std::string &src_filename,
	const std::string &dst_filename)
{
	if (!MoveFileEx(src_filename.c_str(), dst_filename.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		throw std::runtime_error("rename");
}

inline void
cruft_rename_file_over_running_exe(
	const std::string &src_filename,
	const std::string &dst_filename)
{
	if (!boost::regex_search(src_filename.c_str(), boost::cmatch(), boost::regex(".exe$")) || !boost::regex_search(dst_filename.c_str(), boost::cmatch(), boost::regex(".exe$")))
		std::runtime_error("reexec name match");
	cruft_rename_file_file(dst_filename, boost::filesystem::path(dst_filename).replace_extension(".old").string());
	cruft_rename_file_file(src_filename, dst_filename);
}

inline int
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

inline void
cruft_exec_file_expecting(const std::string &exec_filename, int ret_expected)
{
	if (cruft_exec_file_lowlevel(exec_filename, {}, std::chrono::milliseconds(0xFFFFFFFF)) != ret_expected)
		throw std::runtime_error("process exec code check");
}

inline void
cruft_exec_file_checking_retcode(
	const std::string &exec_filename,
	const std::string &arg_opt,
	std::chrono::milliseconds wait_ms,
	int ret_expected)
{
	if (cruft_exec_file_lowlevel(exec_filename, { arg_opt }, wait_ms) != ret_expected)
		throw std::runtime_error("process exec code check");
}

inline void
cruft_debug_wait()
{
	MessageBoxA(NULL, "Attach Debugger", cruft_current_executable_filename().c_str(), MB_OK);
	if (IsDebuggerPresent())
		__debugbreak();
}

inline boost::property_tree::ptree
cruft_config_read()
{
	std::stringstream ss(getenv("PS_CONFIG") ? getenv("PS_CONFIG") : std::string((char *)g_ps_config_updater, sizeof g_ps_config_updater));

	boost::property_tree::ptree pt;
	boost::property_tree::json_parser::read_json(ss, pt);

	if (pt.get<std::string>("DEBUG_WAIT", "OFF") != "OFF")
		cruft_debug_wait();

	return pt;
}

inline boost::filesystem::path
cruft_config_get_path(
	const boost::property_tree::ptree &config,
	const char *entryname)
{
	boost::filesystem::path path = config.get<std::string>(entryname);
	if (path.is_absolute())
		return path;
	else
		return boost::filesystem::path(cruft_current_executable_filename()).parent_path() / path;
}

inline void
cruft_file_write_moving(const std::string &finalpathdir_creation_lump_check, const boost::filesystem::path &finalpath, const std::string &content)
{
	/* prepare final */
	const boost::filesystem::path finalpathdir = finalpath.parent_path();
	if (finalpathdir_creation_lump_check.size()) {
		if (finalpathdir.string().find(finalpathdir_creation_lump_check) == std::string::npos)
			throw std::runtime_error("finalpathdir_creation_lump_check");
		try {
			if (!boost::filesystem::exists(finalpathdir))
				boost::filesystem::create_directories(finalpathdir);
		} catch (boost::filesystem::filesystem_error &) {
			/* empty */
		}
	}
	/* write temp */
	boost::filesystem::path temppath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("pstmp_%%%%-%%%%-%%%%-%%%%");
	std::ofstream ff(temppath.string(), std::ios::out | std::ios::trunc | std::ios::binary);
	ff.write(content.data(), content.size());
	ff.flush();
	ff.close();
	if (!ff.good())
		throw std::runtime_error("file write");
	/* write final */
	cruft_rename_file_file(temppath.string(), finalpath.string());
}

inline std::string
cruft_file_read(const boost::filesystem::path &path)
{
	std::ifstream ff(path.string().c_str(), std::ios::in | std::ios::binary);
	std::stringstream ss;
	ss << ff.rdbuf();
	if (!ff.good())
		throw std::runtime_error("file read");
	std::string str(ss.str());
	return str;
}

}

#endif /* _PS_CRUFT_HPP_ */
