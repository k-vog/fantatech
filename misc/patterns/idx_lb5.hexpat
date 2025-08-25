// .idx for .lb5 files

struct IDX_Entry
{
  u32 off;
  u32 len;
  padding[1];
  char name[15];
};

struct IDX
{
  u32 count;
  IDX_Entry entries[count];
};

IDX idx @ 0x0;