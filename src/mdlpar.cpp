#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
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

#define PS_GLSYNC_FLAGS (GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)

#define PS_F2UI16(f) uint16_t(std::round((f) * 65535))

#define PS_MEMBARR_ELTTYPE(VERTCLAS, MEMBARR) std::remove_reference<decltype(VERTCLAS::MEMBARR[0])>::type
#define PS_VERT_SLOT(VARNAME, VERTCLAS, MEMBARR, MEMTYPE)																									\
	static_assert(std::rank<decltype(VERTCLAS::MEMBARR)>::value == 1,                  "not array [MEMBARR]");												\
	static_assert(std::is_same<MEMTYPE, PS_MEMBARR_ELTTYPE(VERTCLAS, MEMBARR)>::value, "member type mismatch [MEMTYPE]");									\
	GxVertSlot VARNAME { offsetof(VERTCLAS, MEMBARR), sizeof(PS_MEMBARR_ELTTYPE(VERTCLAS, MEMBARR)), std::extent<decltype(VERTCLAS::MEMBARR)>::value }

const float PS_PI = 3.14159265358979323846;

// https://eigen.tuxfamily.org/dox/group__TutorialGeometry.html
//   If you are working with OpenGL 4x4 matrices then Affine3f and Affine3d are what you want.
//   Since Eigen defaults to column-major storage, you can directly use the Transform::data() method to pass your transformation matrix to OpenGL.
// GL_UNIFORM_BUFFER bindings are NOT VAO STATE
// https://www.khronos.org/opengl/wiki/Sync_Object#Flushing_and_contexts
//   about GLsync (Fence Syncs) and flushing
//   (GLspec46 @ 4.1.2 Signaling)
// ::boost::property_tree::json_parser::write_json(std::cout, ptree, true);
// round(x * 3) / 3

namespace ei = ::Eigen;

template<typename T>
using sp = ::std::shared_ptr<T>;

using st = size_t;

using weit_t = std::tuple<std::string, float>;

using A3f = ::ei::Transform<float, 3, ei::Affine, ei::DontAlign>;
using M4f = ::ei::Matrix<float, 4, 4, ei::DontAlign>;
using Mp4f = ::ei::Map<::ei::Matrix<float, 4, 4, ei::DontAlign> >;
using V3f = ::ei::Matrix<float, 3, 1, ei::DontAlign>;

typedef std::function<void(uint8_t *, const std::string &)> fconv_t;

enum PaLvl
{
	PALVL_SAME = 1,
	PALVL_NEXT = 2,
};

class PaExc : public std::runtime_error
{
public:
	inline PaExc() : std::runtime_error("PaExc") {}
};

class PaBone
{
public:
	inline PaBone(const std::string &name, const std::vector<float> &matx) :
		m_name(name),
		m_matx(matx)
	{}

	std::string m_name;
	std::vector<float> m_matx;
};

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
	std::vector<float> m_matx;
	std::vector<PaBone> m_bone;

	std::map<std::string, uint32_t> m_map_str;
	std::map<uint32_t, std::string> m_map_int;
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

	inline std::tuple<std::map<std::string, uint32_t>, std::map<uint32_t, std::string> >
	_bonemap(const pt_t &bone)
	{
		std::map<std::string, uint32_t> map_str;
		std::map<uint32_t, std::string> map_int;

		size_t idx = 0;
		map_str["NONE"] = -1;
		for (auto it = bone.ordered_begin(); it != bone.not_found(); ++it)
			map_str[it->first] = idx++;
		for (auto &[k, v] : map_str)
			map_int[v] = k;
		assert(1 + bone.size() == map_str.size());

		return std::make_tuple(std::move(map_str), std::move(map_int));
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
		const pt_t &bone = armt.get_child("bone");

		assert(uvla.size() == 1);
		assert(indx.size() == vert.size() && indx.size() == uvla.begin()->second.size());
		assert(indx.size() == bna.size() && indx.size() == bwt.size());
		std::vector<GxVert> v(indx.size());

		const auto &[bone_map_str, bone_map_int] = _bonemap(bone);
		auto f_bone_id = [&bone_map_str](uint8_t *p, const std::string &s) { *((uint16_t *)p) = bone_map_str.find(s)->second; };
		auto f_float = [](uint8_t *p, const std::string &s) { *((float *)p) = std::stof(s); };
		auto f_f2ui16 = [](uint8_t *p, const std::string &s) { *((uint16_t *)p) = PS_F2UI16(std::stof(s)); };

		PS_VERT_SLOT(slot_vert, GxVert, m_vert, float);
		PS_VERT_SLOT(slot_uv, GxVert, m_uv, uint16_t);
		_vecflatten(vert, f_float, slot_vert, v);
		_vecflatten(uvla.begin()->second, f_f2ui16, slot_uv, v);

		PS_VERT_SLOT(slot_weid, GxVert, m_weid, uint16_t);
		PS_VERT_SLOT(slot_wewt, GxVert, m_wewt, float);
		_vecflatten(bna, f_bone_id, slot_weid, v);
		_vecflatten(bwt, f_float, slot_wewt, v);

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
		sp<PaArmt> q(new PaArmt());
		q->m_name = armt_.begin()->first;
		q->m_matx = _vec(matx, 4*4);
		for (auto it = bone.ordered_begin(); it != bone.not_found(); ++it)
			q->m_bone.push_back(PaBone(it->first, _vec(it->second, 4*4)));
		std::tie(q->m_map_str, q->m_map_int) = _bonemap(bone);
		return q;
	}

	inline void
	pars()
	{
		sp<PaModl> modl(_modl(m_pt.get_child("modl"), m_pt.get_child("armt")));
		sp<PaArmt> armt(_armt(m_pt.get_child("armt")));
		m_modl = modl;
		m_armt = armt;
	}

	std::string m_s;
	pt_t m_pt;

	sp<PaModl> m_modl;
	sp<PaArmt> m_armt;
};

class GxModl
{
	sp<Pa> m_pa;
	GLuint m_vbo;

	GxModl(const sp<Pa> &pa) :
		m_pa(pa),
		m_vbo(0)
	{}

	~GxModl()
	{
		glDeleteBuffers(1, &m_vbo);
	}

	void pars()
	{
		glCreateBuffers(1, &m_vbo);
		glNamedBufferStorage(m_vbo, m_pa->m_modl->m_data.size() * sizeof m_pa->m_modl->m_data[0], m_pa->m_modl->m_data.data(), GL_MAP_WRITE_BIT);
	}
};

void _perspective(M4f &m, float left, float right, float bottom, float top, float _near, float _far)
{
	float A = (right + left) / (right - left);
	float B = (top + bottom) / (top - bottom);
	float C = - (_far + _near) / (_far - _near);
	float D = - (2 * _far * _near) / (_far - _near);
	float X = (2 * _near) / (right - left);
	float Y = (2 * _near) / (top - bottom);
	m << X, 0, A, 0,
		 0, Y, B, 0,
		 0, 0, C, D,
		 0, 0, -1, 0;
}

void _lookat(M4f &m, const V3f &eye, const V3f &cen, const V3f &up)
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
	m = q * trns.matrix();
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
out vec3 bary;
layout(binding = 0, std140) uniform Ubo0
{
	vec4 colr;
	mat4 proj;
	mat4 view;
} ubo0;
void main()
{
	bary = vec3(mod(gl_VertexID - 0, 3) == 0, mod(gl_VertexID - 1, 3) == 0, mod(gl_VertexID - 2, 3) == 0);
	gl_Position = ubo0.proj * ubo0.view * vec4(vert.xyz, 1);
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
	horz_rot.rotate(ei::AngleAxisf(0.01 * PS_PI, V3f::UnitY()));

	M4f proj;
	_perspective(proj, -1, 1, -1, 1, 1, 10);

	V3f eye_pt(0, 0, 5);

	GLuint vao = 0;
	std::vector<GLuint> vbo(2);

	struct UColr {
		float colr[4] = { 0, 0, 1, 1 };
		float proj[16] = {};
		float view[16] = {};
	} colr;

	GLsync sync = 0;

	glCreateBuffers(vbo.size(), vbo.data());
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

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, vbo[1]);
	glUniformBlockBinding(sha.getNativeHandle(), 0, 0);

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
			M4f view;
			_lookat(view, eye_pt, V3f(0, 0, 0), V3f(0, 1, 0));

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
			glDrawArrays(GL_TRIANGLES, 0, pars->m_modl->m_data.size());
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
