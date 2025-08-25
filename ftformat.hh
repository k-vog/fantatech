#ifndef _FTECH_FORMAT_H_
#define _FTECH_FORMAT_H_

#include "ftbase.hh"

//-----------------------------------------------------------------------------
// BP2/BP3 files
//-----------------------------------------------------------------------------

struct Bitmap
{
  SDL_Palette* pal;
  SDL_Surface* surf;
  SDL_Texture* tex;
};

// Load 1997 bitmap
bool LoadBP2(Bitmap* bmp, SDL_IOStream* src);

// Load 2006 bitmap
bool LoadBP3(Bitmap* bmp, SDL_IOStream* src);

//-----------------------------------------------------------------------------
// TXT files
//-----------------------------------------------------------------------------

// Load 1997 text as UTF-8
char* DecodeTXT_1997(SDL_IOStream* io);

// Load 2006 text as UTF-8
char* DecodeTXT_2006(SDL_IOStream* io, usize len = 0);

//-----------------------------------------------------------------------------
// BIN/LB5 files
//-----------------------------------------------------------------------------

struct PackEntry
{
  u32   off;
  u32   len;
  char* name;
};

struct PackFile
{
  Span<PackEntry> entries;
  SDL_IOStream*   lump_file;

  void  Close();
  void* ReadEntry(const PackEntry* entry);
};

bool OpenPackFile(PackFile* pack, const char* path);

#endif // _FTECH_FORMAT_H_