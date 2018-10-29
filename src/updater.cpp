#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/regex.hpp>
#include <git2.h>
#include <miniz.h>

#include <cruft.h>

template<typename T>
using sp = ::std::shared_ptr<T>;

using pt_t = ::boost::property_tree::ptree;
using shahex_t = ::std::string;

typedef ::std::unique_ptr<git_blob, void(*)(git_blob *)> unique_ptr_gitblob;
typedef ::std::unique_ptr<git_commit, void(*)(git_commit *)> unique_ptr_gitcommit;
typedef ::std::unique_ptr<git_odb, void(*)(git_odb *)> unique_ptr_gitodb;
typedef ::std::unique_ptr<git_repository, void(*)(git_repository *)> unique_ptr_gitrepository;
typedef ::std::unique_ptr<git_signature, void(*)(git_signature *)> unique_ptr_gitsignature;
typedef ::std::unique_ptr<git_tree, void(*)(git_tree *)> unique_ptr_gittree;

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

namespace ns_git
{

class ObjectDataInfo
{
public:
	ObjectDataInfo(const std::string &data, const std::string &type, size_t data_offset) :
		m_type(type),
		m_data_offset(data_offset)
	{
		if (m_data_offset >= data.size())
			throw std::runtime_error("odi data_offset");
	}

	std::string m_type;
	size_t m_data_offset;
};

std::string inflatebuf(const std::string &buf)
{
	/* https://www.zlib.net/zpipe.c
	     official example
	*/

    std::string result;

	int ret = Z_OK;

	const size_t CHUNK = 16384;
	char out[CHUNK] = {};

	z_stream strm = {};

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    if (inflateInit(&strm) != Z_OK)
        throw std::runtime_error("inflate init");

	strm.avail_in = (unsigned int) buf.size();
	strm.next_in = (Bytef *) buf.data();

	result.reserve(2 * CHUNK);  // arbitrary preallocation

	do {
		strm.avail_out = CHUNK;
		strm.next_out = (Bytef *) out;

		ret = inflate(&strm, Z_NO_FLUSH);
		if (ret != Z_OK && ret != Z_STREAM_END) {
			if (inflateEnd(&strm) != Z_OK)
				throw std::runtime_error("inflate inflateend");
			throw std::runtime_error("inflate inflate");
		}

		size_t have = CHUNK - strm.avail_out;
		result.append(out, have);
	} while (ret != Z_STREAM_END);

	if (inflateEnd(&strm) != Z_OK)
		throw std::runtime_error("inflate inflateend");

	return result;
}

ObjectDataInfo get_object_data_info(const std::string &data)
{
	/* format: "(type)(space)(number)(NULL)" */
	boost::cmatch what;
	if (! boost::regex_search(data.c_str(), what, boost::regex("([[:alpha:]]+) ([[:digit:]]+)\000"), boost::match_continuous))
		throw std::runtime_error("hdr regex");
	return ObjectDataInfo(data, what[1], (what[0].second + 1) - what[0].first);
}

git_oid oid_from_hexstr(const shahex_t &str)
{
	git_oid oid = {};
	if (str.size() != GIT_OID_HEXSZ || !!git_oid_fromstr(&oid, str.c_str()))
		throw std::runtime_error("oid");
	return oid;
}

std::string hexstr_from_oid(const git_oid &oid)
{
	char buf[GIT_OID_HEXSZ + 1] = {};
	return std::string(git_oid_tostr(buf, sizeof buf, &oid));
}

bool hexstr_equals(const shahex_t &a, const shahex_t &b)
{
	git_oid a_ = oid_from_hexstr(a), b_ = oid_from_hexstr(b);
	return !git_oid_cmp(&a_, &b_);
}

git_otype otype_from_type(const std::string &type)
{
	static std::map<std::string, git_otype> tbl = {{"commit", GIT_OBJ_COMMIT}, {"tree", GIT_OBJ_TREE}, {"blob", GIT_OBJ_BLOB}};
	if (tbl.find(type) == tbl.end())
		throw std::runtime_error("type otype");
	return tbl[type];
}

shahex_t get_object_data_hexstr(const std::string &incoming_data)
{
	const ns_git::ObjectDataInfo info = ns_git::get_object_data_info(incoming_data);
	git_oid oid_loose = {};
	if (!!git_odb_hash(&oid_loose, incoming_data.data() + info.m_data_offset, incoming_data.size() - info.m_data_offset, ns_git::otype_from_type(info.m_type)))
		throw std::runtime_error("object data oid hash");
	return hexstr_from_oid(oid_loose);
}

void blob_delete(git_blob *p) { if (p) git_blob_free(p); }
void commit_delete(git_commit *p) { if (p) git_commit_free(p); }
void odb_delete(git_odb *p) { if (p) git_odb_free(p); }
void repo_delete(git_repository *p) { if (p) git_repository_free(p); }
void sig_delete(git_signature *p) { if (p) git_signature_free(p); }
void tree_delete(git_tree *p) { if (p) git_tree_free(p); }

unique_ptr_gitblob blob_lookup(git_repository *repo, const git_oid oid)
{
	git_blob *p = nullptr;
	if (!!git_blob_lookup(&p, repo, &oid))
		throw std::runtime_error("blob lookup");
	return unique_ptr_gitblob(p, blob_delete);
}

unique_ptr_gitcommit commit_lookup(git_repository *repo, const git_oid oid)
{
	git_commit *p = nullptr;
	if (!!git_commit_lookup(&p, repo, &oid))
		throw std::runtime_error("commit lookup");
	return unique_ptr_gitcommit(p, commit_delete);
}

unique_ptr_gitodb odb_from_repo(git_repository *repo)
{
	git_odb *odb = nullptr;
	if (!!git_repository_odb(&odb, repo))
		throw std::runtime_error("odb repository");
	return unique_ptr_gitodb(odb, odb_delete);
}

unique_ptr_gitrepository repository_open(std::string path)
{
	git_repository *p = NULL;
	if (!! git_repository_open(&p, path.c_str()))
		throw std::runtime_error("repository");
	return unique_ptr_gitrepository(p, repo_delete);
}

unique_ptr_gitsignature sig_new_dummy()
{
	git_signature *sig = nullptr;
	if (!! git_signature_new(&sig, "DummyName", "DummyEMail", 0, 0))
		throw std::runtime_error("signature");
	return unique_ptr_gitsignature(sig, sig_delete);
}

unique_ptr_gittree tree_lookup(git_repository *repo, const git_oid oid)
{
	git_tree *p = nullptr;
	if (!!git_tree_lookup(&p, repo, &oid))
		throw std::runtime_error("tree lookup");
	return unique_ptr_gittree(p, tree_delete);
}

unique_ptr_gitrepository repository_ensure(const std::string &repopath)
{
	int err = 0;
	git_repository *repo = NULL;
	git_repository_init_options init_options = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	init_options.flags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR;
	assert(init_options.version == 1);

	if (!!(err = git_repository_init_ext(&repo, repopath.c_str(), &init_options))) {
		if (err == GIT_EEXISTS)
			return unique_ptr_gitrepository(ns_git::repository_open(repopath.c_str()));
		throw std::runtime_error("ensure repository init");
	}
	return unique_ptr_gitrepository(repo, ns_git::repo_delete);
}

void checkout_obj(git_repository *repo, const shahex_t &tree, const std::string &chkoutdir)
{
	if (!boost::regex_search(chkoutdir.c_str(), boost::cmatch(), boost::regex("chk")))
		throw std::runtime_error("checkout sanity");
	// FIXME: consider GIT_CHECKOUT_REMOVE_UNTRACKED
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	opts.disable_filters = 1;
	opts.target_directory = chkoutdir.c_str();
	unique_ptr_gittree _tree(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(tree)));
	if (!!git_checkout_tree(repo, (git_object *) _tree.get(), &opts))
		throw std::runtime_error("checkout tree");
}

}

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

using res_t = http::response<http::string_body>;

class PsConExc : public std::runtime_error
{
public:
	PsConExc() :
		std::runtime_error("error")
	{}
};

class PsCon
{
public:
	PsCon(const std::string &host, const std::string &port) :
		m_host(host),
		m_port(port),
		m_host_http(host + ":" + port),
		m_ioc(),
		m_resolver(m_ioc),
		m_resolver_r(m_resolver.resolve(host, port)),
		m_socket(new tcp::socket(m_ioc))
	{
		boost::asio::connect(*m_socket, m_resolver_r.begin(), m_resolver_r.end());
	};

	~PsCon()
	{
		m_socket->shutdown(tcp::socket::shutdown_both);
	}

	void _reconnect()
	{
		m_socket.reset(new tcp::socket(m_ioc));
		boost::asio::connect(*m_socket, m_resolver_r.begin(), m_resolver_r.end());
	}

	res_t reqPost(const std::string &path, const std::string &data)
	{
		http::request<http::string_body> req(http::verb::post, path, 11);
		req.set(http::field::host, m_host_http);
		req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
		http::write(*m_socket, req);
		boost::beast::flat_buffer buffer;
		http::response<http::string_body> res;
		http::read(*m_socket, buffer, res);
		// https://github.com/boostorg/beast/issues/927
		//   Repeated calls to an URL (repeated http::write calls without remaking the socket)
		//     - needs http::response::keep_alive() true
		//     - (for which http::response::version() must be 11 (HTTP 1.1))
		// https://www.reddit.com/r/flask/comments/634i5u/make_flask_return_header_response_with_http11/
		//   You can't. Flask's dev server does not implement the HTTP 1.1 spec
		//     - flask does not support HTTP 1.1, remake socket if necessary
		if (!res.keep_alive())
			_reconnect();
		return res;
	}

	virtual res_t reqPost_(const std::string &path, const std::string &data)
	{
		res_t res = reqPost(path, data);
		if (res.result_int() != 200)
			throw PsConExc();
		return res;
	}

	std::string m_host;
	std::string m_port;
	std::string m_host_http;
	boost::asio::io_context m_ioc;
	tcp::resolver m_resolver;
	tcp::resolver::results_type m_resolver_r;
	sp<tcp::socket> m_socket;
};

class PsConTest : public PsCon
{
public:
	PsConTest(const std::string &host, const std::string &port) :
		PsCon(host, port)
	{};

	res_t reqPost_(const std::string &path, const std::string &data) override
	{
		boost::cmatch what;
		if (boost::regex_search(path.c_str(), what, boost::regex("/objects/([[:xdigit:]]{2})/([[:xdigit:]]{38})"), boost::match_continuous))
			m_objects_requested.push_back(boost::algorithm::to_lower_copy(what[1].str() + what[2].str()));
		return PsCon::reqPost_(path, data);
	}

	std::vector<shahex_t> m_objects_requested;
};

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

int main(int argc, char **argv)
{
	bool arg_skipselfupdate = false;
	for (size_t i = 1; i < argc; ++i)
		if (std::string(argv[i]) == "--tryout")
			return 123;
	for (size_t i = 1; i < argc; ++i)
		if (std::string(argv[i]) == "--skipselfupdate")
			arg_skipselfupdate = true;

	git_libgit2_init();

	pt_t config = cruft_config_read();
	boost::filesystem::path chkoutdir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("chk_%%%%-%%%%-%%%%-%%%%");

	unique_ptr_gitrepository repo(ns_git::repository_ensure(config.get<std::string>("REPO_DIR")));

	PsConTest client(config.get<std::string>("ORIGIN_DOMAIN_API"), config.get<std::string>("LISTEN_PORT"));
	shahex_t head = get_head(&client, "master");

	std::cout << "repodir: " << git_repository_path(repo.get()) << std::endl;
	std::cout << "chkodir: " << chkoutdir.string() << std::endl;
	std::cout << "head: " << head << std::endl;

	std::vector<shahex_t> trees = get_trees_writing(&client, repo.get(), head);
	std::vector<shahex_t> blobs = get_blobs_writing(&client, repo.get(), trees);
	ns_git::checkout_obj(repo.get(), head, chkoutdir.string());

	do {
		if (arg_skipselfupdate)
			break;
		std::string updater_content(blob_tree_entry_content(repo.get(), head, "updater.exe"));
		if (file_read(cruft_current_executable_filename()) == updater_content)
			break;
		boost::filesystem::path tryout_exe_path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("pstmp_%%%%-%%%%-%%%%-%%%%.exe");
		file_write_moving("", tryout_exe_path, updater_content);
		cruft_exec_file_expecting_ex(tryout_exe_path.string(), "--tryout", std::chrono::milliseconds(5000), 123);
		cruft_rename_file_selfexec(tryout_exe_path.string(), cruft_current_executable_filename());
		if (file_read(cruft_current_executable_filename()) != updater_content)
			throw std::runtime_error("failed updating");
		cruft_exec_file_lowlevel(cruft_current_executable_filename(), { "--skipselfupdate" }, std::chrono::milliseconds(0));

		return EXIT_SUCCESS;
	} while (false);

	boost::filesystem::path stage2 = boost::filesystem::path(cruft_current_executable_filename()).parent_path() / config.get<std::string>("UPDATER_STAGE2_EXE_RELATIVE");
	cruft_exec_file_lowlevel(stage2.string(), {}, std::chrono::milliseconds(0));

	return EXIT_SUCCESS;
}
