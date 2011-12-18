#ifndef _ASSERT_H_INCLUDED_
#define _ASSERT_H_INCLUDED_

// A bunch of utilities

#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>

#define Assert(expr, msg) do {			\
	if (!(expr)) {											\
		std::cerr << msg << std::endl;	\
		assert(expr);										\
	}																	\
} while (0);


class IOException : public std::runtime_error
{
	public:
	IOException() : std::runtime_error("I/O Exception") {}
};

#endif
