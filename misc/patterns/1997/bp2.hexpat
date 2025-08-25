// BP2 files from the original 1997 game

import std.sys;

struct BP2_Header
{
  // 999
  u32 magic; std::assert(magic == 999, "magic");
  // Image data encoding method
  // 1: INDEX8 RLE 8-tall columns (palette)
  // 2: RGB24 RLE 8-tall columns
  // 3: R8 RLE 8-tall columns (expand to gray)
  u32 encoding;
  // Palette data
  u32 palette_len;
  // No clue what this is for
  u32 idk;
  // Assuming encoding method 2:
  // Number of 8-tall image data slices
  u32 slice_count;
  // Remaining uncompressed image data if image height isn't divisible by 8
  u32 extra_slice_count;
};

// ref: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader
struct BITMAPFILEHEADER
{
  u8  bfType[2];
  u32 bfSize;
  u16 bfReserved1;
  u16 bfReserved2;
  u32 bfOffBits;
};

// Every .BP2 file in the game is V1 (biSize = 0x28 (40))
// ref: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader
struct BITMAPINFOHEADER
{
  u32 biSize;
  u32 biWidth;
  u32 biHeight;
  u16 biPlanes;
  u16 biBitCount;
  u32 biCompression;
  u32 biSizeImage;
  u32 biXPelsPerMeter;
  u32 biYPelsPerMeter;
  u32 biClrUsed;
  u32 biClrImportant;
};

struct BP2_Chunk
{
  u32 len;
  u8  buf[len];
};

struct BP2
{
  BP2_Header       bp2;
  BITMAPFILEHEADER bfi;
  BITMAPINFOHEADER bih;

  u8 palette[bp2.palette_len];

  BP2_Chunk slices[bp2.slice_count];

  if (bp2.extra_slice_count > 0) {
    BP2_Chunk extra_slice;
  }
};

BP2 bp2 @ 0x0;