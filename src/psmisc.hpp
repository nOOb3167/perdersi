#ifndef _PSMISC_HPP_
#define _PSMISC_HPP_

#include <memory>
#include <string>

template<typename T>
using sp = ::std::shared_ptr<T>;

template<typename T>
using up = ::std::unique_ptr<T>;

using shahex_t = ::std::string;

#endif /* _PSMISC_HPP_ */
