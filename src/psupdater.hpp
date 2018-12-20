#ifndef _PSUPDATER_HPP_
#define _PSUPDATER_HPP_

#include <algorithm>
#include <string>
#include <vector>
#include <tuple>
#include <utility>

#include <boost/filesystem.hpp>
#include <git2.h>

#include <pscon.hpp>
#include <pscruft.hpp>
#include <psgit.hpp>
#include <psmisc.hpp>

namespace ps
{

inline std::string
updater_object_get(Con *client, const shahex_t &obj)
{
	return client->reqPost("/objects/" + obj.substr(0, 2) + "/" + obj.substr(2), "").body();
}

inline shahex_t
updater_head_get(Con *client, const std::string &refname)
{
	return git_refcontent2hex(client->reqPost("/refs/heads/" + refname, "").body());
}

inline shahex_t
updater_commit_tree_get(Con *client, const shahex_t &commit)
{
	const std::string &incoming_comt = git_inflatebuf(updater_object_get(client, commit));
	git_hexeq(commit, git_incoming_data_hex(incoming_comt));
	const GitObjectDataInfo odi(incoming_comt, git_tag_incoming_data_t());
	if (odi.m_type != "commit")
		throw ConExc();
	return git_comtcontent_tree2hex(std::string(incoming_comt.data() + odi.m_data_offset, incoming_comt.size() - odi.m_data_offset));
}

inline void
updater_object_write_raw_ifnotexist(Con *client, git_repository *repo, const shahex_t &obj, const std::string &incoming_loose)
{
	git_hexeq(obj, git_incoming_data_hex(git_inflatebuf(incoming_loose)));
	if (! git_odb_exists(odb_from_repo(repo).get(), git_hex2bin(obj)))
		cruft_file_write_moving(".git", boost::filesystem::path(git_repository_path(repo)) / "objects" / obj.substr(0, 2) / obj.substr(2), incoming_loose);;
}

inline std::vector<shahex_t>
updater_trees_get_writing_recursive(Con *client, git_repository *repo, const shahex_t &tree)
{
	std::vector<shahex_t> out;

	updater_object_write_raw_ifnotexist(client, repo, tree, updater_object_get(client, tree));

	out.push_back(tree);

	unique_ptr_gittree t(tree_lookup(repo, git_hex2bin(tree)));
	for (size_t i = 0; i < git_tree_entrycount(t.get()); ++i)
		if (git_tree_entry_filemode(git_tree_entry_byindex(t.get(), i)) == GIT_FILEMODE_TREE)
			for (const auto &elt : updater_trees_get_writing_recursive(client, repo, git_bin2hex(*git_tree_entry_id(git_tree_entry_byindex(t.get(), i)))))
				out.push_back(elt);
	return out;
}

inline void
updater_blobs_get_writing(Con *client, git_repository *repo, const std::vector<shahex_t> &blobs)
{
	for (const auto &blob : blobs)
		updater_object_write_raw_ifnotexist(client, repo, blob, updater_object_get(client, blob));
}

inline std::vector<shahex_t>
updater_blobs_list(git_repository *repo, const std::vector<shahex_t> &trees)
{
	std::vector<shahex_t> blobs;

	for (const auto &t : tree_lookup_v(repo, trees))
		for (size_t i = 0; i < git_tree_entrycount(t.get()); ++i)
			if (git_tree_entry_filemode_bloblike_is(t.get(), i))
				blobs.push_back(git_bin2hex(*git_tree_entry_id(git_tree_entry_byindex(t.get(), i))));

	return blobs;
}

inline unique_ptr_gitblob
updater_tree_entry_blob(git_repository *repo, const shahex_t &tree, const std::string &entry)
{
	const git_tree_entry *e = git_tree_entry_byname(tree_lookup(repo, git_hex2bin(tree)).get(), entry.c_str());
	if (!e || git_tree_entry_filemode(e) != GIT_FILEMODE_BLOB && git_tree_entry_filemode(e) != GIT_FILEMODE_BLOB_EXECUTABLE)
		throw ConExc();
	return blob_lookup(repo, *git_tree_entry_id(e));
}

inline std::string
updater_tree_entry_blob_content(git_repository *repo, const shahex_t &tree, const std::string &entry)
{
	unique_ptr_gitblob b(updater_tree_entry_blob(repo, tree, entry));
	return std::string((const char *)git_blob_rawcontent(b.get()), (size_t)git_blob_rawsize(b.get()));;
}

inline shahex_t
updater_commit_create(git_repository *repo, const shahex_t &tree)
{
	unique_ptr_gitbuf buf(buf_new());
	git_oid commit_oid_even_if_error = {};

	if (!!git_commit_create_buffer(buf.get(), repo, sig_new_dummy().get(), sig_new_dummy().get(), "UTF-8", "Dummy", tree_lookup(repo, git_hex2bin(tree)).get(), 0, NULL))
		throw ConExc();
 
	if (!!git_odb_write(&commit_oid_even_if_error, odb_from_repo(repo).get(), buf->ptr, buf->size, GIT_OBJ_COMMIT))
		if (!git_odb_exists(odb_from_repo(repo).get(), &commit_oid_even_if_error))
			throw ConExc();

	return git_bin2hex(commit_oid_even_if_error);
}

inline boost::filesystem::path
updater_running_exe_content_file_replace_prepare(
	const std::string &content,
	const std::string &curexefname)
{
	// create file with content
	boost::filesystem::path tryout_exe_path = boost::filesystem::path(curexefname).replace_extension(".tryout.exe");
	cruft_file_write_moving("", tryout_exe_path, content);
	// see if it runs
	cruft_exec_file_checking_retcode(tryout_exe_path.string(), "--tryout", std::chrono::milliseconds(5000), 123);
	return tryout_exe_path;
}

inline bool
updater_running_exe_content_file_replace_ensure(
	const std::string &content,
	const std::string &curexefname
)
{
	// content already as wanted
	if (cruft_file_read(curexefname) == content)
		return false;
	// attempt ensuring content
	cruft_rename_file_over_running_exe(updater_running_exe_content_file_replace_prepare(content, curexefname).string(), curexefname);
	// was attempt successful?
	if (cruft_file_read(curexefname) != content)
		throw std::runtime_error("failed updating");
	return true;
}

inline std::vector<std::string>
updater_argv_vectorize(int argc, char **argv)
{
	if (argc < 1)
		throw std::runtime_error("argc");
	std::vector<std::string> args;
	for (size_t i = 1; i < argc; i++)
		args.push_back(argv[i]);
	return args;
}

inline std::string
updater_argv_find_opt_arg(const std::vector<std::string> &args, std::string opt)
{
	auto it = std::find(args.begin(), args.end(), opt);
	if (std::distance(it, args.end()) < 2)
		throw std::runtime_error("not opt arg");
	return std::string(*++it);
}

inline std::tuple<bool, bool, std::string>
updater_argv_parse(int argc, char **argv)
{
	std::vector<std::string> args(updater_argv_vectorize(argc, argv));
	return {
		std::find(args.begin(), args.end(), "--tryout") != args.end(),
		std::find(args.begin(), args.end(), "--skipselfupdate") != args.end(),
		updater_argv_find_opt_arg(args, "--fsmode")
	};
}

inline void
updater_replace_cond(
	bool arg_skipselfupdate,
	git_repository *repo,
	const shahex_t &head,
	const std::string &updatr,
	const std::string &curexefname,
	const boost::filesystem::path &stage2path)
{
	if (!arg_skipselfupdate && updater_running_exe_content_file_replace_ensure(updater_tree_entry_blob_content(repo, head, updatr), curexefname))
		cruft_exec_file_lowlevel(cruft_current_executable_filename(), { "--skipselfupdate" }, std::chrono::milliseconds(0));
	else
		cruft_exec_file_lowlevel(stage2path.string(), {}, std::chrono::milliseconds(0));
}

}

#endif /* _PSUPDATER_HPP_ */
