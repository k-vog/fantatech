#include "ftgame.hh"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

// Attempt to open a file, seacrhing in multiple common locations
static SDL_IOStream* OpenGameFile(const char* path)
{
  const char* disk_paths[] = {
#ifdef _WIN32
    "C:\\eva95",
#else
    "~/.wine/drive_c/eva95",
#endif
  };
  for (usize i = 0; i < ArrLen(disk_paths); ++i) {
    char full[GOS_MAX_PATH] = { };

    SDL_snprintf(full, sizeof(full), "%s/%s", ExpandPath(disk_paths[i]), path);

    SDL_IOStream* io = SDL_IOFromFile(full, "rb");
    printf("looking for %s in %s\n", path, full);;
    if (io) {
      return io;
    }
  }
  return NULL;
}

int main(int argc, const char* argv[])
{
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
    // @@ errors
    SDL_assert(0);
  }

  // @@ make this more dynamic
  const f32 scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

  const char* title =
    "Neon Genesis Evangelion: Girlfriend of Steel (Special Edition) (FantaTech)";
  u32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
  SDL_Window* wnd = SDL_CreateWindow(title, 1024, 768, flags);
  SDL_assert(wnd);

  SDL_Renderer* rnd = SDL_CreateRenderer(wnd, 0);
  SDL_assert(rnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiStyle& imstyle = ImGui::GetStyle();
  imstyle.ScaleAllSizes(scale);
  imstyle.FontScaleDpi = scale;

  ImGuiIO& imio = ImGui::GetIO();

  ImGui_ImplSDL3_InitForSDLRenderer(wnd, rnd);
  ImGui_ImplSDLRenderer3_Init(rnd);

  SDL_IOStream* io = OpenGameFile("grp/BG01.BP2");
  if (!io) {
    fprintf(stderr, "Error: %s\n", SDL_GetError());
    return 1;
  }

  Bitmap bmp = { };
  if (!LoadBP2(&bmp, io)) {
    fprintf(stderr, "Error: %s\n", SDL_GetError());
    return 1;
  }
  SDL_CloseIO(io);

  io = OpenGameFile("exec/GAME01.TXT");
  if (!io) {
    fprintf(stderr, "Error: %s\n", SDL_GetError());
    return 1;
  }

  char* script = DecodeTXT_1997(io);
  if (!script) {
    fprintf(stderr, "Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_CloseIO(io);

  printf("SCRIPT:\n%s\n", script);
  
  bmp.tex = SDL_CreateTextureFromSurface(rnd, bmp.surf);
  SDL_assert(bmp.tex);
  SDL_DestroySurface(bmp.surf);

  bool running = true;
  while (running) {
    SDL_Event evt = { };
    while (SDL_PollEvent(&evt)) {
      ImGui_ImplSDL3_ProcessEvent(&evt);
      switch (evt.type) {
      case SDL_EVENT_QUIT: {
        running = false;
      } break;
      }
    }

    SDL_SetRenderScale(rnd, 1.0f, 1.0f);
    SDL_RenderTexture(rnd, bmp.tex, NULL, NULL);

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();
    SDL_SetRenderScale(rnd, imio.DisplayFramebufferScale.x,
                       imio.DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), rnd);

    SDL_RenderPresent(rnd);
  }

  SDL_DestroyRenderer(rnd);
  SDL_DestroyWindow(wnd);
}