#ifndef _PSIKM_HPP_
#define _PSIKM_HPP_

#include <map>
#include <vector>

#include <Eigen/Dense>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Window.hpp>

const double PS_PI = 3.14159265358979323846;

namespace ei = ::Eigen;

using A2f = ::ei::Transform<float, 2, ei::Affine, ei::DontAlign>;
using A3f = ::ei::Transform<float, 3, ei::Affine, ei::DontAlign>;
using M4f = ::ei::Matrix<float, 4, 4, ei::DontAlign>;
using Mp4f = ::ei::Map<::ei::Matrix<float, 4, 4, ei::DontAlign> >;
using V2f = ::ei::Matrix<float, 2, 1, ei::DontAlign>;
using V3f = ::ei::Matrix<float, 3, 1, ei::DontAlign>;
using V4f = ::ei::Matrix<float, 4, 1, ei::DontAlign>;
using Qf = ::ei::Quaternion<float, ei::DontAlign>;

class IkmBone
{
public:
	inline IkmBone(float x, float y, float angl, float refp) :
		m_d(x, y, angl, refp)
	{
		_refresh();
	}

	inline void
	_refresh()
	{
		A2f r(A2f::Identity());
		r.prerotate(ei::Rotation2Df(m_d.z()));
		r.pretranslate(V2f(m_d.x(), m_d.y()));
		m_m = r;
	}

	/** x y angle reflectp */
	V4f m_d;
	A2f m_m;
};

class IkmChin
{
public:
	std::vector<IkmBone> m_chin;
};

inline void
ikdraw1(sf::RenderWindow &win, const A2f &acum, const float &len = 20.f, const sf::Color & colr0 = sf::Color(255, 0, 0), const sf::Color & colr1 = sf::Color(0, 255, 0))
{
	const sf::Vector2u s(win.getSize());
	const V2f ls_(.0f, .0f), le1_(len, .0f), le2_(.0f, len);
	const V2f ls(acum * ls_), le1(acum * le1_), le2(acum * le2_);
	sf::Vertex v[4];
	v[0].color = colr0;
	v[1].color = colr0;
	v[2].color = colr1;
	v[3].color = colr1;
	v[0].position = sf::Vector2f(ls.x(), s.y - ls.y());
	v[1].position = sf::Vector2f(le1.x(), s.y - le1.y());
	v[2].position = sf::Vector2f(ls.x(), s.y - ls.y());
	v[3].position = sf::Vector2f(le2.x(), s.y - le2.y());
	win.draw(v, sizeof v / sizeof *v, sf::Lines);
}

inline void
ikdraw(sf::RenderWindow &win, const IkmChin &chin)
{
	A2f a(A2f::Identity());
	for (size_t i = 0; i < chin.m_chin.size(); i++) {
		a = a * chin.m_chin[i].m_m;
		ikdraw1(win, a);
	}
}

inline void
ikmex(sf::RenderWindow &win)
{
	IkmChin chin;
	chin.m_chin.push_back(IkmBone(0, 0, PS_PI / 4, 1));
	chin.m_chin.push_back(IkmBone(200, 0, PS_PI / 2, 1));
	ikdraw(win, chin);
}

#endif /* _PSIKM_HPP_ */
