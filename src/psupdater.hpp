#ifndef _PSUPDATER_HPP_
#define _PSUPDATER_HPP_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <git2.h>

#include <pscon.hpp>
#include <pscruft.hpp>
#include <psgit.hpp>
#include <psmisc.hpp>

namespace ps
{

std::string
updater_object_get(PsCon *client, const shahex_t &obj)
{
	std::string loose = client->reqPost_("/objects/" + obj.substr(0, 2) + "/" + obj.substr(2), "").body();
	return loose;
}

shahex_t
updater_head_get(PsCon *client, const std::string &refname)
{
	shahex_t objhex = client->reqPost_("/refs/heads/" + refname, "").body();

	if (objhex.size() != GIT_OID_HEXSZ)
		throw PsConExc();

	return objhex;
}

void
updater_object_get_write_raw(git_repository *repo, const shahex_t &obj, const std::string &incoming_loose)
{
	if (!ns_git::hexstr_equals(obj, ns_git::get_object_data_hexstr(ns_git::inflatebuf(incoming_loose))))
		throw PsConExc();
	assert(git_repository_path(repo));
	const boost::filesystem::path objectpath = boost::filesystem::path(git_repository_path(repo)) / "objects" / obj.substr(0, 2) / obj.substr(2);
	cruft_file_write_moving(".git", objectpath, incoming_loose);
}

void
updater_object_get_write_raw_ifnotexist(PsCon *client, git_repository *repo, const shahex_t &obj)
{
	unique_ptr_gitodb odb(ns_git::odb_from_repo(repo));
	git_oid _obj = ns_git::oid_from_hexstr(obj);
	if (git_odb_exists(odb.get(), &_obj))
		return;
	updater_object_get_write_raw(repo, obj, updater_object_get(client, obj));
}

std::vector<shahex_t>
updater_trees_get_writing(PsCon *client, git_repository *repo, const shahex_t &tree)
{
	std::vector<shahex_t> out;

	updater_object_get_write_raw_ifnotexist(client, repo, tree);

	out.push_back(tree);

	unique_ptr_gittree t(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(tree)));
	for (size_t i = 0; i < git_tree_entrycount(t.get()); ++i)
		if (git_tree_entry_filemode(git_tree_entry_byindex(t.get(), i)) == GIT_FILEMODE_TREE)
			for (const auto &elt : updater_trees_get_writing(client, repo, ns_git::hexstr_from_oid(*git_tree_entry_id(git_tree_entry_byindex(t.get(), i)))))
				out.push_back(elt);
	return out;
}

std::vector<shahex_t>
updater_blobs_get_writing(PsCon *client, git_repository *repo, const std::vector<shahex_t> &trees)
{
	std::vector<shahex_t> blobs;

	std::vector<unique_ptr_gittree> trees_;
	for (const auto &t : trees)
		trees_.push_back(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(t)));

	for (const auto &t : trees_)
		for (size_t i = 0; i < git_tree_entrycount(t.get()); ++i)
			if (ns_git::tree_entry_filemode_bloblike_is(t.get(), i))
				blobs.push_back(ns_git::hexstr_from_oid(*git_tree_entry_id(git_tree_entry_byindex(t.get(), i))));

	for (const auto &blob : blobs)
		updater_object_get_write_raw_ifnotexist(client, repo, blob);

	return blobs;
}

unique_ptr_gitblob
updater_tree_entry_blob(git_repository *repo, const shahex_t &tree, const std::string &entry)
{
	unique_ptr_gittree t(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(tree)));
	const git_tree_entry *e = git_tree_entry_byname(t.get(), entry.c_str());
	if (!e || git_tree_entry_filemode(e) != GIT_FILEMODE_BLOB && git_tree_entry_filemode(e) != GIT_FILEMODE_BLOB_EXECUTABLE)
		throw PsConExc();
	return ns_git::blob_lookup(repo, *git_tree_entry_id(e));
}

std::string
updater_tree_entry_blob_content(git_repository *repo, const shahex_t &tree, const std::string &entry)
{
	unique_ptr_gitblob b(updater_tree_entry_blob(repo, tree, entry));
	std::string content((const char *)git_blob_rawcontent(b.get()), (size_t)git_blob_rawsize(b.get()));
	return content;
}

shahex_t
updater_commit_create(git_repository *repo, const shahex_t &tree)
{
	unique_ptr_gitodb odb(ns_git::odb_from_repo(repo));
	unique_ptr_gitsignature sig(ns_git::sig_new_dummy());
	unique_ptr_gittree t(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(tree)));

	git_buf buf = {};
	git_oid commit_oid_even_if_error = {};

	if (!!git_commit_create_buffer(&buf, repo, sig.get(), sig.get(), "UTF-8", "Dummy", t.get(), 0, NULL))
		throw PsConExc();
 
	if (!!git_odb_write(&commit_oid_even_if_error, odb.get(), buf.ptr, buf.size, GIT_OBJ_COMMIT))
		if (!git_odb_exists(odb.get(), &commit_oid_even_if_error))
			throw PsConExc();

	return ns_git::hexstr_from_oid(commit_oid_even_if_error);
}

void
updater_ref_create(git_repository *repo, const std::string &refname, const git_oid commit)
{
	git_reference *ref = NULL;
	if (!! git_reference_create(&ref, repo, refname.c_str(), &commit, true, "DummyLogMessage"))
		throw PsConExc();
	git_reference_free(ref);
}

boost::filesystem::path
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

bool
updater_running_exe_content_file_replace_ensure(
	const std::string &content,
	const std::string &curexefname
)
{
	// content already as wanted
	if (cruft_file_read(curexefname) == content)
		return false;
	// attempt ensuring content
	boost::filesystem::path tryout_exe_path = updater_running_exe_content_file_replace_prepare(content, curexefname);
	cruft_rename_file_over_running_exe(tryout_exe_path.string(), curexefname);
	// was attempt successful?
	if (cruft_file_read(curexefname) != content)
		throw std::runtime_error("failed updating");
	return true;
}

}

#endif /* _PSUPDATER_HPP_ */
