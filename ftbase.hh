#ifndef _FTECH_BASE_H_
#define _FTECH_BASE_H_

#include <cstdio>
#include <cstdlib>

#include <SDL3/SDL.h>

//-----------------------------------------------------------------------------
// Core types
//-----------------------------------------------------------------------------

using u8  = Uint8;
using u16 = Uint16;
using u32 = Uint32;
using u64 = Uint64;

using f32 = float;

using usize = size_t;

//-----------------------------------------------------------------------------
// Core type helpers
//-----------------------------------------------------------------------------

template <typename T, usize N>
constexpr usize ArrLen(const T (&)[N])
{
  return N;
}

template <typename T>
static inline bool InRange(T x, T xmin, T xmax)
{
  return x >= xmin && x <= xmax;
}

// Align to next 4-byte boundary
template <typename T>
static inline T Align4(T x)
{
  return (x + 3) & ~3;
}

// Align to next 8-byte boundary
template <typename T>
static inline T Align8(T x)
{
  const T rem = x % 8;
  if (rem != 0) {
    return x + 8 - rem;
  } else {
    return x;
  }
}

template <typename T>
static inline T Min(T x1, T x2)
{
  if (x1 > x2) {
    return x2;
  } else {
    return x1;
  }
}

template <typename T>
static inline T Max(T x1, T x2)
{
  if (x1 > x2) {
    return x1;
  } else {
    return x2;
  }
}

//-----------------------------------------------------------------------------
// Defer macro via C++ nightmare world RAII shenanigans
//-----------------------------------------------------------------------------

#define DEFER_CONCAT_(x, y) x ## y
#define DEFER_CONCAT(x, y) DEFER_CONCAT_(x, y)

template <typename T>
struct DeferHelper
{
  T func;

  DeferHelper(T func) : func(func) { }
  ~DeferHelper() { func(); }

  DeferHelper(const DeferHelper&)            = delete;
  DeferHelper& operator=(const DeferHelper&) = delete;
};

#define defer const DeferHelper DEFER_CONCAT(_defer_, __LINE__) = [&]()

//-----------------------------------------------------------------------------
// Memory allocation helpers
//-----------------------------------------------------------------------------

template <typename T>
static inline T* MemAlloc(usize count = 1)
{
  T* mem = (T*)SDL_malloc(sizeof(T) * count);
  SDL_assert(mem && "allocation failed");
  return mem;
}

template <typename T>
static inline T* MemAllocZ(usize count = 1)
{
  T* mem = (T*)SDL_calloc(count, sizeof(T));
  SDL_assert(mem && "allocation failed");
  return mem;
}

template <typename T>
static inline void MemFree(T* mem)
{
  SDL_free(mem);
}

//-----------------------------------------------------------------------------
// Span
//-----------------------------------------------------------------------------

template <typename T>
struct Span
{
public:
  T*    buf;
  usize len;
public:
  Span()
    : buf(NULL), len(0)
  {
  }

  Span(T* buf, usize len)
    : buf(buf), len(len)
  {
  }

  template <usize N>
  Span(T (&arr)[N])
    : buf(arr), len(N)
  {
  }

  inline T& Get(usize idx)
  {
    SDL_assert_paranoid(buf && idx < len);
    return buf[idx];
  }

  inline T& operator[](usize idx)
  {
    return Get(idx);
  }

  inline T* Begin()
  {
    return buf;
  }

  inline T* End()
  {
    return buf + len;
  }
};

//-----------------------------------------------------------------------------
// String helpers
//-----------------------------------------------------------------------------

bool WildcardMatch(const char* pattern, const char* str);
bool IsValidUTF8(Span<u8> data);

//-----------------------------------------------------------------------------
// Path helpers
//-----------------------------------------------------------------------------

#define GOS_MAX_PATH 1024

const char* ExpandPath(const char* path);
const char* Extension(const char* path);

#endif // _FTECH_BASE_H_
