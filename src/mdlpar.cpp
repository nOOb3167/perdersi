#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

// GRRR
#include <psasio.hpp>

#include <boost/regex.hpp>
#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Window.hpp>

#include <pscruft.hpp>
#include <ps_b1.h>

#define N0 n0()

template<typename T>
using sp = ::std::shared_ptr<T>;

using st = size_t;

using weit_t = std::tuple<std::string, float>;

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

void stuff()
{
	sp<Pa> pars(new Pa(std::string((char *)g_ps_b1, g_ps_b1_size)));
	pars->pars();

	sf::RenderWindow win(sf::VideoMode(800, 600), "");

	if (glewInit() != GLEW_OK)
		throw PaExc();

	sf::Shader sha;

	std::string sha_vs = R"EOF(
#version 440
layout(location = 0) in vec3 vert;
vec3 loc[] = {
vec3(0,0,0),
vec3(1,0,0),
vec3(1,1,0)
};
void main()
{
	int b = gl_VertexID % 3;
	//gl_Position = vec4(loc[b], 1);
	gl_Position = vec4(vert.xyz * 0.3, 1);
}
)EOF";

	std::string sha_fs = R"EOF(
#version 440
layout(location = 0) out vec4 color;
void main()
{
	color = vec4(1,0,0,0);
}
)EOF";

	if (!sha.loadFromMemory(sha_vs, sha_fs))
		throw PaExc();

	GLuint vao = 0;
	std::vector<GLuint> vbo(1);

	glGenVertexArrays(1, &vao);
	glGenBuffers(vbo.size(), vbo.data());

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		{
			glBufferData(GL_ARRAY_BUFFER, pars->m_modl->m_vert.size() * sizeof(float), pars->m_modl->m_vert.data(), GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glBindVertexBuffer(0, vbo[0], 0, 3 * sizeof(float));
			glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
			glVertexAttribBinding(0, 0);
			//glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	glBindVertexArray(0);

	while (win.isOpen()) {
		sf::Event event;
		while (win.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				goto end;
		}
		//win.clear(sf::Color(255, 255, 0));
		{
			glClearColor(1, 1, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			// https://github.com/SFML/SFML/blob/master/src/SFML/Graphics/RenderTarget.cpp#L482
			//   RenderTarget::resetGLStates()
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);

			glBindVertexArray(vao);
			{
				sf::Shader::bind(&sha);
				glDrawArrays(GL_TRIANGLES, 0, pars->m_modl->m_indx.size());
				sf::Shader::bind(nullptr);
			}
			glBindVertexArray(0);

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
