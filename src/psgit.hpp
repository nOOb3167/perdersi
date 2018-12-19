#ifndef _PSGIT_HPP_
#define _PSGIT_HPP_

#include <cassert>
#include <cctype>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/regex.hpp>
#include <git2.h>
#include <miniz.h>

#include <psmisc.hpp>

typedef ::std::unique_ptr<git_blob, void(*)(git_blob *)> unique_ptr_gitblob;
typedef ::std::unique_ptr<git_buf, void(*)(git_buf *)> unique_ptr_gitbuf;
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
	inline ObjectDataInfo(const std::string &data, const std::string &type, size_t data_offset) :
		m_type(type),
		m_data_offset(data_offset)
	{
		if (m_data_offset >= data.size())
			throw std::runtime_error("odi data_offset");
	}

	std::string m_type;
	size_t m_data_offset;
};

inline std::string
inflatebuf(const std::string &buf)
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

inline ObjectDataInfo
get_object_data_info(const std::string &data)
{
	/* format: "(type)(space)(number)(NULL)" */
	boost::cmatch what;
	if (! boost::regex_search(data.c_str(), what, boost::regex("([[:alpha:]]+) ([[:digit:]]+)\000"), boost::match_continuous))
		throw std::runtime_error("hdr regex");
	return ObjectDataInfo(data, what[1], (what[0].second + 1) - what[0].first);
}

inline git_oid
oid_from_hexstr(const shahex_t &str)
{
	git_oid oid = {};
	if (str.size() != GIT_OID_HEXSZ || !!git_oid_fromstr(&oid, str.c_str()))
		throw std::runtime_error("oid");
	return oid;
}

inline std::string
hexstr_from_oid(const git_oid &oid)
{
	char buf[GIT_OID_HEXSZ + 1] = {};
	return std::string(git_oid_tostr(buf, sizeof buf, &oid));
}

inline void
check_hexstr_equals(const shahex_t &a, const shahex_t &b)
{
	git_oid a_ = oid_from_hexstr(a), b_ = oid_from_hexstr(b);
	if (!!git_oid_cmp(&a_, &b_))
		throw std::runtime_error("not hexstr equals");
}

inline git_otype
otype_from_type(const std::string &type)
{
	static std::map<std::string, git_otype> tbl = {{"commit", GIT_OBJ_COMMIT}, {"tree", GIT_OBJ_TREE}, {"blob", GIT_OBJ_BLOB}};
	if (tbl.find(type) == tbl.end())
		throw std::runtime_error("type otype");
	return tbl[type];
}

inline shahex_t
get_object_data_hexstr(const std::string &incoming_data)
{
	const ns_git::ObjectDataInfo info = ns_git::get_object_data_info(incoming_data);
	git_oid oid_loose = {};
	if (!!git_odb_hash(&oid_loose, incoming_data.data() + info.m_data_offset, incoming_data.size() - info.m_data_offset, ns_git::otype_from_type(info.m_type)))
		throw std::runtime_error("object data oid hash");
	return hexstr_from_oid(oid_loose);
}

inline bool
tree_entry_filemode_bloblike_is(git_tree *t, size_t i)
{
	return (git_tree_entry_filemode(git_tree_entry_byindex(t, i)) == GIT_FILEMODE_BLOB ||
			git_tree_entry_filemode(git_tree_entry_byindex(t, i)) == GIT_FILEMODE_BLOB_EXECUTABLE);
}

inline std::string
shahex_from_refcontent(const std::string &refcontent)
{
	// https://github.com/git/git/blob/master/refs/files-backend.c#L347
	// https://github.com/git/git/blob/master/strbuf.c#L109
	//   files_read_raw_ref uses strbuf_rtrim
	//   rtrim right trims using std::isspace
	std::string r(refcontent);
	while (r.size() && std::isspace(r[r.size() - 1]))
		r.pop_back();
	if (r.size() != GIT_OID_HEXSZ)
		throw std::runtime_error("refcontent size");
	return r;
}

inline std::string
shahex_tree_from_comtcontent(const std::string &comtcontent)
{
	// https://github.com/git/git/blob/master/commit.c#L386
	//   parse_commit_buffer
	const int tree_entry_len = GIT_OID_HEXSZ + 5;
	if (comtcontent.size() <= tree_entry_len + 1 || comtcontent.substr(0, 5) != "tree " || comtcontent[tree_entry_len] != '\n')
		throw PsConExc();
	return ns_git::hexstr_from_oid(ns_git::oid_from_hexstr(comtcontent.substr(5, GIT_OID_HEXSZ)));
}

inline bool
odb_exists(git_odb *odb, git_oid obj)
{
	return git_odb_exists(odb, &obj);
}

inline void blob_delete(git_blob *p) { if (p) git_blob_free(p); }
inline void buf_delete(git_buf *p) { if (p) { git_buf_dispose(p); delete p; } }
inline void commit_delete(git_commit *p) { if (p) git_commit_free(p); }
inline void odb_delete(git_odb *p) { if (p) git_odb_free(p); }
inline void repo_delete(git_repository *p) { if (p) git_repository_free(p); }
inline void sig_delete(git_signature *p) { if (p) git_signature_free(p); }
inline void tree_delete(git_tree *p) { if (p) git_tree_free(p); }

inline unique_ptr_gitblob
blob_lookup(git_repository *repo, const git_oid oid)
{
	git_blob *p = nullptr;
	if (!!git_blob_lookup(&p, repo, &oid))
		throw std::runtime_error("blob lookup");
	return unique_ptr_gitblob(p, blob_delete);
}

inline unique_ptr_gitbuf
buf_new()
{
	return unique_ptr_gitbuf(new git_buf(), buf_delete);
}

inline unique_ptr_gitcommit
commit_lookup(git_repository *repo, const git_oid oid)
{
	git_commit *p = nullptr;
	if (!!git_commit_lookup(&p, repo, &oid))
		throw std::runtime_error("commit lookup");
	return unique_ptr_gitcommit(p, commit_delete);
}

inline unique_ptr_gitodb
odb_from_repo(git_repository *repo)
{
	git_odb *odb = nullptr;
	if (!!git_repository_odb(&odb, repo))
		throw std::runtime_error("odb repository");
	return unique_ptr_gitodb(odb, odb_delete);
}

inline unique_ptr_gitrepository
repository_open(std::string path)
{
	git_repository *p = NULL;
	if (!! git_repository_open(&p, path.c_str()))
		throw std::runtime_error("repository");
	assert(git_repository_path(p));
	return unique_ptr_gitrepository(p, repo_delete);
}

inline unique_ptr_gitsignature
sig_new_dummy()
{
	git_signature *sig = nullptr;
	if (!! git_signature_new(&sig, "DummyName", "DummyEMail", 0, 0))
		throw std::runtime_error("signature");
	return unique_ptr_gitsignature(sig, sig_delete);
}

inline unique_ptr_gittree
tree_lookup(git_repository *repo, const git_oid oid)
{
	git_tree *p = nullptr;
	if (!!git_tree_lookup(&p, repo, &oid))
		throw std::runtime_error("tree lookup");
	return unique_ptr_gittree(p, tree_delete);
}

inline std::vector<unique_ptr_gittree>
tree_lookup_v(git_repository *repo, const std::vector<shahex_t> &oids)
{
	std::vector<unique_ptr_gittree> trees;
	for (const auto &a : oids)
		trees.push_back(ns_git::tree_lookup(repo, ns_git::oid_from_hexstr(a)));
	return trees;
}

inline unique_ptr_gitrepository
repository_ensure(const std::string &repopath)
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

inline void
checkout_obj(git_repository *repo, const shahex_t &tree, const std::string &chkoutdir)
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

#endif /* _PSGIT_HPP_ */
