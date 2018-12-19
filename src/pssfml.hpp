#ifndef _PSSFML_HPP_
#define _PSSFML_HPP_

#include <cassert>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <psdata.hpp>
#include <psmisc.hpp>
#include <psthr.hpp>

#define PS_TEX_MKTEX(VARNAME) (m_tex[#VARNAME] = SfTex::_mktex((VARNAME), VARNAME ## _size))

typedef ::std::chrono::steady_clock::time_point pstimept_t;

namespace ps
{

class SfTex
{
public:
	SfTex() :
		m_tex()
	{
		PS_TEX_MKTEX(g_ps_data_test00);
	}

	static sp<sf::Texture> _mktex(uint8_t *data, size_t size)
	{
		sp<sf::Texture> tex(new sf::Texture());
		tex->loadFromMemory(data, size);
		return tex;
	}

	void draw(sf::RenderWindow &window, const std::string &name, const sf::Vector2f &pos = sf::Vector2f(0, 0), const sf::Vector2f &siz = sf::Vector2f(-1.0f, -1.0f))
	{
		assert(m_tex.find(name) != m_tex.end());
		sf::Sprite spr(*m_tex[name]);
		spr.move(pos);
		spr.setScale(siz.x == -1.0f && siz.y == -1.0f ? sf::Vector2f(1.0f, 1.0f) : sf::Vector2f(siz.x / m_tex[name]->getSize().x, siz.y / m_tex[name]->getSize().y));
		window.draw(spr);
	}

	std::map<std::string, sp<sf::Texture> > m_tex;
};

class SfWin
{
public:
	SfWin(const sp<Thr> &thr) :
		m_start(std::chrono::steady_clock::now()),
		m_win(sf::VideoMode(800, 600), "perder.si"),
		m_tex(),
		m_thr(thr)
	{
		m_win.setFramerateLimit(60);
	}

	void run()
	{
		while (m_win.isOpen()) {
			sf::Event event;
			while (m_win.pollEvent(event)) {
				if (event.type == sf::Event::Closed)
					m_win.close();
			}
			if (m_thr->isdead())
				m_win.close();
			m_win.clear(sf::Color(255, 255, 0));
			m_tex.draw(m_win, "g_ps_data_test00", { 0, 0 }, { 256, 256 });
			m_win.display();
		}
	}

	pstimept_t m_start;
	sf::RenderWindow m_win;
	SfTex m_tex;
	sp<Thr> m_thr;
};

}

#endif /* _PSSFML_HPP_ */
