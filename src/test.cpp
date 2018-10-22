#include <cassert>
#include <cstdlib>>
#include <memory>
#include <stdexcept>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>
#include <git2.h>
#include <miniz.h>

using shahex_t = ::std::string;

typedef ::std::unique_ptr<git_tree, void(*)(git_tree *)> unique_ptr_gittree;

namespace ns_git
{

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

	strm.avail_in = buf.size();
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

void get_object_data_info(const std::string &data, std::string *out_type, size_t *out_data_offset)
{
	/* see function get_object_header in odb_loose.c */
	/* also this comment from git source LUL: (sha1_file.c::parse_sha1_header_extended)
	*   """We used to just use "sscanf()", but that's actually way
	*   too permissive for what we want to check. So do an anal
	*   object header parse by hand."""
	*/

	/* format: "[:alpha:]+ [:digit:]+\0" */
	/*   two space separated fields. NULL at the end. */

	/* parse type string */
	size_t spc;
	if ((spc = data.find_first_of(' ', 0)) == std::string::npos)
		throw std::runtime_error("hdr spc");
	/* parse size string */
	if (data.find_first_not_of("0123456789", spc + 1) == std::string::npos)
		throw std::runtime_error("hdr num");
	if (data.at(data.find_first_not_of("0123456789", spc + 1)) != '\0')
		throw std::runtime_error("hdr null");

	*out_type = data.substr(0, data.find_first_of(' ', 0));
	*out_data_offset = data.find_first_not_of("0123456789", spc + 1);
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

void tree_delete(git_tree * p)
{
	if (p)
		git_tree_free(p);
}

unique_ptr_gittree tree_lookup(git_repository *repo, const git_oid oid)
{
	git_tree *p = NULL;
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
		if (res.result_int != 200)
			throw PsConExc();
	}

	std::string m_host;
	std::string m_port;
	std::string m_host_http;
	boost::asio::io_context m_ioc;
	tcp::resolver m_resolver;
	tcp::socket m_socket;
};

std::string get_head(PsCon *client, const std::string &refname);

std::string get_object(PsCon *client, const std::string &objhex)
{
	std::string fst = objhex.substr(0, 2);
	std::string snd = objhex.substr(2, std::string::npos);
	std::string loose = client->reqPost_("/lowtech/objects/" + fst + "/" + snd, "").body;
	return loose;
	std::string treedata = ns_git::inflatebuf(loose);
}

std::string get_head(PsCon *client, const std::string &refname)
{
	std::string objhex = client->reqPost_("/lowtech/refs/heads/" + refname, "").body;

	if (objhex.size() != GIT_OID_HEXSZ)
		throw PsConExc();

	return objhex;
}

void ensure_object_match(const std::string &objloose, const shahex_t &obj_expected, const std::string &type_expected, git_otype otype_expected)
{
	git_oid oid_expected = ns_git::oid_from_hexstr(obj_expected);

	std::string type_loose;
	size_t data_offset;
	ns_git::get_object_data_info(ns_git::inflatebuf(objloose), &type_loose, &data_offset);
	if (type_loose != type_expected)
		throw PsConExc();

	git_oid oid_loose = {};
	if (!!git_odb_hash(&oid_loose, objloose.data() + data_offset, objloose.size() - data_offset, otype_expected))
		throw PsConExc();

	if (!!git_oid_cmp(&oid_expected, &oid_loose))
		throw PsConExc();
}

void get_write_object_raw(git_repository *repo, const shahex_t &obj, const std::string &objloose)
{
	ensure_object_match(obj, objloose, "tree", GIT_OBJ_TREE);
	assert(!!git_repository_path(repo));
	/* get temp and final path */
	std::string temppath = (boost::filesystem::temp_directory_path() / boost::filesystem::unique_path()).string();
	std::string objectpath = std::string(git_repository_path(repo)) + "/objects/" + obj.substr(0, 2) + "/" + obj.substr(2, std::string::npos);
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
	std::string objectpathdir = boost::filesystem::path(objectpath).parent_path().string();
	assert(objectpathdir.find(".git") != std::string::npos);
	boost::filesystem::create_directories(objectpathdir);
	/* write final */
	boost::filesystem::rename(temppath, objectpath);
}

std::vector<shahex_t> get_trees_writing_rec(PsCon *client, git_repository *repo, const std::string &tree)
{
	bool error = false;
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

	for (const auto &elt : blobs) {
	get_write_object_raw(repo, )
}

int main(int argc, char **argv)
{
  git_libgit2_init();

  PsCon client("perder.si", "5201");
  
  return EXIT_SUCCESS;
}
