#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/regex.hpp>

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

class Bone
{
public:
	std::string m_name;
	std::vector<float> m_matx;
};

class UvLa
{
public:
	std::string m_name;
	std::vector<float> m_layr;
};

class PaModl
{
public:
	std::string m_name;
	std::vector<float> m_vert;
	std::vector<uint32_t> m_indx;
	std::vector<UvLa> m_uvla;
	std::vector<weit_t> m_weit;
};

class PaArmt
{
public:
	std::vector<float> m_matx;
	std::vector<Bone> m_bone;
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
		for (auto it = node.ordered_begin(); it != node.not_found(); ++it) {
			assert(it->second.size() == vecsizehint);
			for (auto it2 = it->second.ordered_begin(); it2 != it->second.not_found(); ++it2)
				v.push_back(std::stof(it2->second.data()));
		}
		return v;
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
		q->m_vert = _vec(vert, 3);
		for (auto it = indx.ordered_begin(); it != indx.not_found(); ++it)
			q->m_indx.push_back(std::stol(it->second.data()));
		for (auto it = uvla.ordered_begin(); it != uvla.not_found(); ++it) {
			UvLa uvla;
			uvla.m_name = it->first;
			uvla.m_layr = _vec(it->second, 2);
			q->m_uvla.push_back(std::move(uvla));
		}
		for (auto it = weit.ordered_begin(); it != weit.not_found(); ++it) {
			assert(it->second.size() % 2 == 0);
			for (auto it2 = it->second.ordered_begin(); it2 != it->second.not_found(); ++it2) {
				auto it2_ = it2++;
				q->m_weit.push_back(weit_t(it2_->second.data(), std::stof(it2->second.data())));
			}
			for (size_t i = it->second.size() / 2; i < 4; i++)
				q->m_weit.push_back(weit_t("NONE", 0.0f));
		}
		return q;
	}

	inline void
	pars()
	{
		sp<PaModl> modl(_modl(m_pt.get_child("modl")));
	}

	std::string m_s;
	pt_t m_pt;
};

int main(int argc, char **argv)
{
	sp<Pa> pars(new Pa(std::string((char *)g_ps_b1, g_ps_b1_size)));
	pars->pars();
	return EXIT_SUCCESS;
}
