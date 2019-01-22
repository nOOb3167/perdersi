#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

// GRRR
#include <psasio.hpp>

#include <boost/regex.hpp>
#include <Eigen/Dense>
#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Window.hpp>

#include <pscruft.hpp>
#include <ps_b1.h>

#define PS_MAX(a, b) ((a) > (b)) ? (a) : (b))
#define PS_MIN(a, b) ((a) < (b)) ? (a) : (b))

#define PS_GLSYNC_FLAGS (GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)

#define PS_F2UI16(f) uint16_t(std::round((f) * 65535))

#define PS_MEMBARR_ELTTYPE(VERTCLAS, MEMBARR) std::remove_reference<decltype(VERTCLAS::MEMBARR[0])>::type
#define PS_VERT_SLOT(VARNAME, VERTCLAS, MEMBARR, MEMTYPE, FCONV)																								\
	static_assert(std::rank<decltype(VERTCLAS::MEMBARR)>::value == 1,                  "not array [MEMBARR]");													\
	static_assert(std::is_same<MEMTYPE, PS_MEMBARR_ELTTYPE(VERTCLAS, MEMBARR)>::value, "member type mismatch [MEMTYPE]");										\
	const GxVertSlot VARNAME { offsetof(VERTCLAS, MEMBARR), sizeof(PS_MEMBARR_ELTTYPE(VERTCLAS, MEMBARR)), std::extent<decltype(VERTCLAS::MEMBARR)>::value };	\
	const std::function<MEMTYPE(const std::string &s)> VARNAME ## _fconv_aux = (FCONV);																			\
	fconv_t VARNAME ## _fconv = [& VARNAME ## _fconv_aux ] (uint8_t *p, const std::string &s) { *(MEMTYPE *)p = VARNAME ## _fconv_aux(s); }

#define PS_BONE_UNIFORM_MAX 64

const double PS_PI = 3.14159265358979323846;

// https://eigen.tuxfamily.org/dox/group__TutorialGeometry.html
//   If you are working with OpenGL 4x4 matrices then Affine3f and Affine3d are what you want.
//   Since Eigen defaults to column-major storage, you can directly use the Transform::data() method to pass your transformation matrix to OpenGL.
// GL_UNIFORM_BUFFER bindings are NOT VAO STATE
// https://www.khronos.org/opengl/wiki/Sync_Object#Flushing_and_contexts
//   about GLsync (Fence Syncs) and flushing
//   (GLspec46 @ 4.1.2 Signaling)
// ::boost::property_tree::json_parser::write_json(std::cout, ptree, true);
// round(x * 3) / 3
// https://eigen.tuxfamily.org/dox/classEigen_1_1Quaternion.html
//   Operations interpreting the quaternion as rotation have undefined behavior if the quaternion is not normalized.
//https://blenderartists.org/t/get-bone-position-data-matrix-relative-to-parent-bone/1116191/5
//  rest_matrix * fcurve_key.to_matrix()
//  https://github.com/HENDRIX-ZT2/bfb-blender/blob/master/common_bfb.py#L178
//    def get_bfb_matrix(bone):
//  https://github.com/HENDRIX-ZT2/bfb-blender/blob/master/export_bf.py#L131
//    export_keymat(rest_rot, ...)
// https://docs.blender.org/api/blender2.8/bpy.types.PoseBone.html#bpy.types.PoseBone.matrix_basis
//   'Alternative access to location/scale/rotation relative to the parent and own rest bone'
//   bpy.data.objects['Armature'].pose.bones['Bone'].matrix_basis
//   bpy.data.objects['Armature'].pose.bones['Bone'].rotation_quaternion.normalized().to_matrix()
//     seem to match
// https://github.com/MicrosoftDocs/visualstudio-docs/blob/master/docs/debugger/create-custom-views-of-native-objects.md
// .natvisreload command in the Watch window

namespace ei = ::Eigen;

template<typename T>
using sp = ::std::shared_ptr<T>;

using st = size_t;

using weit_t = std::tuple<std::string, float>;

using A3f = ::ei::Transform<float, 3, ei::Affine, ei::DontAlign>;
using M4f = ::ei::Matrix<float, 4, 4, ei::DontAlign>;
using Mp4f = ::ei::Map<::ei::Matrix<float, 4, 4, ei::DontAlign> >;
using V3f = ::ei::Matrix<float, 3, 1, ei::DontAlign>;
using Qf = ::ei::Quaternion<float, ei::DontAlign>;

typedef std::function<void(uint8_t *, const std::string &)> fconv_t;

inline pt_t::const_assoc_iterator
begin(pt_t &v)
{
	return v.ordered_begin();
}

inline pt_t::const_assoc_iterator
end(pt_t &v)
{
	return v.not_found();
}

inline std::vector<float>
_m2v(const std::vector<M4f> &v)
{
	const size_t szmat = 4 * 4;
	std::vector<float> a(v.size() * szmat);
	for (size_t i = 0; i < v.size(); i++)
		memcpy(a.data() + i * szmat, v[i].data(), szmat * sizeof(float));
	return a;
}

inline std::vector<float>
_m2v_multi(const std::vector<std::vector<M4f> > &v)
{
	std::vector<float> a;
	for (size_t i = 0; i < v.size(); i++) {
		auto &b = _m2v(v[i]);
		std::copy(b.begin(), b.end(), std::back_inserter(a));
	}
	return a;
}

class PaExc : public std::runtime_error
{
public:
	inline PaExc() : std::runtime_error("PaExc") {}
};

class PaBone
{
public:
	inline PaBone(const std::string &name, const M4f &matx) :
		m_name(name),
		m_matx(matx)
	{}

	std::string m_name;
	M4f m_matx;
};

inline M4f
mtx_parent(const std::vector<M4f> &mtxs, const std::string &arpai, const std::map<std::string, uint16_t> &map_str, const M4f &def)
{
	if (uint16_t i = map_str.at(arpai); i != uint16_t(-1))
		return mtxs[i];
	return def;
}

inline M4f
mtx_us(const std::vector<PaBone> &bone, const std::string &arnai, const std::map<std::string, uint16_t> &map_str)
{
	return bone[map_str.at(arnai)].m_matx;
}

inline M4f
mtx_fcrv(const std::map<std::string, std::vector<M4f> > &bonemat, const std::string &arnai, size_t nfram)
{
	if (auto it = bonemat.find(arnai); it != bonemat.end())
		return it->second.at(nfram);
	return M4f::Identity();
}

class PaUvLa
{
public:
	inline PaUvLa(const std::string &name, const std::vector<float> &layr) :
		m_name(name),
		m_layr(layr)
	{}

	std::string m_name;
	std::vector<float> m_layr;
};

class PaWeit
{
public:
	std::vector<uint32_t> m_id;
	std::vector<float> m_wt;
};

#pragma pack (push, 1)
class GxVert
{
public:
	float m_vert[3];
	uint16_t m_uv[2];
	uint16_t m_weid[4];
	float m_wewt[4];
};
#pragma pack (pop)

class PaModl
{
public:
	std::string m_name;
	std::vector<GxVert> m_data;
};

class PaArmt
{
public:
	std::string m_name;
	M4f m_matx;
	std::vector<PaBone> m_bone;

	std::map<std::string, uint16_t> m_map_str;
	std::map<uint16_t, std::string> m_map_int;

	std::vector<std::string> m_ar_pa;
	std::vector<std::string> m_ar_na;
};

class PaActn1
{
public:
	std::map<std::string, std::vector<M4f> > m_bonemat;
	size_t m_nframe;
};

class PaActn
{
public:
	std::map<std::string, sp<PaActn1> > m_actn;
};

class GxVertSlot
{
public:
	size_t m_off;
	size_t m_eltsize;
	size_t m_vecsize;

	inline size_t
	get_off(size_t i) const
	{
		return m_off + m_eltsize * i;
	}
};

class Pa
{
public:
	inline Pa(const std::string &s) :
		m_s(s),
		m_pt()
	{
		boost::property_tree::json_parser::read_json(std::stringstream(m_s), m_pt);
	}

	inline size_t
	_checked_nframe(const std::vector<M4f> &scas, const std::vector<M4f> &rots, const std::vector<M4f> &locs)
	{
		std::vector<size_t> tmp;
		if (scas.size()) tmp.push_back(scas.size());
		if (rots.size()) tmp.push_back(rots.size());
		if (locs.size()) tmp.push_back(locs.size());
		assert(tmp.size() && std::equal(tmp.begin()++, tmp.end(), tmp.begin()));
		return *tmp.begin();
	}

	inline M4f
	_r2a(const ei::Matrix3f &a)
	{
		M4f m;
		m << a(0, 0), a(0, 1), a(0, 2), 0,
			a(1, 0), a(1, 1), a(1, 2), 0,
			a(2, 0), a(2, 1), a(2, 2), 0,
			0, 0, 0, 1;
		return m;
	}

	inline std::vector<float>
	_vec(const pt_t &node, size_t vecsizehint)
	{
		std::vector<float> v;
		assert(node.size() == vecsizehint);
		for (auto it = node.ordered_begin(); it != node.not_found(); ++it)
			v.push_back(std::stof(it->second.data()));
		return v;
	}

	inline void
	_vecflatten(const pt_t &node, const fconv_t &fconv, const GxVertSlot &vs, std::vector<GxVert> &v)
	{
		size_t c = 0;
		assert(v.size() == node.size());
		for (auto it = node.ordered_begin(); it != node.not_found(); ++it, c++) {
			size_t c2 = 0;
			assert(it->second.size() == vs.m_vecsize);
			for (auto it2 = it->second.ordered_begin(); it2 != it->second.not_found(); ++it2, c2++)
				fconv(((uint8_t *)(v.data() + c)) + vs.get_off(c2), it2->second.data());
		}
	}

	inline std::tuple<std::map<std::string, uint16_t>, std::map<uint16_t, std::string> >
	_bonemap_hier(const std::vector<std::string> &arna)
	{
		std::map<std::string, uint16_t> map_str;
		std::map<uint16_t, std::string> map_int;

		map_str["NONE"] = -1;
		for (uint16_t i = 0; i < arna.size(); i++)
			map_str[arna[i]] = i;
		for (auto &[k, v] : map_str)
			map_int[v] = k;
		assert(1 + arna.size() == map_str.size());

		return std::make_tuple(std::move(map_str), std::move(map_int));
	}

	inline void
	_bone_hier_rec(const pt_t::const_assoc_iterator &it, std::vector<std::string> &arpa, std::vector<std::string> &arna)
	{
		for (auto it2 = it->second.ordered_begin(); it2 != it->second.not_found(); ++it2) {
			arpa.push_back(it->first);
			arna.push_back(it2->first);
			_bone_hier_rec(it2, arna, arpa);
		}
	}

	inline std::tuple<std::vector<std::string>, std::vector<std::string> >
	_bone_hier(const pt_t &tree)
	{
		std::vector<std::string> arpa;
		std::vector<std::string> arna;
		assert(tree.size() == 1);
		arpa.push_back("NONE");
		arna.push_back(tree.ordered_begin()->first);
		_bone_hier_rec(tree.ordered_begin(), arpa, arna);
		return std::make_tuple(std::move(arpa), std::move(arna));
	}

	inline void
	_modl_pre(pt_t &modl_)
	{
		assert(modl_.size() == 1);
		pt_t &modl = modl_.begin()->second;
		pt_t &weit = modl.get_child("weit");
		pt_t &bna = weit.get_child("bna");
		pt_t &bwt = weit.get_child("bwt");
		const std::string fzero = std::to_string(0.0f);
		assert(bna.size() == bwt.size());
		for (auto it = bna.ordered_begin(); it != bna.not_found(); ++it)
			while (it->second.size() < 4)
				it->second.push_back(std::make_pair("", pt_t("NONE")));
		for (auto it = bwt.ordered_begin(); it != bwt.not_found(); ++it)
			while (it->second.size() < 4)
				it->second.push_back(std::make_pair("", pt_t(fzero)));
	}

	inline sp<PaModl>
	_modl(pt_t &modl_, const pt_t &armt_)
	{
		_modl_pre(modl_);

		assert(modl_.size() == 1);
		const pt_t &modl = modl_.begin()->second;
		const pt_t &vert = modl.get_child("vert");
		const pt_t &indx = modl.get_child("indx");
		const pt_t &uvla = modl.get_child("uvla");
		const pt_t &weit = modl.get_child("weit");
		const pt_t &bna = weit.get_child("bna");
		const pt_t &bwt = weit.get_child("bwt");
		assert(armt_.size() == 1);
		const pt_t &armt = armt_.begin()->second;
		const pt_t &tree = armt.get_child("tree");

		assert(uvla.size() == 1);
		assert(indx.size() == vert.size() && indx.size() == uvla.begin()->second.size());
		assert(indx.size() == bna.size() && indx.size() == bwt.size());
		std::vector<GxVert> v(indx.size());

		const auto &[bone_map_str, bone_map_int] = _bonemap_hier(std::get<1>(_bone_hier(tree)));
		auto f_bone_id = [&bone_map_str](const std::string &s) { return (uint16_t) bone_map_str.find(s)->second; };
		auto f_float = [](const std::string &s) { return std::stof(s); };
		auto f_f2ui16 = [](const std::string &s) { return PS_F2UI16(std::stof(s)); };

		PS_VERT_SLOT(slot_vert, GxVert, m_vert, float, f_float);
		PS_VERT_SLOT(slot_uv, GxVert, m_uv, uint16_t, f_f2ui16);
		PS_VERT_SLOT(slot_weid, GxVert, m_weid, uint16_t, f_bone_id);
		PS_VERT_SLOT(slot_wewt, GxVert, m_wewt, float, f_float);
		_vecflatten(vert, slot_vert_fconv, slot_vert, v);
		_vecflatten(uvla.begin()->second, slot_uv_fconv, slot_uv, v);
		_vecflatten(bna, slot_weid_fconv, slot_weid, v);
		_vecflatten(bwt, slot_wewt_fconv, slot_wewt, v);

		sp<PaModl> q(new PaModl());
		q->m_name = modl_.begin()->first;
		q->m_data = std::move(v);

		return q;
	}

	inline sp<PaArmt>
	_armt(const pt_t &armt_)
	{
		assert(armt_.size() == 1);
		const pt_t &armt = armt_.begin()->second;
		const pt_t &matx = armt.get_child("matx");
		const pt_t &bone = armt.get_child("bone");
		const pt_t &tree = armt.get_child("tree");
		M4f amatx(Mp4f(_vec(matx, 4 * 4).data()));
		sp<PaArmt> q(new PaArmt());
		q->m_name = armt_.begin()->first;
		q->m_matx = Mp4f(_vec(matx, 4 * 4).data());
		std::tie(q->m_ar_pa, q->m_ar_na) = _bone_hier(tree);
		std::tie(q->m_map_str, q->m_map_int) = _bonemap_hier(q->m_ar_na);
		for (auto &[bna, bma] : bone)
			q->m_bone.push_back(PaBone(bna, Mp4f(_vec(bma, 4 * 4).data())));
		std::sort(q->m_bone.begin(), q->m_bone.end(), [&q](const PaBone &a, const PaBone &b) { return q->m_map_str.at(a.m_name) < q->m_map_str.at(b.m_name); });
		return q;
	}

	inline sp<PaActn>
	_actn(const pt_t &actn_)
	{
		sp<PaActn> q(new PaActn());
		for (auto &[anam, adat] : actn_) {
			const pt_t &fcrv = adat.get_child("fcrv");
			sp<PaActn1> w(new PaActn1());
			for (auto &[bnam, blanes] : fcrv) {
				std::vector<M4f> scas;
				std::vector<M4f> rots;
				std::vector<M4f> locs;
				for (auto &[lnam, lane] : blanes) {
					const pt_t &m0 = lane.get_child("0");
					const pt_t &m1 = lane.get_child("1");
					const pt_t &m2 = lane.get_child("2");
					const boost::optional<const pt_t &> &m3 = lane.get_child_optional("3");
					std::vector<float> v0 = _vec(m0, m0.size());
					std::vector<float> v1 = _vec(m1, m1.size());
					std::vector<float> v2 = _vec(m2, m2.size());
					std::vector<float> v3 = m3 ? _vec(*m3, (*m3).size()) : std::vector<float>();
					assert(v0.size() == v1.size() && v0.size() == v2.size() && (v3.empty() || (v0.size() == v3.size())));
					// Quaternion convention is WXYZ
					if (lnam == "location")
						for (size_t i = 0; i < v0.size(); i++)
							locs.push_back(A3f(A3f::Identity()).translate(V3f(v0[i], v1[i], v2[i])).matrix());
					else if (lnam == "rotation_quaternion")
						for (size_t i = 0; i < v0.size(); i++)
							rots.push_back(_r2a(Qf(v0[i], v1[i], v2[i], v3[i]).normalized().matrix()));
					else if (lnam == "scale")
						for (size_t i = 0; i < v0.size(); i++)
							scas.push_back(A3f(A3f::Identity()).scale(V3f(v0[i], v1[i], v2[i])).matrix());
					else
						throw PaExc();
				}
				const size_t nframe = _checked_nframe(scas, rots, locs);
				std::vector<M4f> mats;
				for (size_t i = 0; i < nframe; i++) {
					// Transform order is SCA ROT LOC
					M4f m(M4f::Identity());
					if (scas.size()) m = scas[i] * m;
					if (rots.size()) m = rots[i] * m;
					if (locs.size()) m = locs[i] * m;
					mats.push_back(m);
				}
				w->m_bonemat[bnam] = std::move(mats);
				w->m_nframe = nframe;
			}
			q->m_actn[anam] = w;
		}
		return q;
	}

	inline void
	pars()
	{
		m_modl = _modl(m_pt.get_child("modl"), m_pt.get_child("armt"));
		m_armt = _armt(m_pt.get_child("armt"));
		m_actn = _actn(m_pt.get_child("actn"));
	}

	std::string m_s;
	pt_t m_pt;

	sp<PaModl> m_modl;
	sp<PaArmt> m_armt;
	sp<PaActn> m_actn;
};

class GxActn1
{
public:
	inline GxActn1() :
		m_nbone(0),
		m_nfram(0),
		m_ubo(0)
	{}

	inline ~GxActn1()
	{
		glDeleteBuffers(1, &m_ubo);
	}

	size_t m_nbone;
	size_t m_nfram;
	GLuint m_ubo;
};

class GxActn
{
public:
	inline GxActn() :
		m_actn()
	{}

	inline std::tuple<std::vector<std::vector<M4f> >, size_t>
	posebld(const sp<Pa> &pa, const std::string &anam)
	{
		const size_t nbone = pa->m_armt->m_bone.size();
		const auto &arna = pa->m_armt->m_ar_na;
		const auto &arpa = pa->m_armt->m_ar_pa;
		const auto &bone = pa->m_armt->m_bone;
		const auto &map_str = pa->m_armt->m_map_str;
		const auto &mactn = pa->m_actn->m_actn;
		const auto &bonemat = mactn.at(anam)->m_bonemat;
		const auto &armtmtx = pa->m_armt->m_matx;

		const size_t nfram = mactn.at(anam)->m_nframe;

		std::vector<std::vector<M4f> > bonemtx;

		assert(arna.size() == nbone && arpa.size() == nbone);

		for (size_t f = 0; f < nfram; f++) {
			std::vector<M4f> bonemtx_(nbone);
			for (size_t i = 0; i < nbone; i++) {
				const M4f p = mtx_parent(bonemtx_, arpa[i], map_str, armtmtx);
				const M4f r = mtx_us(bone, arna[i], map_str);
				const M4f s = mtx_fcrv(bonemat, arna[i], f);
				bonemtx_[i] = mtx_parent(bonemtx_, arpa[i], map_str, armtmtx) * mtx_us(bone, arna[i], map_str) * mtx_fcrv(bonemat, arna[i], f);
			}
			bonemtx.push_back(bonemtx_);
		}

		return std::make_tuple(std::move(bonemtx), nbone);
	}

	inline std::tuple<GLuint, size_t, size_t>
	bufparm(const std::string &anam, size_t fram)
	{
		const size_t szmat = 4 * 4;
		const GxActn1 &actn = *m_actn.at(anam);
		assert(fram < actn.m_nfram);
		const size_t siz = actn.m_nbone * szmat * sizeof(float);
		const size_t off = fram * siz;
		return std::make_tuple(actn.m_ubo, off, siz);
	}

	inline void
	pars(const sp<Pa> &pa)
	{
		const size_t nbone = pa->m_armt->m_bone.size();
		const size_t szmat = 4 * 4;
		for (auto &[anam, actn] : pa->m_actn->m_actn) {
			sp<GxActn1> q(new GxActn1());

			auto &[bonemtx, _] = posebld(pa, anam);

			std::vector<float> bonemtx_(_m2v_multi(bonemtx));

			glCreateBuffers(1, &q->m_ubo);
			glNamedBufferStorage(q->m_ubo, bonemtx_.size() * sizeof bonemtx_[0], bonemtx_.data(), GL_MAP_WRITE_BIT);

			q->m_nbone = nbone;
			q->m_nfram = actn->m_nframe;

			m_actn[anam] = q;
		}
	}

	std::map<std::string, sp<GxActn1> > m_actn;
};

class GxModl
{
public:
	GxModl(const sp<Pa> &pa) :
		m_pa(pa),
		m_vbo(0),
		m_ubo_restmtx(0),
		m_ubo_bonemtx(0)
	{}

	~GxModl()
	{
		glDeleteBuffers(1, &m_vbo);
	}

	void pars()
	{
		glCreateBuffers(1, &m_vbo);
		glNamedBufferStorage(m_vbo, m_pa->m_modl->m_data.size() * sizeof m_pa->m_modl->m_data[0], m_pa->m_modl->m_data.data(), GL_MAP_WRITE_BIT);

		const auto &bone = m_pa->m_armt->m_bone;
		const size_t nbone = bone.size();
		const size_t szmat = 4 * 4;
		const auto &armtmtx = m_pa->m_armt->m_matx;
		const auto &arpa = m_pa->m_armt->m_ar_pa;
		const auto &arna = m_pa->m_armt->m_ar_na;
		const auto &map_str = m_pa->m_armt->m_map_str;
		assert(bone.size() <= PS_BONE_UNIFORM_MAX);
		//std::vector<float> restmtx(PS_BONE_UNIFORM_MAX * szmat);

		std::vector<M4f> restmtx(nbone);
		for (size_t i = 0; i < nbone; i++) {
			const M4f &tmp = mtx_parent(restmtx, arpa[i], map_str, armtmtx) * mtx_us(bone, arna[i], map_str);
			restmtx[i] = tmp.inverse();
		}
		std::vector<float> restmtx_(_m2v(restmtx));
		glCreateBuffers(1, &m_ubo_restmtx);
		glNamedBufferStorage(m_ubo_restmtx, restmtx_.size() * sizeof restmtx_[0], restmtx_.data(), GL_MAP_WRITE_BIT);

		std::vector<M4f> bonemtx(nbone);
		for (size_t i = 0; i < bone.size(); i++)
			bonemtx[i] = bone[i].m_matx;
		std::vector<float> bonemtx_(_m2v(bonemtx));
		glCreateBuffers(1, &m_ubo_bonemtx);
		glNamedBufferStorage(m_ubo_bonemtx, bonemtx_.size() * sizeof bonemtx_[0], bonemtx_.data(), GL_MAP_WRITE_BIT);
	}

	sp<Pa> m_pa;
	GLuint m_vbo;
	GLuint m_ubo_restmtx;
	GLuint m_ubo_bonemtx;
};

M4f _blender2sanity()
{
	M4f m;
	m << 1, 0, 0, 0,
		0, 0, 1, 0,
		0, 1, 0, 0,
		0, 0, 0, 1;
	return m;
}

M4f _perspective(float left, float right, float bottom, float top, float _near, float _far)
{
	float A = (right + left) / (right - left);
	float B = (top + bottom) / (top - bottom);
	float C = - (_far + _near) / (_far - _near);
	float D = - (2 * _far * _near) / (_far - _near);
	float X = (2 * _near) / (right - left);
	float Y = (2 * _near) / (top - bottom);
	M4f m;
	m << X, 0, A, 0,
		 0, Y, B, 0,
		 0, 0, C, D,
		 0, 0, -1, 0;
	return m;
}

M4f _lookat(const V3f &eye, const V3f &cen, const V3f &up)
{
	V3f f = (cen - eye).normalized();
	V3f u_ = up.normalized();
	V3f s = f.cross(u_);
	V3f u = s.cross(f);
	// https://www.cs.auckland.ac.nz/courses/compsci716s2t/resources/manpagesOpenGL/gluLookAt.html
	// https://math.stackexchange.com/questions/2145611/inverse-of-an-orthogonal-matrix-is-its-transpose/2145626
	//   remember that transpose is orthogonal inverse
	M4f q(M4f::Identity());
	q << s.x(), s.y(), s.z(), 0,
		 u.x(), u.y(), u.z(), 0,
		 -f.x(), -f.y(), -f.z(), 0,
		 0, 0, 0, 1;
	A3f trns(A3f::Identity());
	trns.translate(V3f(-eye.x(), -eye.y(), -eye.z()));
	M4f m = q * trns.matrix();
	return m;
}

void stuff()
{
	sp<Pa> pars(new Pa(std::string((char *)g_ps_b1, g_ps_b1_size)));
	pars->pars();

	sf::ContextSettings ctx(24);
	sf::RenderWindow win(sf::VideoMode(800, 600), "", sf::Style::Default, ctx);

	if (win.getSettings().majorVersion < 4 || (win.getSettings().majorVersion == 4 && win.getSettings().minorVersion < 5))
		throw PaExc();

	if (glewInit() != GLEW_OK)
		throw PaExc();

	sf::Shader sha;

	std::string sha_vs = R"EOF(
#version 460
layout(location = 0) in vec3 vert;
layout(location = 1) in vec2 uv;
layout(location = 2) in ivec4 weid;
layout(location = 3) in vec4 wewt;
out vec3 bary;
layout(binding = 0, std140) uniform Ubo0
{
	vec4 colr;
	mat4 proj;
	mat4 view;
} ubo0;
layout(binding = 1, std140) uniform Ubo1
{
	mat4 restmtx[1];
} ubo1;
layout(binding = 2, std140) uniform Ubo2
{
	mat4 bonemtx[1];
} ubo2;
layout(binding = 3, std140) uniform Ubo3
{
	mat4 bonemtx[64];
} ubo3;
void main()
{
	bary = vec3(mod(gl_VertexID - 0, 3) == 0, mod(gl_VertexID - 1, 3) == 0, mod(gl_VertexID - 2, 3) == 0);
	vec3 x = vec3(0, 0, 0);
	if (wewt[0] == 0.0f) {
		x = vert.xyz;
	} else {
		for (int i = 0; i < 4; i++) {
			vec4 t = ubo3.bonemtx[10+max(weid[i], 0)] * ubo1.restmtx[max(weid[i], 0)] * vec4(vert.xyz, 1);
			x += t.xyz * wewt[i];
		}
	}
	gl_Position = ubo0.proj * ubo0.view * vec4(x.xyz, 1);
	//gl_Position = ubo0.proj * ubo0.view * vec4(vert.xyz, 1);
}
)EOF";

	std::string sha_fs = R"EOF(
#version 460
in vec3 bary;
layout(location = 0) out vec4 color;
layout(binding = 0, std140) uniform Ubo0
{
	vec4 colr;
	mat4 proj;
	mat4 view;
} ubo0;
void main()
{
	color = bary.x < 0.05 || bary.y < 0.05 || bary.z < 0.05 ? vec4(1,0,0,1) : vec4(0,1,0,1);
}
)EOF";

	if (!sha.loadFromMemory(sha_vs, sha_fs))
		throw PaExc();

	A3f horz_rot(A3f::Identity());
	horz_rot.rotate(ei::AngleAxisf(float(0.01 * PS_PI), V3f::UnitY()));

	M4f proj(_perspective(-1, 1, -1, 1, 1, 10));

	V3f eye_pt(0, 0, 5);

	GLuint vao = 0;
	std::vector<GLuint> vbo(2);

	struct UColr {
		float colr[4] = { 0, 0, 1, 1 };
		float proj[16] = {};
		float view[16] = {};
	} colr;

	GLsync sync = 0;

	sp<GxModl> modl(new GxModl(pars));
	modl->pars();
	sp<GxActn> actn(new GxActn());
	actn->pars(pars);

	glCreateBuffers(GLsizei(vbo.size()), vbo.data());
	glNamedBufferData(vbo[0], pars->m_modl->m_data.size() * sizeof pars->m_modl->m_data[0], pars->m_modl->m_data.data(), GL_STATIC_DRAW);
	glNamedBufferStorage(vbo[1], sizeof colr, nullptr, PS_GLSYNC_FLAGS);

	glCreateVertexArrays(1, &vao);
	glVertexArrayVertexBuffer(vao, 0, vbo[0], 0, sizeof(GxVert));

	glEnableVertexArrayAttrib(vao, 0);
	glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(GxVert, m_vert));
	glVertexArrayAttribBinding(vao, 0, 0);

	glEnableVertexArrayAttrib(vao, 1);
	glVertexArrayAttribFormat(vao, 1, 2, GL_UNSIGNED_SHORT, GL_TRUE, offsetof(GxVert, m_uv));
	glVertexArrayAttribBinding(vao, 1, 0);

	glEnableVertexArrayAttrib(vao, 2);
	glVertexArrayAttribFormat(vao, 2, 4, GL_UNSIGNED_SHORT, GL_FALSE, offsetof(GxVert, m_weid));
	glVertexArrayAttribBinding(vao, 2, 0);

	glEnableVertexArrayAttrib(vao, 3);
	glVertexArrayAttribFormat(vao, 3, 4, GL_FLOAT, GL_FALSE, offsetof(GxVert, m_wewt));
	glVertexArrayAttribBinding(vao, 3, 0);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, vbo[1]);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, modl->m_ubo_restmtx);
	glBindBufferBase(GL_UNIFORM_BUFFER, 2, modl->m_ubo_bonemtx);

	auto &[aubo, aoff, asiz] = actn->bufparm("Anim0", 10);
	// FIXME: NSight does not show an active bufferbinding for glBindBufferRange
	//   shader edited for glBindBufferBase
	//glBindBufferRange(GL_UNIFORM_BUFFER, 3, aubo, aoff, asiz);
	glBindBufferBase(GL_UNIFORM_BUFFER, 3, aubo);

	glUniformBlockBinding(sha.getNativeHandle(), 0, 0);
	glUniformBlockBinding(sha.getNativeHandle(), 1, 1);
	glUniformBlockBinding(sha.getNativeHandle(), 2, 2);
	glUniformBlockBinding(sha.getNativeHandle(), 3, 3);

	auto timn = std::chrono::system_clock::now();

	while (win.isOpen()) {
		sf::Event event;
		while (win.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				goto end;
		}
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - timn).count() >= 50) {
				eye_pt = (horz_rot * eye_pt).eval();
				timn = std::chrono::system_clock::now();
			}
			M4f view(_lookat(eye_pt, V3f(0, 0, 0), V3f(0, 1, 0)) * _blender2sanity());

			if (sync != 0)
				if (glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, (GLuint64)-1) == GL_WAIT_FAILED)
					throw PaExc();
			UColr *p = (UColr *)glMapNamedBuffer(vbo[1], GL_WRITE_ONLY);
			Mp4f(colr.proj) = proj;
			Mp4f(colr.view) = view;
			*p = colr;
			glUnmapNamedBuffer(vbo[1]);

			glClearColor(1, 1, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			glEnable(GL_DEPTH_TEST);

			// https://github.com/SFML/SFML/blob/master/src/SFML/Graphics/RenderTarget.cpp#L482
			//   RenderTarget::resetGLStates()
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);

			sf::Shader::bind(&sha);
			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLES, 0, GLsizei(pars->m_modl->m_data.size()));
			glBindVertexArray(0);
			sf::Shader::bind(nullptr);

			glDeleteSync(sync);
			sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

			//win.pushGLStates();
			//win.popGLStates();
		}
		win.display();
	}
end:
	(void)0;
}

int main(int argc, char **argv)
{
	stuff();
	return EXIT_SUCCESS;
}
