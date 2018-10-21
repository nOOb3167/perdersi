#include <cstdlib>>

#include <boost/asio.hpp>
#include <git2.h>

int main(int argc, char **argv)
{
  git_libgit2_init();
  
  return EXIT_SUCCESS;
}
