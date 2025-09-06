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

#elif defined(__GNUC__)

#define FORMAT_ATTR(fmt, args) __attribute__((format(printf, fmt, args)))
#define RET_NONNULL_ATTR __attribute__((returns_nonnull))
#define NORETURN_ATTR __attribute__((noreturn))

#else

#define FORMAT_ATTR(fmt, args)
#define RET_NONNULL_ATTR
#define NORETURN_ATTR

#endif
