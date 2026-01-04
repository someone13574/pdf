#pragma once

#if defined(__has_attribute)

#if __has_attribute(format)
#define FORMAT_ATTR(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define FORMAT_ATTR(fmt, args)
#endif

#if __has_attribute(returns_nonnull)
#define RET_NONNULL_ATTR __attribute__((returns_nonnull))
#else
#define RET_NONNULL_ATTR
#endif

#if __has_attribute(noreturn)
#define NORETURN_ATTR __attribute__((noreturn))
#else
#define NORETURN_ATTR
#endif

#if __has_attribute(malloc)
#define MALLOC_ATTR(size) __attribute__((malloc, alloc_size(size)))
#else
#define MALLOC_ATTR(size)
#endif

#if __has_attribute(malloc)
#define MALLOC_ALIGNED_ATTR(size, align)                                       \
    __attribute__((malloc, alloc_size(size), alloc_align(align)))
#else
#define MALLOC_ALIGNED_ATTR(size, align)
#endif

#elif defined(__GNUC__)

#define FORMAT_ATTR(fmt, args) __attribute__((format(printf, fmt, args)))
#define RET_NONNULL_ATTR __attribute__((returns_nonnull))
#define NORETURN_ATTR __attribute__((noreturn))
#define MALLOC_ATTR(size) __attribute__((malloc, size))
#define MALLOC_ALIGNED_ATTR(size, align)                                       \
    __attribute__((malloc, alloc_size(size), alloc_align(align)))

#else

#define FORMAT_ATTR(fmt, args)
#define RET_NONNULL_ATTR
#define NORETURN_ATTR
#define MALLOC_ALIGNED_ATTR(size, align)

#endif
