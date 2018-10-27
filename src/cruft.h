#ifndef _CRUFT_H_
#define _CRUFT_H_

#include <string>

#include <boost/property_tree/json_parser.hpp>

std::string
cruft_current_executable_filename();
void
cruft_rename_file_file(std::string src_filename, std::string dst_filename);
void
cruft_rename_file_selfexec(std::string src_filename, std::string dst_filename);
void
cruft_exec_file_expecting(std::string exec_filename, int ret_expected);
void
cruft_debug_wait();
boost::property_tree::ptree
cruft_config_read();

#endif /* _CRUFT_H_ */
