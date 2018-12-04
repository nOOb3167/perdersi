#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <git2.h>
#include <miniz.h>

#include <cruft.h>
#include <psmisc.hpp>
#include <pscon.hpp>
#include <psgit.hpp>

using pt_t = ::boost::property_tree::ptree;

void file_write_moving(const std::string &finalpathdir_creation_lump_check, const boost::filesystem::path &finalpath, const std::string &content)
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

std::string file_read(const boost::filesystem::path &path)
{
	std::ifstream ff(path.string().c_str(), std::ios::in | std::ios::binary);
	std::stringstream ss;
	ss << ff.rdbuf();
	if (!ff.good())
		throw std::runtime_error("file read");
	std::string str(ss.str());
	return str;
}

std::string get_object(PsCon *client, const shahex_t &obj)
{
	std::string loose = client->reqPost_("/objects/" + obj.substr(0, 2) + "/" + obj.substr(2), "").body();
	return loose;
}

shahex_t get_head(PsCon *client, const std::string &refname)
{
	shahex_t objhex = client->reqPost_("/refs/heads/" + refname, "").body();

	if (objhex.size() != GIT_OID_HEXSZ)
		throw PsConExc();

	return objhex;
}

void get_write_object_raw(git_repository *repo, const shahex_t &obj, const std::string &incoming_loose)
{
	if (!ns_git::hexstr_equals(obj, ns_git::get_object_data_hexstr(ns_git::inflatebuf(incoming_loose))))
		throw PsConExc();
	assert(git_repository_path(repo));
	const boost::filesystem::path objectpath = boost::filesystem::path(git_repository_path(repo)) / "objects" / obj.substr(0, 2) / obj.substr(2);
	file_write_moving(".git", objectpath, incoming_loose);
}

void get_write_object_raw_ifnotexist(PsCon *client, git_repository *repo, const shahex_t &obj)
{
	unique_ptr_gitodb odb(ns_git::odb_from_repo(repo));
	git_oid _obj = ns_git::oid_from_hexstr(obj);
	if (git_odb_exists(odb.get(), &_obj))
		return;
	get_write_object_raw(repo, obj, get_object(client, obj));
}

std::vector<shahex_t> get_trees_writing(PsCon *client, git_repository *repo, const shahex_t &tree)
{
	std::vector<shahex_t> out;

	get_write_object_raw_ifnotexist(client, repo, tree);

	out.push_back(tree);

	unique_ptr_gittree t(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(tree)));
	for (size_t i = 0; i < git_tree_entrycount(t.get()); ++i)
		if (git_tree_entry_filemode(git_tree_entry_byindex(t.get(), i)) == GIT_FILEMODE_TREE)
			for (const auto &elt : get_trees_writing(client, repo, ns_git::hexstr_from_oid(*git_tree_entry_id(git_tree_entry_byindex(t.get(), i)))))
				out.push_back(elt);
	return out;
}

std::vector<shahex_t> get_blobs_writing(PsCon *client, git_repository *repo, const std::vector<shahex_t> &trees)
{
	std::vector<shahex_t> blobs;

	for (const auto &elt : trees) {
		unique_ptr_gittree t(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(elt)));
		for (size_t i = 0; i < git_tree_entrycount(t.get()); ++i)
			if (git_tree_entry_filemode(git_tree_entry_byindex(t.get(), i)) == GIT_FILEMODE_BLOB ||
				git_tree_entry_filemode(git_tree_entry_byindex(t.get(), i)) == GIT_FILEMODE_BLOB_EXECUTABLE)
			{
				blobs.push_back(ns_git::hexstr_from_oid(*git_tree_entry_id(git_tree_entry_byindex(t.get(), i))));
			}
	}

	for (const auto &blob : blobs)
		get_write_object_raw_ifnotexist(client, repo, blob);

	return blobs;
}

unique_ptr_gitblob blob_tree_entry(git_repository *repo, const shahex_t &tree, const std::string &entry)
{
	unique_ptr_gittree t(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(tree)));
	const git_tree_entry *e = git_tree_entry_byname(t.get(), entry.c_str());
	if (!e || git_tree_entry_filemode(e) != GIT_FILEMODE_BLOB && git_tree_entry_filemode(e) != GIT_FILEMODE_BLOB_EXECUTABLE)
		throw PsConExc();
	return ns_git::blob_lookup(repo, *git_tree_entry_id(e));
}

std::string blob_tree_entry_content(git_repository *repo, const shahex_t &tree, const std::string &entry)
{
	unique_ptr_gitblob b(blob_tree_entry(repo, tree, entry));
	std::string content((const char *)git_blob_rawcontent(b.get()), (size_t)git_blob_rawsize(b.get()));
	return content;
}

shahex_t create_commit(git_repository *repo, const shahex_t &tree)
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

void create_ref(git_repository *repo, const std::string &refname, const git_oid commit)
{
	git_reference *ref = NULL;
	if (!! git_reference_create(&ref, repo, refname.c_str(), &commit, true, "DummyLogMessage"))
		throw PsConExc();
	git_reference_free(ref);
}

boost::filesystem::path
prepare_content_file_for_replacing_running_exe(
	const std::string &content,
	const std::string &curexefname)
{
	// create file with content
	boost::filesystem::path tryout_exe_path = boost::filesystem::path(curexefname).replace_extension(".tryout.exe");
	file_write_moving("", tryout_exe_path, content);
	// see if it runs
	cruft_exec_file_checking_retcode(tryout_exe_path.string(), "--tryout", std::chrono::milliseconds(5000), 123);
	return tryout_exe_path;
}

bool
ensuring_content_replace_running_exe(
	const std::string &content,
	const std::string &curexefname
)
{
	// content already as wanted
	if (file_read(curexefname) == content)
		return false;
	// attempt ensuring content
	boost::filesystem::path tryout_exe_path = prepare_content_file_for_replacing_running_exe(content, curexefname);
	cruft_rename_file_over_running_exe(tryout_exe_path.string(), curexefname);
	// was attempt successful?
	if (file_read(curexefname) != content)
		throw std::runtime_error("failed updating");
	return true;
}

int main(int argc, char **argv)
{
	if (std::find(argv + 1, argv + argc, "--tryout") != argv + argc)
		return 123;
	bool arg_skipselfupdate = std::find(argv + 1, argv + argc, "--skipselfupdate") != argv + argc;

	git_libgit2_init();

	pt_t config = cruft_config_read();
	boost::filesystem::path chkoutdir = cruft_config_get_path(config, "REPO_CHK_DIR");
	boost::filesystem::path stage2path = chkoutdir / config.get<std::string>("UPDATER_STAGE2_EXE_RELATIVE");
	std::string updatr = config.get<std::string>("UPDATER_EXE_RELATIVE");

	unique_ptr_gitrepository repo(ns_git::repository_ensure(config.get<std::string>("REPO_DIR")));

	PsConTest client(config.get<std::string>("ORIGIN_DOMAIN_API"), config.get<std::string>("LISTEN_PORT"));
	shahex_t head = get_head(&client, "master");

	std::cout << "repodir: " << git_repository_path(repo.get()) << std::endl;
	std::cout << "chkodir: " << chkoutdir.string() << std::endl;
	std::cout << "stage2p: " << stage2path.string() << std::endl;
	std::cout << "updatr: " << updatr << std::endl;
	std::cout << "head: " << head << std::endl;

	std::vector<shahex_t> trees = get_trees_writing(&client, repo.get(), head);
	std::vector<shahex_t> blobs = get_blobs_writing(&client, repo.get(), trees);
	ns_git::checkout_obj(repo.get(), head,	chkoutdir.string());

	do {
		if (arg_skipselfupdate)
			break;
		if (!ensuring_content_replace_running_exe(blob_tree_entry_content(repo.get(), head, updatr), cruft_current_executable_filename()))
			break;

		cruft_exec_file_lowlevel(cruft_current_executable_filename(), { "--skipselfupdate" }, std::chrono::milliseconds(0));

		return EXIT_SUCCESS;
	} while (false);

	cruft_exec_file_lowlevel(stage2path.string(), {}, std::chrono::milliseconds(0));

	return EXIT_SUCCESS;
}
