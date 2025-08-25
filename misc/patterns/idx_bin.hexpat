// .idx for .bin files

struct IDX_Entry
{
  u32 name_len;
  char name[name_len];
  u32 off;
  u32 len;
};

struct IDX
{
  u32 count;
  IDX_Entry entries[count];
};

IDX idx @ 0x0;