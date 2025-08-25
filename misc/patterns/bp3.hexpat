#include <std/mem.pat>
#include <std/sys.pat>

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

struct BP3_Header
{
  u32 magic;
  u32 w;
  u32 h;
  u32 size;
  BITMAPFILEHEADER bfh;
  BITMAPINFOHEADER bih;
};

enum BlockType : u8
{
  SOLID  = 0,
  BGR332 = 1,
  BGR233 = 2,
  BGR323 = 3,
  GRAY4  = 4,
  GRAY8  = 5,
  BGR555 = 6,
  BGR888 = 7,
};

// Helper: Align to 8-byte boundary
fn align8(u32 x)
{
  if ((x % 8) != 0) {
    return x + 8 - (x % 8);
  } else {
    return x;
  }
};

BP3_Header hdr @ 0x0;

u32 num_blocks = align8(hdr.w) * align8(hdr.h) / 64;

u8 mode_table[num_blocks] @ $;
u8 param_table[num_blocks * 3] @ $;

fn block_bpp(BlockType type)
{
  match (type) {
    (BlockType::SOLID):  return 0;
    (BlockType::BGR332): return 8;
    (BlockType::BGR233): return 8;
    (BlockType::BGR323): return 8;
    (BlockType::GRAY4):  return 4;
    (BlockType::GRAY8):  return 8;
    (BlockType::BGR555): return 16;
    (BlockType::BGR888): return 24;
  }
  std::assert(0, "invalid block type");
  return 0;
};

fn calc_tile_size(u32 idx)
{
  u32 result = 0;
  
  u32 tiles_per_row = align8(hdr.w) / 8;
  u32 chunk_w = 8;
  if ((i % tiles_per_row) * 8 + 8 >= hdr.w) {
    chunk_w = hdr.w + 8 - align8(hdr.w); 
  }
  u32 chunk_h = 8;
  if ((i / tiles_per_row) * 8 + 8 >= hdr.h) {
    chunk_h = hdr.h + 8 - align8(hdr.h);
  }
  u32 bpp = block_bpp(mode_table[i]);
  if (bpp > 0) {
    u32 stored_row_bytes = bpp * chunk_w / 8;
    for (u32 y = 0, y < chunk_h, y += 1) {
      result += stored_row_bytes;
    }
  }
  
  return result;
};

u32 i = 0;
u32 tiles_per_row = align8(hdr.w) / 8;
struct Tile
{
  u8 data[calc_tile_size(i)];
  i += 1;
};

Tile tiles[num_blocks] @ $;