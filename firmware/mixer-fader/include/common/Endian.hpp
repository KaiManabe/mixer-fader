#ifndef _ENDIAN_HPP_
#define _ENDIAN_HPP_

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This firmware requires a little-endian target."
#endif
#elif defined(_WIN32)
// Windows targets are little-endian.
#else
#error "Cannot determine endianness. A little-endian target is required."
#endif


#endif