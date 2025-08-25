import std.sys;
import type.magic;

fn fourcc(str s)
{
  return (u32(s[0]) <<  0) | 
         (u32(s[1]) <<  8) |
         (u32(s[2]) << 16) |
         (u32(s[3]) << 24);
};

fn unfourcc(u32 v) 
{
  u8 a =  v        & 0xFF;
  u8 b = (v >> 8)  & 0xFF;
  u8 c = (v >> 16) & 0xFF;
  u8 d = (v >> 24) & 0xFF;
  return type::escape_bytes(std::format("{:c}{:c}{:c}{:c}", a, b, c, d));
};

struct DFS
{
  u32 magic [[format("unfourcc")]];
  u32;
  u32;
  u32;
  u32;
  u32;
  u32;
  if (magic != fourcc("DFS0")) {
    u32;
    u32;
  }
};

struct ZDF
{
  DFS dfs;
};

ZDF zdf @ 0x0;