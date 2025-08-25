#include "ftbase.hh"

//-----------------------------------------------------------------------------
// String helpers
//-----------------------------------------------------------------------------

bool WildcardMatch(const char* pattern, const char* str)
{
  // From notes, I can't remember where I originally got this
  const char *p = pattern;
  const char* s = str;
  const char* star = NULL;
  const char* ss = NULL;
  while (*s) {
    if (*p == '?' || *p == *s) {
      ++p;
      ++s;
    } else if (*p == '*') {
      star = p++;
      ss = s;
    } else if (star) {
      p = star + 1;
      s = ++ss;
    } else {
      return 0;
    }
  }
  while (*p == '*') {
    ++p;
  }
  return !*p;
}

bool IsValidUTF8(Span<u8> data)
{
  usize i = 0;
  while (i < data.len) {
    u8 p0 = data[i++];

    // [00, 7F]
    if (p0 <= 0x7F) {
      continue;
    }

    // [C2, DF] [80, BF]
    if (InRange<u8>(p0, 0xC2, 0xDF)) {
      if (i >= data.len) {
        return false;
      }
      u8 p1 = data[i++];
      if (!InRange<u8>(p1, 0x80, 0xBF)) {
        return false;
      }
      continue;
    }

    // E0 [A0, BF] [80, BF]
    if (p0 == 0xE0) {
      if (i + 1 >= data.len) {
        return false;
      }
      u8 p1 = data[i++];
      u8 p2 = data[i++];
      if (!InRange<u8>(p1, 0xA0, 0xBF)) {
        return false;
      }
      if (!InRange<u8>(p2, 0x80, 0xBF)) {
        return false;
      }
      continue;
    }

    // [E1, EC] [80, BF] [80, BF]
    if (InRange<u8>(p0, 0xE1, 0xEC)) {
      if (i + 1 >= data.len) {
        return false;
      }
      u8 p1 = data[i++];
      u8 p2 = data[i++];
      if (!InRange<u8>(p1, 0x80, 0xBF)) {
        return false;
      }
      if (!InRange<u8>(p2, 0x80, 0xBF)) {
        return false;
      }
      continue;
    }

    // ED [80, 9F] [80, BF]
    if (p0 == 0xED) {
      if (i + 1 >= data.len) {
        return false;
      }
      u8 p1 = data[i++];
      u8 p2 = data[i++];
      if (!InRange<u8>(p1, 0x80, 0x9F)) {
        return false;
      }
      if (!InRange<u8>(p2, 0x80, 0xBF)) {
        return false;
      }
      continue;
    }

    // [EE, EF] [80, BF] [80, BF]
    if (InRange<u8>(p0, 0xEE, 0xEF)) {
      if (i + 1 >= data.len) {
        return false;
      }
      u8 p1 = data[i++];
      u8 p2 = data[i++];
      if (!InRange<u8>(p1, 0x80, 0xBF)) {
        return false;
      }
      if (!InRange<u8>(p2, 0x80, 0xBF)) {
        return false;
      }
      continue;
    }

    // F0 [90, BF] [80, BF] [80, BF]
    if (p0 == 0xF0) {
      if (i + 2 >= data.len) {
        return false;
      }
      u8 p1 = data[i++];
      u8 p2 = data[i++];
      u8 p3 = data[i++];
      if (!InRange<u8>(p1, 0x90, 0xBF)) {
        return false;
      }
      if (!InRange<u8>(p2, 0x80, 0xBF)) {
        return false;
      }
      if (!InRange<u8>(p3, 0x80, 0xBF)) {
        return false;
      }
      continue;
    }

    // [F1, F3] [80, BF] [80, BF] [80, BF]
    if (InRange<u8>(p0, 0xF1, 0xF3)) {
      if (i + 2 >= data.len) {
        return false;
      }
      u8 p1 = data[i++];
      u8 p2 = data[i++];
      u8 p3 = data[i++];
      if (!InRange<u8>(p1, 0x80, 0xBF)) {
        return false;
      }
      if (!InRange<u8>(p2, 0x80, 0xBF)) {
        return false;
      }
      if (!InRange<u8>(p3, 0x80, 0xBF)) {
        return false;
      }
      continue;
    }

    // F4 [80, 8F] [80, BF] [80, BF]
    if (p0 == 0xF4) {
      if (i + 2 >= data.len) {
        return false;
      }
      u8 p1 = data[i++];
      u8 p2 = data[i++];
      u8 p3 = data[i++];
      if (!InRange<u8>(p1, 0x80, 0x8F)) {
        return false;
      }
      if (!InRange<u8>(p2, 0x80, 0xBF)) {
        return false;
      }
      if (!InRange<u8>(p3, 0x80, 0xBF)) {
        return false;
      }
      continue;
    }

    // Otherwise invalid lead byte
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
// Path helpers
//-----------------------------------------------------------------------------

static inline bool IsDelim(char c)
{
#ifdef _WIN32
  if (c == '\\') {
    return true;
  }
#endif
  return c == '/';
}

const char* ExpandPath(const char* path)
{
  // @@: will probably need to expand more than just ~ at some point
  static const char* home = 0;
  if (!home) {
#ifndef _WIN32
    home = SDL_getenv("HOME");
#endif
    SDL_assert(home && "Failed to get user directory");
  }

  static char result[GOS_MAX_PATH] = { };
  if (path[0] == '~') {
    SDL_snprintf(result, ArrLen(result), "%s/%s", home, path + 1);
  } else {
    SDL_snprintf(result, ArrLen(result), "%s", path);
  }
  return result;
}

const char* Extension(const char* path)
{
  // very not robust
  const char* end_part = NULL;
  for (usize i = 0; path[i]; ++i) {
    if (path[i] == '.' && !IsDelim(path[i - 1])) {
      end_part = &path[i + 1];
    }
  }

  for (const char* p = end_part; p && *p; ++p) {
    if (IsDelim(*p)) {
      return NULL;
    }
  }

  return end_part;
}
