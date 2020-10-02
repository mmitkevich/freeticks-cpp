#pragma once

#define FT_NO_LOG

#ifndef FT_NO_LOG
#define FT_TRACE(x) std::cerr << std::dec << x << std::endl;
#else
#define FT_TRACE(...)
#endif

#define FT_ASSERT assert

namespace ft::utils {}