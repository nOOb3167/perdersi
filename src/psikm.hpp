#ifndef _PSIKM_HPP_
#define _PSIKM_HPP_

#include <pch1.h>

#include <Eigen/Dense>
#include <nlopt.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Window.hpp>

const double PS_PI = 3.14159265358979323846;
const float PS_IK_INCR = (float)(PS_PI) * 2.f / 16.f;

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

	inline void
	_state_from(const size_t cnt_old, const size_t cnt_cur)
	{
		const size_t fnum = m_chin.size();
		// TODO: if last ikmbone is rotation only we can skip it (j < fnum - 1)
		for (size_t j = 0; j < (fnum - 1); j++) {
			const size_t xold = (cnt_old >> (j * 4)) & 0x0F;
			const size_t xnew = (cnt_cur >> (j * 4)) & 0x0F;
			if (xold == xnew)
				continue;
			m_chin[j].m_d.z() = PS_IK_INCR * xnew;
			m_chin[j]._refresh();
		}
	}

	inline void
	_state_from_x(size_t n, const double *x)
	{
		// TODO: if last ikmbone is rotation only we can skip it (j < fnum - 1)
		assert(n == m_chin.size() - 1);
		for (size_t j = 0; j < n; j++) {
			(m_chin.data() + j)->m_d.z() = x[j];
			(m_chin.data() + j)->_refresh();
		}
	}
};

inline V2f
iktip(const IkmChin &chin)
{
	A2f a(A2f::Identity());
	for (size_t i = 0; i < chin.m_chin.size(); i++)
		a = a * chin.m_chin[i].m_m;
	return a * V2f(0, 0);
}

inline IkmChin
iktrgt(const IkmChin &chin_, const V2f &trgt)
{
	IkmChin chin(chin_);
	float dif_min = std::numeric_limits<float>::infinity();
	size_t cnt_min = 0;

	assert(((chin.m_chin.size() - 1) * 4) < 32);
	// TODO: if last ikmbone is rotation only we can skip it (j < fnum - 1)
	const size_t cnt_num = 1 << (4 * (chin.m_chin.size() - 1));
	size_t cnt_old = 0;

	for (size_t i = 0; i < cnt_num; (cnt_old = i, i++)) {
		chin._state_from(cnt_old, i);
		const V2f btip(iktip(chin));
		const float dif((trgt - btip).norm());
		if (dif < dif_min) {
			cnt_min = i;
			dif_min = dif;
		}
	}

	chin._state_from(~cnt_min, cnt_min);

	return chin;
}

class IkmNloptCtx
{
public:
	IkmChin *m_chin;
	V2f m_trgt;
};

inline double
ik_nlopt_func(unsigned n, const double *x, double *gradient, void *func_data)
{
	IkmNloptCtx *ctx = static_cast<IkmNloptCtx *>(func_data);
	// TODO: if last ikmbone is rotation only we can skip it (j < fnum - 1)
	assert(n == ctx->m_chin->m_chin.size() - 1 && !gradient);
	ctx->m_chin->_state_from_x(n, x);
	const double r = (ctx->m_trgt - iktip(*ctx->m_chin)).norm();
	return r;
}

inline IkmChin
iktrgt2(const IkmChin &chin_, const V2f &trgt)
{
	IkmChin chin(chin_);
	// TODO: if last ikmbone is rotation only we can skip it (j < fnum - 1)
	nlopt::opt opt(nlopt::LN_COBYLA, chin.m_chin.size() - 1);
	const std::vector<double> lb(chin.m_chin.size() - 1, .0);
	const std::vector<double> ub(chin.m_chin.size() - 1, 2. * PS_PI);
	opt.set_lower_bounds(lb);
	opt.set_upper_bounds(ub);
	IkmNloptCtx ctx; ctx.m_chin = &chin; ctx.m_trgt = trgt;
	opt.set_min_objective(ik_nlopt_func, &ctx);
	opt.set_maxeval(256);
	std::vector<double> ig(chin.m_chin.size() - 1, .0);
	double rval = .0;
	nlopt::result res = opt.optimize(ig, rval);
	chin._state_from_x(ig.size(), ig.data());
	return chin;
}

inline void
ikdrawline(sf::RenderWindow &win, const V2f &ls, const V2f &le, const float &len, const sf::Color &colr)
{
	const sf::Vector2u &s(win.getSize());
	sf::Vertex v[2];
	v[0].color = colr;
	v[1].color = colr;
	v[0].position = sf::Vector2f(ls.x(), s.y - ls.y());
	v[1].position = sf::Vector2f(le.x(), s.y - le.y());
	win.draw(v, sizeof v / sizeof * v, sf::Lines);
}

inline void
ikdraw1(sf::RenderWindow &win, const A2f &acum, const float &len = 20.f, const sf::Color &colr0 = sf::Color(255, 0, 0), const sf::Color &colr1 = sf::Color(0, 255, 0))
{
	const float &le_ = len / 7.f;
	const V2f ls_(.0f, .0f), le1_(len, .0f), le2_(.0f, len);
	const V2f ls(acum * ls_), le1(acum * le1_), le2(acum * le2_);
	ikdrawline(win, ls, le1, len, colr0);
	ikdrawline(win, ls, le2, len, colr1);
}

inline void
ikdraw(sf::RenderWindow &win, const IkmChin &chin)
{
	A2f a(A2f::Identity());
	for (size_t i = 0; i < chin.m_chin.size(); i++) {
		const V2f &old_(a * V2f(0, 0));
		a = a * chin.m_chin[i].m_m;
		const V2f &new_(a * V2f(0, 0));
		ikdrawline(win, old_, new_, (old_ - new_).norm(), sf::Color(0, 128, 255));
		ikdraw1(win, a);
	}
}

inline void
ikmex(sf::RenderWindow &win)
{
	IkmChin chin;
	chin.m_chin.push_back(IkmBone(200, 0, PS_PI / 4, 1));
	chin.m_chin.push_back(IkmBone(200, 0, PS_PI / 2, 1));
	chin.m_chin.push_back(IkmBone(50, 0, 0, 1));
	ikdraw(win, chin);
	auto &v2i = sf::Mouse::getPosition(win);
	auto &v2u = win.getSize();
	ikdraw(win, iktrgt2(chin, V2f(v2i.x, v2u.y - v2i.y)));
}

#endif /* _PSIKM_HPP_ */
