#include <cassert>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>
#include <git2.h>
#include <miniz.h>

template<typename T>
using sp = ::std::shared_ptr<T>;

using shahex_t = ::std::string;

typedef ::std::unique_ptr<git_commit, void(*)(git_commit *)> unique_ptr_gitcommit;
typedef ::std::unique_ptr<git_odb, void(*)(git_odb *)> unique_ptr_gitodb;
typedef ::std::unique_ptr<git_repository, void(*)(git_repository *)> unique_ptr_gitrepository;
typedef ::std::unique_ptr<git_signature, void(*)(git_signature *)> unique_ptr_gitsignature;
typedef ::std::unique_ptr<git_tree, void(*)(git_tree *)> unique_ptr_gittree;

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
	/* format: "(type)(space)(number)(NULL)"
	   format: "[:alpha:]+ [:digit:]+\0" */
	/* skip past type */
	size_t spc;
	if ((spc = data.find_first_of(' ', 0)) == std::string::npos)
		throw std::runtime_error("hdr spc");
	/* skip past space and size */
	if (data.find_first_not_of("0123456789", spc + 1) == std::string::npos)
		throw std::runtime_error("hdr num");
	if (data.at(data.find_first_not_of("0123456789", spc + 1)) != '\0')
		throw std::runtime_error("hdr null");

	return ObjectDataInfo(
		data,
		data.substr(0, data.find_first_of(' ', 0)),       /* type */
		data.find_first_not_of("0123456789", spc + 1) + 1 /* data_offset */
	);
}

git_oid oid_from_hexstr(const std::string &str)
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

void commit_delete(git_commit *p) { if (p) git_commit_free(p); }
void odb_delete(git_odb *p) { if (p) git_odb_free(p); }
void repo_delete(git_repository *p) { if (p) git_repository_free(p); }
void sig_delete(git_signature *p) { if (p) git_signature_free(p); }
void tree_delete(git_tree *p) { if (p) git_tree_free(p); }

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

}

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

using res_t = http::response<http::string_body>;

class PsConExc : public std::exception
{
public:
	PsConExc() :
		std::exception()
	{}
};

void enz(bool w)
{
	if (! w)
		throw PsConExc();
}

class PsCon
{
public:
	PsCon(const std::string &host, const std::string &port) :
		m_host(host),
		m_port(port),
		m_host_http(host + ":" + port),
		m_ioc(),
		m_resolver(m_ioc),
		m_socket(m_ioc)
	{
		auto results = m_resolver.resolve(host, port);
		boost::asio::connect(m_socket, results.begin(), results.end());
	};

	~PsCon()
	{
		m_socket.shutdown(tcp::socket::shutdown_both);
	}

	res_t reqPost(const std::string &path, const std::string &data)
	{
		http::request<http::string_body> req(http::verb::post, path, 11);
		req.set(http::field::host, m_host_http);
		req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
		http::write(m_socket, req);
		boost::beast::flat_buffer buffer;
		http::response<http::string_body> res;
		http::read(m_socket, buffer, res);
		return res;
	}

	res_t reqPost_(const std::string &path, const std::string &data)
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
	tcp::socket m_socket;
};

std::string get_object(PsCon *client, const std::string &objhex)
{
	std::string loose = client->reqPost_("/object/" + objhex.substr(0, 2) + "/" + objhex.substr(2), "").body();
	return loose;
}

shahex_t get_head(PsCon *client, const std::string &refname)
{
	shahex_t objhex = client->reqPost_("/refs/heads/" + refname, "").body();

	if (objhex.size() != GIT_OID_HEXSZ)
		throw PsConExc();

	return objhex;
}

void ensure_object_match(const std::string &objloose, const shahex_t &obj_expected, const std::string &type_expected, git_otype otype_expected)
{
	const git_oid oid_expected = ns_git::oid_from_hexstr(obj_expected);

	const ns_git::ObjectDataInfo loose = ns_git::get_object_data_info(ns_git::inflatebuf(objloose));
	if (loose.m_type != type_expected)
		throw PsConExc();

	git_oid oid_loose = {};
	if (!!git_odb_hash(&oid_loose, objloose.data() + loose.m_data_offset, objloose.size() - loose.m_data_offset, otype_expected))
		throw PsConExc();

	if (!!git_oid_cmp(&oid_expected, &oid_loose))
		throw PsConExc();
}

void get_write_object_raw(git_repository *repo, const shahex_t &obj, const std::string &objloose)
{
	ensure_object_match(obj, objloose, "tree", GIT_OBJ_TREE);
	assert(!!git_repository_path(repo));
	/* get temp and final path */
	const std::string temppath = (boost::filesystem::temp_directory_path() / boost::filesystem::unique_path()).string();
	const std::string objectpath = std::string(git_repository_path(repo)) + "/objects/" + obj.substr(0, 2) + "/" + obj.substr(2, std::string::npos);
	/* write temp */
	std::ofstream ff(temppath, std::ios::out | std::ios::trunc | std::ios::binary);
	ff.write(objloose.data(), objloose.size());
	ff.flush();
	ff.close();
	if (! ff.good())
		throw PsConExc();
	/* prepare final */
	const char *repodir = git_repository_path(repo);
	assert(repodir);
	const std::string objectpathdir = boost::filesystem::path(objectpath).parent_path().string();
	assert(objectpathdir.find(".git") != std::string::npos);
	boost::filesystem::create_directories(objectpathdir);
	/* write final */
	boost::filesystem::rename(temppath, objectpath);
}

std::vector<shahex_t> get_trees_writing_rec(PsCon *client, git_repository *repo, const std::string &tree)
{
	std::vector<shahex_t> out;

	get_write_object_raw(repo, tree, get_object(client, tree));

	unique_ptr_gittree t(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(tree)));
	for (size_t i = 0; i < git_tree_entrycount(t.get()); ++i)
		if (git_tree_entry_filemode(git_tree_entry_byindex(t.get(), i)) == GIT_FILEMODE_TREE)
			for (const auto &elt : get_trees_writing_rec(client, repo, ns_git::hexstr_from_oid(*git_tree_entry_id(git_tree_entry_byindex(t.get(), i)))))
				out.push_back(elt);
	return out;
}

std::vector<shahex_t> get_trees_writing(PsCon *client, git_repository *repo, const shahex_t &tree)
{
	std::vector<shahex_t> shas = get_trees_writing_rec(client, repo, tree);
	return shas;
}

std::vector<shahex_t> get_blobs_writing(PsCon *client, git_repository *repo, const std::vector<shahex_t> &trees)
{
	std::vector<shahex_t> blobs;

	for (const auto &elt : trees) {
		unique_ptr_gittree t(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(elt)));
		for (size_t i = 0; i < git_tree_entrycount(t.get()); ++i)
			if (git_tree_entry_type(git_tree_entry_byindex(t.get(), i)) == GIT_FILEMODE_BLOB ||
				git_tree_entry_filemode(git_tree_entry_byindex(t.get(), i)) == GIT_FILEMODE_BLOB_EXECUTABLE)
			{
				blobs.push_back(elt);
			}
	}

	for (const auto &blob : blobs)
		get_write_object_raw(repo, blob, get_object(client, blob));

	return blobs;
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
}

#ifndef _PS_UPDATER_TESTING

int main(int argc, char **argv)
{
  git_libgit2_init();

  PsCon client("perder.si", "5201");
  
  return EXIT_SUCCESS;
}

#else /* _PS_UPDATER_TESTING */

unique_ptr_gitrepository _git_repository_ensure(const std::string &repopath)
{
	git_repository *repo = NULL;
	git_repository_init_options init_options = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	init_options.flags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR;
	assert(init_options.version == 1);

	int err = git_repository_init_ext(&repo, repopath.c_str(), &init_options);
	if (!!err && err == GIT_EEXISTS)
		return unique_ptr_gitrepository(ns_git::repository_open(repopath.c_str()));
	if (!!err)
		throw std::runtime_error("ensure repository init");

	return unique_ptr_gitrepository(repo, ns_git::repo_delete);
}

int main(int argc, char **argv)
{
	git_libgit2_init();

	boost::filesystem::path tempdir = (boost::filesystem::temp_directory_path() / "ps_updater" / boost::filesystem::unique_path());
	boost::filesystem::path repodir = tempdir / "repo";
	boost::filesystem::create_directories(repodir);

	std::cout << "repodir: " << repodir.string() << std::endl;

	unique_ptr_gitrepository repo(_git_repository_ensure(repodir.string()));

	PsCon client("api.localhost.localdomain", "5201");

	std::cout << "qq " << get_head(&client, "master") << std::endl;

	return EXIT_FAILURE;
}

#endif /* _PS_UPDATER_TESTING */
