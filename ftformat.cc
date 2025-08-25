#include "ftformat.hh"

extern "C" {
#include "thirdparty/thtk_cp932/cp932.h"
}

//-----------------------------------------------------------------------------
// BMP files
//-----------------------------------------------------------------------------

enum
{
  BP2_FMT_INDEX8 = 1,
  BP2_FMT_BGR888 = 2,
  BP2_FMT_GRAY8  = 3,
};

struct BMP_FileHeader
{
  u8  bfType[2];
  u32 bfSize;
  u16 bfReserved1;
  u16 bfReserved2;
  u32 bfOffBits;
};

struct BMP_InfoHeader
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

static bool LoadBMPFileHeader(BMP_FileHeader* bfh, SDL_IOStream* io)
{
  return
    SDL_ReadU8(io, &bfh->bfType[0]) &&
    SDL_ReadU8(io, &bfh->bfType[1]) &&
    SDL_ReadU32LE(io, &bfh->bfSize) &&
    SDL_ReadU16LE(io, &bfh->bfReserved1) &&
    SDL_ReadU16LE(io, &bfh->bfReserved2) &&
    SDL_ReadU32LE(io, &bfh->bfOffBits);
}

static bool LoadBMPInfoHeader(BMP_InfoHeader* bih, SDL_IOStream* io)
{
  return
    SDL_ReadU32LE(io, &bih->biSize) &&
    SDL_ReadU32LE(io, &bih->biWidth) &&
    SDL_ReadU32LE(io, &bih->biHeight) &&
    SDL_ReadU16LE(io, &bih->biPlanes) &&
    SDL_ReadU16LE(io, &bih->biBitCount) &&
    SDL_ReadU32LE(io, &bih->biCompression) &&
    SDL_ReadU32LE(io, &bih->biSizeImage) &&
    SDL_ReadU32LE(io, &bih->biXPelsPerMeter) &&
    SDL_ReadU32LE(io, &bih->biYPelsPerMeter) &&
    SDL_ReadU32LE(io, &bih->biClrUsed) &&
    SDL_ReadU32LE(io, &bih->biClrImportant);
}

//-----------------------------------------------------------------------------
// BP2 files
//-----------------------------------------------------------------------------

struct BP2
{
  u32 magic;
  u32 encoding;
  u32 palette_len;
  u32 idk;
  u32 slice_count;
  u32 extra_slice_count;
};

struct BP2Params
{
  BP2            bp2;
  BMP_FileHeader bfh;
  BMP_InfoHeader bih;
};

template <usize SRC_BPP, usize DST_BPP = SRC_BPP>
static bool BP2_DecodeRLE(SDL_Surface* surf, SDL_IOStream* io, BP2Params* bp2)
{
  static_assert(DST_BPP >= SRC_BPP);

  const usize dst_pitch = Align4(bp2->bih.biWidth * DST_BPP);
  u8* slice = MemAlloc<u8>(dst_pitch * 8);
  defer { MemFree(slice); };

  for (u32 i = 0; i < bp2->bp2.slice_count; ++i) {
    u32 chunk_len = 0;
    if (!SDL_ReadU32LE(io, &chunk_len)) {
      return false;
    }
    u8* chunk = MemAlloc<u8>(chunk_len);
    defer { MemFree(chunk); };
    if (SDL_ReadIO(io, chunk, chunk_len) != chunk_len) {
      return false;
    }

    // Run-length decoding
    bool rrle = false;
    u32  rlen = 0;
    u8   rval[SRC_BPP] = { };

    u8* p = chunk;
    for (u32 x = 0; x < bp2->bih.biWidth; ++x) {
      u8* col = slice + x * DST_BPP;
      for (u32 y = 0; y < 8; ++y) {
        while (rlen == 0) {
          if (chunk_len < 2) {
            return SDL_SetError("Malformed slice");
          }
          const u16 ctrl = p[0] | (((u16)p[1]) << 8);
          p += 2;
          chunk_len -= 2;
          if (ctrl & 0x8000) {
            // repeat pixel
            if (chunk_len < SRC_BPP) {
              return SDL_SetError("Malformed slice");
            }
            for (usize plane = 0; plane < SRC_BPP; ++plane) {
              rval[plane] = *p;
              ++p;
            }
            chunk_len -= SRC_BPP;
            rlen = ctrl & 0x7FFF;
            rrle = true;
          } else {
            // read literals
            rlen = ctrl;
            rrle = false;
          }
        }
        if (rrle) {
          for (usize plane = 0; plane < DST_BPP; ++plane) {
            col[plane] = rval[plane % SRC_BPP];
          }
        } else {
          if (chunk_len < SRC_BPP) {
            return SDL_SetError("Malformed slice");
          }
          u8 literal[SRC_BPP];
          for (usize plane = 0; plane < SRC_BPP; ++plane) {
            literal[plane] = *p;
            ++p;
          }
          chunk_len -= SRC_BPP;
          for (usize plane = 0; plane < DST_BPP; ++plane) {
            col[plane] = literal[plane % SRC_BPP];
          }
        }
        --rlen;
        col += dst_pitch;
      }
    }
    for (u32 y = 0; y < 8; ++y) {
      u8* rsrc = slice + dst_pitch * y;
      u8* rdst = (u8*)surf->pixels + surf->pitch * (y + i * 8);
      SDL_memcpy(rdst, rsrc, bp2->bih.biWidth * DST_BPP);
    }
  }

  if (bp2->bih.biHeight % 8 != 0) {
    if ((bp2->bih.biHeight % 8) * dst_pitch != bp2->bp2.extra_slice_count) {
      return SDL_SetError("Malformed trailing data");;
    }
    u32 extra_bytes = 0;
    if (!SDL_ReadU32LE(io, &extra_bytes)) {
      return false;
    }

    if (SDL_ReadIO(io, slice, extra_bytes) != extra_bytes) {
      return false;
    }
    if (extra_bytes != bp2->bp2.extra_slice_count) {
      return false;
    }
    u32 extra = bp2->bih.biHeight % 8;
    for (u32 y = 0; y < extra; ++y) {
      u8* src_row = slice + dst_pitch * y;
      u8* dst_row = (u8*)surf->pixels + surf->pitch * (bp2->bih.biHeight - extra + y);
      SDL_memcpy(dst_row, src_row, bp2->bih.biWidth * DST_BPP);
    }
  }

  if (!SDL_FlipSurface(surf, SDL_FLIP_VERTICAL)) {
    return false;
  }

  return true;
}

bool LoadBP2(Bitmap* bmp, SDL_IOStream* src)
{
  BP2Params bpar = { };

  bool ok =
    SDL_ReadU32LE(src, &bpar.bp2.magic) &&
    SDL_ReadU32LE(src, &bpar.bp2.encoding) &&
    SDL_ReadU32LE(src, &bpar.bp2.palette_len) &&
    SDL_ReadU32LE(src, &bpar.bp2.idk) &&
    SDL_ReadU32LE(src, &bpar.bp2.slice_count) &&
    SDL_ReadU32LE(src, &bpar.bp2.extra_slice_count);
  if (!ok) {
    return false;
  }

  if (bpar.bp2.magic != 999) {
    return SDL_SetError("File is not a BP2 image");
  }

  ok &= LoadBMPFileHeader(&bpar.bfh, src);
  ok &= LoadBMPInfoHeader(&bpar.bih, src);
  if (!ok) {
    return false;
  }

  // Palette data
  if (bpar.bp2.palette_len > 0) {
    if ((bpar.bp2.palette_len % 4) != 0) {
      return SDL_SetError("Malformed image palette");
    }
    u32 ncolors = bpar.bp2.palette_len / 4;

    usize len = bpar.bp2.palette_len;
    u8* palette = MemAlloc<u8>(bpar.bp2.palette_len);
    if (SDL_ReadIO(src, palette, len) != len) {
      return false;
    }
    defer { MemFree(palette); };

    // Convert palette bytes to SDL_Colors
    SDL_Color* colors = MemAlloc<SDL_Color>(ncolors);
    defer { MemFree(colors); };

    for (u32 i = 0; i < ncolors; ++i) {
      colors[i].r = palette[i * 4 + 2];
      colors[i].g = palette[i * 4 + 1];
      colors[i].b = palette[i * 4 + 0];
      colors[i].a = 0xFF;
    }

    bmp->pal = SDL_CreatePalette(bpar.bp2.palette_len / 4);
    if (!bmp->pal) {
      return false;
    }
    if (!SDL_SetPaletteColors(bmp->pal, colors, 0, ncolors)) {
      return false;
    }
  }

  const SDL_PixelFormat format_map[] = {
    SDL_PIXELFORMAT_UNKNOWN,
    SDL_PIXELFORMAT_INDEX8,
    SDL_PIXELFORMAT_BGR24,
    SDL_PIXELFORMAT_BGR24,
  };

  bmp->surf = SDL_CreateSurface(bpar.bih.biWidth,bpar.bih.biHeight, 
                                format_map[bpar.bp2.encoding]);
  if (!bmp->surf || !SDL_LockSurface(bmp->surf)) {
    return false;
  }

  // @@: Only do this in debug builds
  SDL_FillSurfaceRect(bmp->surf, NULL, 0xFF00FF);

  switch (bpar.bp2.encoding) {
  case BP2_FMT_INDEX8: {
    ok &= BP2_DecodeRLE<1>(bmp->surf, src, &bpar);
    ok &= SDL_SetSurfacePalette(bmp->surf, bmp->pal);
  } break;
  case BP2_FMT_BGR888: {
    ok &= BP2_DecodeRLE<3>(bmp->surf, src, &bpar);
  } break;
  case BP2_FMT_GRAY8: {
    ok &= BP2_DecodeRLE<1, 3>(bmp->surf, src, &bpar);
  } break;
  default: {
    ok &= SDL_SetError("Invalid encoding method: %d", bpar.bp2.encoding);
  } break;
  }

  SDL_UnlockSurface(bmp->surf);

  if (!ok) {
    SDL_DestroySurface(bmp->surf);
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
// BP3 files
//-----------------------------------------------------------------------------

enum
{
  BP3_FMT_SOLID  = 0,
  BP3_FMT_BGR332 = 1,
  BP3_FMT_BGR233 = 2,
  BP3_FMT_BGR323 = 3,
  BP3_FMT_GRAY4  = 4,
  BP3_FMT_GRAY8  = 5,
  BP3_FMT_BGR555 = 6,
  BP3_FMT_BGR888 = 7,
};

struct BP3
{
  u32 magic;
  u32 width;
  u32 height;
  u32 decompressed_length;
};

struct BP3Params
{
  BP3            bp3;
  BMP_FileHeader bfh;
  BMP_InfoHeader bih;
};

bool LoadBP3(Bitmap* bmp, SDL_IOStream* io)
{
  BP3Params bpar = { };
  bool ok =
    SDL_ReadU32LE(io, &bpar.bp3.magic) &&
    SDL_ReadU32LE(io, &bpar.bp3.width) &&
    SDL_ReadU32LE(io, &bpar.bp3.height) &&
    SDL_ReadU32LE(io, &bpar.bp3.decompressed_length);
  if (!ok) {
    return false;
  }

  if (bpar.bp3.magic != 0x88888888) {
    return SDL_SetError("Invalid file");
  }

  ok &= LoadBMPFileHeader(&bpar.bfh, io);
  ok &= LoadBMPInfoHeader(&bpar.bih, io);
  if (!ok) {
    return false;
  }

  const u32 padded_w      = Align8(bpar.bp3.width);
  const u32 padded_h      = Align8(bpar.bp3.height);
  const u32 num_tiles     = (padded_w * padded_h) / 64;
  const u32 tiles_per_row = padded_w / 8;
  const u32 grid_row_bytes = 3 * padded_w;

  u8* mode_tab = MemAlloc<u8>(num_tiles);
  defer { MemFree(mode_tab); };
  if (SDL_ReadIO(io, mode_tab, num_tiles) != num_tiles) {
    return false;
  }

  u8* param_tab = MemAlloc<u8>(num_tiles * 3);
  defer { MemFree(param_tab); };
  if (SDL_ReadIO(io, param_tab, num_tiles * 3) != num_tiles * 3) {
    return false;
  }

  // full padded grid (BGR24)
  u8* outbuf = MemAlloc<u8>(padded_w * padded_h * 3);
  defer { MemFree(outbuf); };
  SDL_memset(outbuf, 0, padded_w * padded_h * 3);

  // scratch for one tile (max 24 bpp * 8 rows = 192 bytes)
  u8 tile_buf[192];

  for (u32 i = 0; i < num_tiles; ++i) {
    // tile extents (handle right/bottom partial tiles)
    u32 chunk_w = 8;
    if ((i % tiles_per_row) * 8 + 8 >= (u32)bpar.bp3.width) {
      chunk_w = (u32)bpar.bp3.width + 8 - padded_w;
    }

    u32 chunk_h = 8;
    if ((i / tiles_per_row) * 8 + 8 >= (u32)bpar.bp3.height) {
      chunk_h = (u32)bpar.bp3.height + 8 - padded_h;
    }

    u32 bpp = 0;
    switch (mode_tab[i]) {
      case BP3_FMT_SOLID:  bpp = 0;  break;
      case BP3_FMT_BGR332: bpp = 8;  break;
      case BP3_FMT_BGR233: bpp = 8;  break;
      case BP3_FMT_BGR323: bpp = 8;  break;
      case BP3_FMT_GRAY4:  bpp = 4;  break;
      case BP3_FMT_GRAY8:  bpp = 8;  break;
      case BP3_FMT_BGR555: bpp = 16; break;
      case BP3_FMT_BGR888: bpp = 24; break;
      default: return SDL_SetError("Invalid tile mode");
    }

    SDL_memset(tile_buf, 0, sizeof(tile_buf));

    // read this tile’s packed rows, pad each stored row to 'bpp' bytes, then pad to 8 rows
    if (bpp > 0) {
      const Sint64 pos0 = SDL_TellIO(io);
      if (pos0 < 0) {
        return false;
      }

      const u32 stored_row_bytes = (bpp * chunk_w) / 8;
      const u32 pad_per_row      = bpp - stored_row_bytes;  // pad each row to 'bpp' bytes
      u32 dst = 0;

      for (u32 y = 0; y < chunk_h; ++y) {
        if (stored_row_bytes) {
          if (dst + stored_row_bytes > sizeof(tile_buf)) {
            return SDL_SetError("Malformed tile data");
          }
          if (SDL_ReadIO(io, &tile_buf[dst], stored_row_bytes) != stored_row_bytes) {
            return false;
          }
          dst += stored_row_bytes;
        }
        if (pad_per_row) {
          if (dst + pad_per_row > sizeof(tile_buf)) {
            return SDL_SetError("Malformed tile data");
          }
          SDL_memset(&tile_buf[dst], 0, pad_per_row);
          dst += pad_per_row;
        }
      }

      const u32 total_needed = bpp * 8;
      if (total_needed > sizeof(tile_buf)) {
        return SDL_SetError("Malformed tile data");
      }
      if (dst < total_needed) {
        SDL_memset(&tile_buf[dst], 0, total_needed - dst);
      }

      // advance file by actual stored bytes for this tile (no per-row padding)
      const u64 stored_total = (u64)bpp * (u64)chunk_w * (u64)chunk_h / 8;
      if (SDL_SeekIO(io, pos0 + (Sint64)stored_total, SDL_IO_SEEK_SET) < 0) {
        return false;
      }
    }

    // decode tile into outbuf
    const u32 tile_row_base = grid_row_bytes * 8 * (i / tiles_per_row);
    const u32 tile_col_base = 24 * (i % tiles_per_row);
    const u32 src_step      = bpp / 8; // bytes per pixel for most modes

    const u8 base_b = param_tab[3 * i + 0];
    const u8 base_g = param_tab[3 * i + 1];
    const u8 base_r = param_tab[3 * i + 2];

    u32 src_row_byte_off = 0; // start of row in tile_buf (bpp bytes per row)

    for (u32 ty = 0; ty < 8; ++ty) {
      u32 dst_off = tile_col_base + tile_row_base + ty * grid_row_bytes;
      u32 src_off = src_row_byte_off;

      for (u32 tx = 0; tx < 8; ++tx) {
        switch (mode_tab[i]) {
          case BP3_FMT_SOLID: {
            outbuf[dst_off + 0] = base_b;
            outbuf[dst_off + 1] = base_g;
            outbuf[dst_off + 2] = base_r;
          } break;
          case BP3_FMT_BGR332: {
            const u8 p = tile_buf[src_off];
            outbuf[dst_off + 0] = (((p >> 0) & 7) + base_b) & 0xFF;
            outbuf[dst_off + 1] = (((p >> 3) & 7) + base_g) & 0xFF;
            outbuf[dst_off + 2] = (((p >> 6) & 3) + base_r) & 0xFF;
          } break;
          case BP3_FMT_BGR233: {
            const u8 p = tile_buf[src_off];
            outbuf[dst_off + 0] = (((p >> 0) & 3) + base_b) & 0xFF;
            outbuf[dst_off + 1] = (((p >> 2) & 7) + base_g) & 0xFF;
            outbuf[dst_off + 2] = (((p >> 5) & 7) + base_r) & 0xFF;
          } break;
          case BP3_FMT_BGR323: {
            const u8 p = tile_buf[src_off];
            outbuf[dst_off + 0] = (((p >> 0) & 7) + base_b) & 0xFF;
            outbuf[dst_off + 1] = (((p >> 3) & 3) + base_g) & 0xFF;
            outbuf[dst_off + 2] = (((p >> 5) & 7) + base_r) & 0xFF;
          } break;
          case BP3_FMT_GRAY4: {
            const u8 p = tile_buf[src_off];
            const u8 nib = (tx & 1) ? (u8)((p >> 4) & 0x0F) : (u8)(p & 0x0F);
            outbuf[dst_off + 0] = (nib + base_b) & 0xFF;
            outbuf[dst_off + 1] = (nib + base_g) & 0xFF;
            outbuf[dst_off + 2] = (nib + base_r) & 0xFF;
          } break;
          case BP3_FMT_GRAY8: {
            const u8 p = tile_buf[src_off];
            outbuf[dst_off + 0] = p;
            outbuf[dst_off + 1] = p;
            outbuf[dst_off + 2] = p;
          } break;
          case BP3_FMT_BGR555: {
            const u8 p0 = tile_buf[src_off + 0];
            const u8 p1 = tile_buf[src_off + 1];
            outbuf[dst_off + 0] = (u8)(((p0) & 0x1F) + base_b);
            outbuf[dst_off + 1] = (u8)((((p0 >> 5) + 8 * (p1 & 3)) + base_g));
            outbuf[dst_off + 2] = (u8)(((p1 & 0x7C) >> 2) + base_r);
          } break;
          case BP3_FMT_BGR888: {
            outbuf[dst_off + 0] = tile_buf[src_off + 0];
            outbuf[dst_off + 1] = tile_buf[src_off + 1];
            outbuf[dst_off + 2] = tile_buf[src_off + 2];
          } break;
        }

        // GRAY4: advance one byte every 2 pixels
        if (mode_tab[i] == 4) {
          if ((tx & 1) == 1) {
            src_off += 1;
          }
        } else {
          src_off += src_step;
        }

        dst_off += 3;
      }

      src_row_byte_off += bpp;
    }
  }

  // create and fill the SDL surface (BGR24)
  bmp->surf = SDL_CreateSurface(bpar.bp3.width, bpar.bp3.height, SDL_PIXELFORMAT_BGR24);
  if (!bmp->surf || !SDL_LockSurface(bmp->surf)) {
    return false;
  }

  for (u32 y = 0; y < bpar.bp3.height; ++y) {
    u8* dst = (u8*)bmp->surf->pixels + y * bmp->surf->pitch;
    const u8* src = outbuf + (size_t)y * grid_row_bytes;
    SDL_memcpy(dst, src, (size_t)bpar.bp3.width * 3u);
  }

  if (!SDL_FlipSurface(bmp->surf, SDL_FLIP_VERTICAL)) {
    return false;
  }

  SDL_UnlockSurface(bmp->surf);

  return true;
}

//-----------------------------------------------------------------------------
// TXT files
//-----------------------------------------------------------------------------

char* ShiftToUTF8(u8* shift_jis_string)
{
  usize utf8_len = cp932_to_utf8_len((char*)shift_jis_string);

  char* result = MemAllocZ<char>(utf8_len + 1);
  cp932_to_utf8(result, (char*)shift_jis_string);

  return result;
}

char* DecodeTXT_1997(SDL_IOStream* io)
{
  u8  txt_magic = 0;
  u32 txt_len = 0;

  if (!SDL_ReadU8(io, &txt_magic) || txt_magic != 1) {
    SDL_SetError("File is not a TXT script");
    return NULL;
  }

  if (!SDL_ReadU32LE(io, &txt_len)) {
    return NULL;
  }

  u8* data = MemAllocZ<u8>(txt_len + 1);
  defer { MemFree(data); };

  if (SDL_ReadIO(io, data, txt_len) != txt_len) {
    return NULL;
  }

  for (u32 i = 0; i < txt_len; ++i) {
    data[i] ^= 0xFF;
  }

  return ShiftToUTF8(data);
}

char* DecodeTXT_2006(SDL_IOStream* io, usize len)
{
  if (!len) {
    if (SDL_SeekIO(io, 0, SDL_IO_SEEK_END) < 0) {
      return NULL;
    }
    len = SDL_TellIO(io);
    if (SDL_SeekIO(io, 0, SDL_IO_SEEK_SET) < 0) {
      return NULL;
    }
  }

  u8* data = MemAllocZ<u8>(len + 1);
  defer { MemFree(data); };

  if (SDL_ReadIO(io, data, len) != len) {
    return NULL;
  }

  for (u32 i = 0; i < len; ++i) {
    if (data[i] > 0xF) {
      data[i] = 0xE - data[i];
    }
  }

  return ShiftToUTF8(data);
}

//-----------------------------------------------------------------------------
// BIN/LB5 files
//-----------------------------------------------------------------------------

static bool LoadBinIDX(Span<PackEntry>* entries, SDL_IOStream* src)
{
  u32 len = 0;
  if (!SDL_ReadU32LE(src, &len)) {
    return false;
  }
  entries->len = len;

  if (entries->len > 0) {
    entries->buf = MemAlloc<PackEntry>(entries->len);
  }

  bool ok = true;
  for (u32 i = 0; i < len && ok; ++i) {
    PackEntry* entry = &entries->Get(i);
    u32 name_len;
    if (!(ok &= SDL_ReadU32LE(src, &name_len))) {
      break;
    }

    u8* name_jis = MemAllocZ<u8>(name_len + 1);
    defer { MemFree(name_jis); };
    if (!(ok &= (SDL_ReadIO(src, name_jis, name_len) == name_len))) {
      break;
    }

    ok &=
      SDL_ReadU32LE(src, &entry->off) &&
      SDL_ReadU32LE(src, &entry->len);

    if (ok) {
      entry->name = MemAllocZ<char>(cp932_to_utf8_len((const char*)name_jis));
      cp932_to_utf8(entry->name, (const char*)name_jis);
    }
  }

  if (!ok) {
    MemFree(entries->buf);
  }

  return ok;
}

static bool LoadLB5IDX(Span<PackEntry>* entries, SDL_IOStream* src)
{
  u32 len = 0;
  if (!SDL_ReadU32LE(src, &len)) {
    return false;
  }
  entries->len = len;

  if (entries->len > 0) {
    entries->buf = MemAlloc<PackEntry>(entries->len);
  }

  bool ok = true;
  for (u32 i = 0; i < len && ok; ++i) {
    PackEntry* entry = &entries->Get(i);
    u8 name_jis[15] = { };
    ok &=
      SDL_ReadU32LE(src, &entry->off) &&
      SDL_ReadU32LE(src, &entry->len) &&
      SDL_SeekIO(src, 1, SDL_IO_SEEK_CUR) >= 0;

    ok &= (SDL_ReadIO(src, name_jis, sizeof(name_jis)) == sizeof(name_jis));
      
    if (ok) {
      entry->name = MemAllocZ<char>(64);
      cp932_to_utf8(entry->name, (const char*)name_jis);
    }
  }

  if (!ok) {
    MemFree(entries->buf);
  }

  return ok;
}

bool OpenPackFile(PackFile* pack, const char* path)
{
  const char* ext = Extension(path);
  if (!ext) {
    return SDL_SetError("Invalid file");
  }

  pack->lump_file = SDL_IOFromFile(path, "rb");
  if (!pack->lump_file) {
    return false;
  }

  const bool is_bin = !SDL_strcasecmp(ext, "bin");
  const bool is_lb5 = !SDL_strcasecmp(ext, "lb5");
  if (!is_bin && !is_lb5) {
    return SDL_SetError("Invalid file");
  }

  // Replace extension with .idx
  usize path_len = SDL_strlen(path);
  char* idx_path = SDL_strdup(path);
  strncpy(idx_path + path_len - 3, "idx", 3);

  defer { SDL_free(idx_path); };

  SDL_IOStream* idx_io = SDL_IOFromFile(idx_path, "rb");
  if (!idx_io) {
    return false;
  }

  if (is_bin) {
    return LoadBinIDX(&pack->entries, idx_io);
  }
  if (is_lb5) {
    return LoadLB5IDX(&pack->entries, idx_io);
  }
  return SDL_SetError("Invalid file");
}

void PackFile::Close()
{
  SDL_CloseIO(lump_file);
  for (PackEntry* e = entries.Begin(); e != entries.End(); ++e) {
    MemFree(e->name);
  }
  MemFree(entries.buf);
}

void* PackFile::ReadEntry(const PackEntry* entry)
{
  if (SDL_SeekIO(lump_file, entry->off, SDL_IO_SEEK_SET) < 0) {
    return NULL;
  }

  void* result = MemAlloc<u8>(entry->len);
  if (SDL_ReadIO(lump_file, result, entry->len) != entry->len) {
    MemFree(result);
    return NULL;
  }

  return result;
}
