#include "ftformat.hh"
#include <cstdlib>

const char* usage_str =
R"(Usage: ftconv [options...] <input> [output]

Command-line utility to pack, unpack, and convert game files for:
 - Neon Genesis Evangelion: Girlfriend of Steel (1997, PC)
 - Neon Genesis Evangelion: Girlfriend of Steel (Special Edition) (2006, PC)

Supported conversions:
  .bin (2006): unpack
  .lb5 (2006): unpack

  .bp2 (1997): decode
  .bp3 (2006): decode
  .txt (1997): decode
  .txt (2006): decode

Options:
  --1997  Target 1997 game when encoding txt
  --help  Display this text
  --ls    List archive contents without unpacking
  --raw   Don't convert inner formats when packing or unpacking
  --yes   Overwrite existing files

Examples:
  ftconv event2048.lb5
    Unpack and convert all files from event2048.lb5 to the current directory

  ftconv music.bin[samisi.wav]
    Unpack samisi.wav from music.bin to the current directory

  ftconv face1024.lb5[ASUKA.*] asuka_faces/
    Unpack and convert all files starting with "ASUKA" from face1024.lb5 to asuka_faces/
)";

// NOT IMPLEMENTED:
//
// ftconv test.bmp face1024.lb5[GENDO.bmp]
//   Convert test.bmp and pack it in face1024.lb5 as GENDO.bmp
//
// ftconv *.txt my_txt.lb5
//   Convert and pack some text files as my_txt.lb5

enum : u8 {
  OPT_1997 = 1 << 0,
  OPT_LS   = 1 << 1,
  OPT_RAW  = 1 << 2,
  OPT_YES  = 1 << 3,
};

enum : u8 {
  FEXT_UNKNOWN = 0,
  FEXT_BIN,
  FEXT_LB5,
};

enum : u8 {
  FTYPE_UNKNOWN = 0,
  FTYPE_BIN,
  FTYPE_LB5,
  FTYPE_BP2,
  FTYPE_BP3,
  FTYPE_BMP,
  FTYPE_TXT_UTF8,
  FTYPE_TXT_1997,
  FTYPE_TXT_2006,
};

struct File
{
  char* path;
  char* subscript;
  bool  is_archive;
};

static struct
{
  u8          options;
  Span<File>  files;
  File*       first_file;
  File*       last_file;
} G = { };

static u8 GuessFileTypeForConversion(File* f, Span<u8> data)
{
  SDL_assert(data.len >= 4);
  const char* ext = Extension(f->path);
  if (ext) {
    if (!SDL_strcasecmp(ext, "bin")) {
      return FTYPE_BIN;
    }
    if (!SDL_strcasecmp(ext, "lb5")) {
      return FTYPE_LB5;
    }
    if (!SDL_strcasecmp(ext, "bp2")) {
      return FTYPE_BP2;
    }
    if (!SDL_strcasecmp(ext, "bmp")) {
      if (data[0] == 0x88 && data[1] == 0x88 && data[2] == 0x88 && data[3] == 0x88) {
        return FTYPE_BP3;
      }
      if (data[0] == 0x42 && data[1] == 0x4D) {
        return FTYPE_BMP;
      }
      return FTYPE_UNKNOWN;
    }
    if (!SDL_strcasecmp(ext, "txt")) {
      // .txt is weird. There are three different formats with this extension, so we
      // need to scan the entire file and guess the encoding

      // 1997 version has a magic, if you can call it that. @@ possible false-positives?
      if (data[0] == 0x01) {
        return FTYPE_TXT_1997;
      }

      // Check if it's valid UTF-8. If not, assume it's 2006 txt
      return IsValidUTF8(data) ? FTYPE_TXT_UTF8 : FTYPE_TXT_2006;
    }
  }
  return FTYPE_UNKNOWN;
}

int main(int argc, const char* argv[])
{
  bool help = argc < 2;
  for (int i = 1; i < argc && !help; ++i) {
    help |= !SDL_strcasecmp(argv[i], "--help") || !SDL_strcasecmp(argv[i], "-h") ;
  }
  if (help) {
    printf("%s\n", usage_str);
    return EXIT_SUCCESS;
  }

  int nfiles = argc - 1;
  for (int i = 1; i < argc; ++i) {
    if (!SDL_strcasecmp(argv[i], "--1997")) {
      G.options |= OPT_1997;
      nfiles -= 1;
    }
    else if (!SDL_strcasecmp(argv[i], "--ls")) {
      G.options |= OPT_LS;
      nfiles -= 1;
    }
    else if (!SDL_strcasecmp(argv[i], "--raw")) {
      G.options |= OPT_RAW;
      nfiles -= 1;
    }
    else if (!SDL_strcasecmp(argv[i], "--yes")) {
      G.options |= OPT_YES;
      nfiles -= 1;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Error: Unknown option %s. See ftconv --help\n", argv[i]);
      return EXIT_FAILURE;
    }
  }

  if (nfiles <= 0) {
    fprintf(stderr, "Error: No files supplied\n");
    return EXIT_FAILURE;
  }

  G.files.len = nfiles;
  G.files.buf = MemAllocZ<File>(G.files.len);

  int cur_file = 0;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      continue;
    }

    File* f = &G.files[cur_file];
    // Parse path
    const char* brace1 = NULL;
    const char* brace2 = NULL;
    for (int j = 0; argv[i][j]; ++j) {
      const char* c = &argv[i][j];
      if (*c == '[') {
        brace1 = c;
      }
      else if (*c == ']') {
        brace2 = c;
      }
    }
    if (brace1 && brace2 && brace2 > brace1) {
      f->path = SDL_strndup(argv[i], brace1 - argv[i]);
      f->subscript = SDL_strndup(brace1 + 1, brace2 - brace1 - 1);
    } else {
      f->path = SDL_strdup(argv[i]);
    }

    const char* ext = Extension(f->path);
    if (ext) {
      if (!SDL_strcasecmp(ext, "bin") || !SDL_strcasecmp(ext, "lb5")) {
        f->is_archive = true;
      }
    }
    
    ++cur_file;
  }

  G.first_file = &G.files[0];
  G.last_file = &G.files[G.files.len - 1];

  // User wants to list
  if ((G.options & OPT_LS)) {
    for (File* f = G.files.Begin(); f != G.files.End(); ++f) {
      if (G.files.len > 1) {
        printf("%s:\n", f->path);
      }
      PackFile pack = { };
      if (OpenPackFile(&pack, f->path)) {
        for (PackEntry* e = pack.entries.Begin(); e != pack.entries.End(); ++e) {
          if (f->subscript && !WildcardMatch(f->subscript, e->name)) {
            continue;
          }
          printf("%s\n", e->name);
        }
        pack.Close();
      } else {
        fprintf(stderr, "Error: %s\n", SDL_GetError());
        return EXIT_FAILURE;
      }
    }
    return EXIT_SUCCESS;
  }

  // User wants to pack
  if (G.files.len > 1 && G.last_file->is_archive) {
    printf("OP: packing into %s\n", G.last_file->path);
    if (G.last_file->subscript) {
      printf("   as %s\n", G.last_file->subscript);
    }
    fprintf(stderr, "Not yet implemented :(\n");
    return EXIT_FAILURE;
  }

  // User wants to unpack
  if (G.first_file->is_archive) {
    if (!(G.options & OPT_RAW)) {
      fprintf(stderr, "Converting while unpacking is not yet implemented :(\n");
      return EXIT_FAILURE;
    }

    if (G.files.len > 2) {
      fprintf(stderr, "Only one archive can be unpacked at a time\n");
      return EXIT_FAILURE;
    }

    File* pack_file = G.first_file;
    const char* dst_dir = (G.files.len == 2) ? G.files[1].path : ".";

    PackFile pack = { };
    if (!OpenPackFile(&pack, pack_file->path)) {
      fprintf(stderr, "Error: %s\n", SDL_GetError());
      return EXIT_FAILURE;
    }

    for (PackEntry* e = pack.entries.Begin(); e != pack.entries.End(); ++e) {
      if (pack_file->subscript && !WildcardMatch(pack_file->subscript, e->name)) {
        continue;
      }
      char dst[GOS_MAX_PATH];
      SDL_snprintf(dst, sizeof(dst), "%s/%s", dst_dir, e->name);
      printf("Unpacking %s\n", e->name);

      void* buf = pack.ReadEntry(e);
      if (buf) {
        if (!SDL_SaveFile(dst, buf, e->len)) {
          fprintf(stderr, "Error: %s\n", SDL_GetError());
        }
        MemFree(buf);
      } else {
        fprintf(stderr, "Error: %s\n", SDL_GetError());
      }
    }

    return EXIT_SUCCESS;
  }

  // User wants to convert
  if (G.files.len == 2) {
    // Load entire file into memory first
    Span<u8> bytes = { };
    if (!(bytes.buf = (u8*)SDL_LoadFile(G.files[0].path, &bytes.len))) {
      fprintf(stderr, "Error: %s\n", SDL_GetError());
      return EXIT_FAILURE;
    }

    SDL_IOStream* io = SDL_IOFromConstMem(bytes.buf, bytes.len);
    if (!io) {
      fprintf(stderr, "Error: %s\n", SDL_GetError());
      return EXIT_FAILURE;
    }

    switch (GuessFileTypeForConversion(&G.files[0], bytes)) {
    case FTYPE_BP2: {
      Bitmap bmp = { };
      if (!LoadBP2(&bmp, io)) {
        fprintf(stderr, "Error decoding: %s\n", SDL_GetError());
      }
      if (!SDL_SaveBMP(bmp.surf, G.files[1].path)) {
        fprintf(stderr, "Error writing: %s\n", SDL_GetError());
      }
      return EXIT_SUCCESS;
    } break;
    case FTYPE_BP3: {
      Bitmap bmp = { };
      if (!LoadBP3(&bmp, io)) {
        fprintf(stderr, "Error decoding: %s\n", SDL_GetError());
      }
      if (!SDL_SaveBMP(bmp.surf, G.files[1].path)) {
        fprintf(stderr, "Error writing: %s\n", SDL_GetError());
      }
      return EXIT_SUCCESS;
    } break;
    case FTYPE_TXT_1997: {
      char* text = DecodeTXT_1997(io);
      if (!text) {
        fprintf(stderr, "Error decoding: %s\n", SDL_GetError());
      }
      if (!SDL_SaveFile(G.files[1].path, text, SDL_strlen(text) + 1)) {
        fprintf(stderr, "Error writing: %s\n", SDL_GetError());
      }
      return EXIT_SUCCESS;
    } break;
    case FTYPE_TXT_2006: {
      char* text = DecodeTXT_2006(io);
      if (!text) {
        fprintf(stderr, "Error decoding: %s\n", SDL_GetError());
      }
      if (!SDL_SaveFile(G.files[1].path, text, SDL_strlen(text) + 1)) {
        fprintf(stderr, "Error writing: %s\n", SDL_GetError());
      }
      return EXIT_SUCCESS;
    } break;
    case FTYPE_TXT_UTF8: {
      fprintf(stderr, "TODO: FTYPE_TXT_UTF8\n");
      return EXIT_FAILURE;
    } break;
    case FTYPE_BIN:
    case FTYPE_LB5: {
      fprintf(stderr, "Unsupported source file type\n");
      return EXIT_FAILURE;
    } break;
    case FTYPE_UNKNOWN: {
      fprintf(stderr, "Unknown source file type\n");
      return EXIT_FAILURE;
    } break;
    default: {
      fprintf(stderr, 
              "Error: Unhandled but known file format. Please report this as a bug!\n");
      return EXIT_FAILURE;
    } break;
    }
  }

  fprintf(stderr, "Unknown operation. See ftconv --help\n");
  return EXIT_FAILURE;
}
