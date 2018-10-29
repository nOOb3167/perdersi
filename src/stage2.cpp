#include <cstdlib>

#include <boost/filesystem.hpp>
#include <SFML/Window.hpp>

int
main(int argc, char **)
{
	sf::Window window(sf::VideoMode(800, 600), "perder.si");

	boost::filesystem::current_path();

	return EXIT_SUCCESS;
}
