#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
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

const float PS_PI = 3.14159265358979323846;

// https://eigen.tuxfamily.org/dox/group__TutorialGeometry.html
//   If you are working with OpenGL 4x4 matrices then Affine3f and Affine3d are what you want.
//   Since Eigen defaults to column-major storage, you can directly use the Transform::data() method to pass your transformation matrix to OpenGL.
// GL_UNIFORM_BUFFER bindings are NOT VAO STATE

namespace ei = ::Eigen;

template<typename T>
using sp = ::std::shared_ptr<T>;

using st = size_t;

using weit_t = std::tuple<std::string, float>;

using A3f = ::ei::Transform<float, 3, ei::Affine, ei::DontAlign>;
using M4f = ::ei::Matrix<float, 4, 4, ei::DontAlign>;
using Mp4f = ::ei::Map<::ei::Matrix<float, 4, 4, ei::DontAlign> >;
using V3f = ::ei::Matrix<float, 3, 1, ei::DontAlign>;

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

class PaModl
{
public:
	std::string m_name;
	std::vector<float> m_vert;
	std::vector<uint32_t> m_indx;
	std::vector<PaUvLa> m_uvla;
	std::vector<weit_t> m_weit;
};

class PaArmt
{
public:
	std::string m_name;
	std::vector<float> m_matx;
	std::vector<PaBone> m_bone;
};

class PaXtra
{
public:
	std::map<std::string, uint32_t> m_map_str;
	std::map<uint32_t, std::string> m_map_int;

	std::vector<uint32_t> m_id;
	std::vector<float> m_wt;
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

	inline std::vector<float>
	_vecflatten(const pt_t &node, size_t vecsizehint)
	{
		std::vector<float> v;
		for (auto it = node.ordered_begin(); it != node.not_found(); ++it)
			for (const auto &x : _vec(it->second, vecsizehint))
				v.push_back(x);
		return v;
	}

	weit_t
	_iterweit(pt_t::const_assoc_iterator &it)
	{
		std::string a = (it++)->second.data();
		float b = std::stof((it++)->second.data());
		return weit_t(a, b);
	}

	inline void
	_bone_name_map(const PaArmt &armt, std::map<std::string, uint32_t> &map_str, std::map<uint32_t, std::string> &map_int)
	{
		size_t idx = 0;
		map_str["NONE"] = -1;
		for (const PaBone &b : armt.m_bone)
			map_str[b.m_name] = idx++;
		for (auto &[k, v] : map_str)
			map_int[v] = k;
		assert(1 + armt.m_bone.size() == map_str.size());
	}

	inline sp<PaModl>
	_modl(const pt_t &modl_)
	{
		assert(modl_.size() == 1);
		const pt_t &modl = modl_.begin()->second;
		const pt_t &vert = modl.get_child("vert");
		const pt_t &indx = modl.get_child("indx");
		const pt_t &uvla = modl.get_child("uvla");
		const pt_t &weit = modl.get_child("weit");
		sp<PaModl> q(new PaModl());
		q->m_name = modl_.begin()->first;
		q->m_vert = _vecflatten(vert, 3);
		for (auto it = indx.ordered_begin(); it != indx.not_found(); ++it)
			q->m_indx.push_back(std::stol(it->second.data()));
		for (auto it = uvla.ordered_begin(); it != uvla.not_found(); ++it)
			q->m_uvla.push_back(PaUvLa(it->first, _vecflatten(it->second, 2)));
		for (auto it = weit.ordered_begin(); it != weit.not_found(); ++it) {
			assert(it->second.size() % 2 == 0);
			for (auto it2 = it->second.ordered_begin(); it2 != it->second.not_found(); /*dummy*/)
				q->m_weit.push_back(_iterweit(it2));
			for (size_t i = it->second.size() / 2; i < 4; i++)
				q->m_weit.push_back(weit_t("NONE", 0.0f));
		}
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
		return q;
	}

	inline sp<PaXtra>
	_xtra(const PaModl &modl, const PaArmt &armt)
	{
		sp<PaXtra> q(new PaXtra());
		size_t idx = 0;
		
		q->m_map_str["NONE"] = -1;
		for (const PaBone &b : armt.m_bone)
			q->m_map_str[b.m_name] = idx++;
		for (auto &[k, v] : q->m_map_str)
			q->m_map_int[v] = k;
		assert(1 + armt.m_bone.size() == q->m_map_str.size());

		for (const weit_t &w : modl.m_weit) {
			q->m_id.push_back(q->m_map_str[std::get<0>(w)]);
			q->m_wt.push_back(std::get<1>(w));
		}

		return q;
	}

	inline void
	pars()
	{
		sp<PaModl> modl(_modl(m_pt.get_child("modl")));
		sp<PaArmt> armt(_armt(m_pt.get_child("armt")));
		sp<PaXtra> xtra(_xtra(*modl, *armt));
		m_modl = modl;
		m_armt = armt;
		m_xtra = xtra;
	}

	std::string m_s;
	pt_t m_pt;

	sp<PaModl> m_modl;
	sp<PaArmt> m_armt;
	sp<PaXtra> m_xtra;
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
out vec3 bary;
layout(binding = 0, std140) uniform Ubo0
{
	vec4 colr;
	mat4 proj;
	mat4 view;
} ubo0;
void main()
{
	bary = vec3((gl_VertexID + 0) % 3 == 0, (gl_VertexID - 1) % 3 == 0, (gl_VertexID - 2) % 3 == 0);
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

	glCreateBuffers(vbo.size(), vbo.data());
	glNamedBufferData(vbo[0], pars->m_modl->m_vert.size() * sizeof(float), pars->m_modl->m_vert.data(), GL_STATIC_DRAW);
	glNamedBufferData(vbo[1], sizeof colr, &colr, GL_STATIC_DRAW);

	glCreateVertexArrays(1, &vao);
	glEnableVertexArrayAttrib(vao, 0);
	glVertexArrayVertexBuffer(vao, 0, vbo[0], 0, 3 * sizeof(float));
	glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao, 0, 0);

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

			glClearColor(1, 1, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			glEnable(GL_DEPTH_TEST);

			// https://github.com/SFML/SFML/blob/master/src/SFML/Graphics/RenderTarget.cpp#L482
			//   RenderTarget::resetGLStates()
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);

			Mp4f(colr.proj) = proj;
			Mp4f(colr.view) = view;
			glNamedBufferData(vbo[1], sizeof colr, &colr, GL_STATIC_DRAW);

			sf::Shader::bind(&sha);
			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLES, 0, pars->m_modl->m_indx.size());
			glBindVertexArray(0);
			sf::Shader::bind(nullptr);

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
