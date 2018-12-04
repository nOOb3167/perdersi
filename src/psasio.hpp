#ifndef _PSASIO_H_
#define _PSASIO_H_

// https ://stackoverflow.com/questions/9750344/boostasio-winsock-and-winsock-2-compatibility-issue
// always asio before windows

#include <boost/asio.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#endif /* _PSASIO_H_ */
