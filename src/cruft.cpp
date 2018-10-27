#include <sstream>
#include <stdexcept>

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
