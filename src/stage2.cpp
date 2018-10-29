#include <cstdlib>

#include <boost/filesystem.hpp>
#include <SFML/Window.hpp>

int
main(int argc, char **)
{
	sf::Window window(sf::VideoMode(800, 600), "perder.si");

	boost::filesystem::current_path();

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();
		}
	}

	return EXIT_SUCCESS;
}
