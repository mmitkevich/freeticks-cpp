#pragma once

#define FT_NO_TRACE

#ifndef FT_NO_TRACE
#define FT_TRACE(x) std::cerr << std::dec << x << std::endl;
#else
#define FT_TRACE(...)
#endif

#define FT_ASSERT assert

#define FT_NO_INLINE __attribute__((noinline)) 
#define FT_ALWAYS_INLINE __attribute__((always_inline))
#define FT_LIKELY(x) (x)
#define FT_UNLIKELY(x) (x)