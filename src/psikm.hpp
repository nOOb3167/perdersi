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
ikdraw1(sf::RenderWindow &win, const A2f &acum, const sf::Color &colr0 = sf::Color(255, 0, 0), const sf::Color &colr1 = sf::Color(0, 0, 255))
{
	const V2f ls(.0f, .0f), le(200.f, .0f);
	const V2f ls_(acum * ls);
	const V2f le_(acum * le);
	sf::Vertex v[2];
	v[0].color = colr0;
	v[1].color = colr0;
	v[0].position = sf::Vector2f(ls_.x(), ls_.y());
	v[1].position = sf::Vector2f(le_.x(), le_.y());
	win.draw(v, 2, sf::Lines);
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
