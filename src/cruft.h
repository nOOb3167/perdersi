#ifndef _CRUFT_H_
#define _CRUFT_H_

#include <chrono>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>

std::string
cruft_current_executable_filename();
void
cruft_rename_file_file(const std::string &src_filename, const std::string &dst_filename);
void
cruft_rename_file_over_running_exe(
	const std::string &src_filename,
	const std::string &dst_filename);
int
cruft_exec_file_lowlevel(
	const std::string &exec_filename,
	const std::vector<std::string> &args,
	const std::chrono::milliseconds &wait_ms);
void
cruft_exec_file_expecting(const std::string &exec_filename, int ret_expected);
void
cruft_exec_file_checking_retcode(
	const std::string &exec_filename,
	const std::string &arg_opt,
	std::chrono::milliseconds wait_ms,
	int ret_expected);
void
cruft_debug_wait();
boost::property_tree::ptree
cruft_config_read();
boost::filesystem::path
cruft_config_get_path(
	const boost::property_tree::ptree &config,
	const char *entryname);

#endif /* _CRUFT_H_ */
