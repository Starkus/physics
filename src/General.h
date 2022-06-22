#if PREPROCESSING
@Ignore #include <stdint.h>
@Ignore #include <stdio.h>
#else
#include <stdint.h>
#include <stdio.h>
#endif

// We don't support any non IEEE754 arch for now.
#define ASSUME_IEEE754 1

#if defined(WIN32) || defined(_WIN32)
#define TARGET_WINDOWS 1
#else
#define TARGET_WINDOWS 0
#endif

#if !TARGET_WINDOWS
#if PREPROCESSING
@Ignore #include <assert.h>
#else
#include <assert.h>
#endif
#define MAX_PATH PATH_MAX
#endif

#if PREPROCESSING
@Ignore #define USING_TYPE_INFO 1
#endif

#define CODE_RELOAD 1

// I hate this programming language
#undef near
#undef far

#if DEBUG_BUILD
#define DEBUG_ONLY(...) __VA_ARGS__
#else
#define DEBUG_ONLY(...)
#endif

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#define U8_MAX 0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFF

#define S8_MIN ((s8)0xFF)
#define S16_MIN ((s16)0xFFFF)
#define S32_MIN ((s32)0xFFFFFFFF)
#define S64_MIN ((s64)0xFFFFFFFFFFFFFFFF)

#define S8_MAX ((s8)0x7F)
#define S16_MAX ((s16)0x7FFF)
#define S32_MAX ((s32)0x7FFFFFFF)
#define S64_MAX ((s64)0x7FFFFFFFFFFFFFFF)

#define CRASH do { *((int*)0) = 1; } while (false)

#if DEBUG_BUILD
#if TARGET_WINDOWS
#define ASSERT(expr) do { if (!(expr)) __debugbreak(); } while (false)
#else
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

#define NOMANGLE extern "C"

#define ArrayCount(array) (sizeof(array) / sizeof(array[0]))