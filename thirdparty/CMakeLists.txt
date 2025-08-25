include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

#
# Dependency: cp932* from thtk
# https://github.com/thpatch/thtk/tree/master/util
#
add_library(ftdep-cp932 STATIC
  "${CMAKE_CURRENT_LIST_DIR}/thtk_cp932/cp932.c"
)

#
# Dependency: Dear ImGui
#
FetchContent_Declare(
  dear_imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.92.2b
  GIT_PROGRESS   TRUE
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(dear_imgui)
add_library(ftdep-dear-imgui STATIC
  "${dear_imgui_SOURCE_DIR}/imgui.cpp"
  "${dear_imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${dear_imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${dear_imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${dear_imgui_SOURCE_DIR}/imgui_widgets.cpp"

  "${dear_imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
  "${dear_imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)
target_include_directories(ftdep-dear-imgui PUBLIC
  "${dear_imgui_SOURCE_DIR}"
)
target_link_libraries(ftdep-dear-imgui PRIVATE
  ftdep-SDL3
)
target_compile_features(ftdep-dear-imgui PRIVATE cxx_std_17)

#
# Dependency: SDL3
# https://www.libsdl.org/
#
if(WIN32)
  FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.18
    GIT_PROGRESS   TRUE
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(SDL)
  add_library(ftdep-SDL3 INTERFACE)
  target_link_libraries(ftdep-SDL3 INTERFACE SDL3::SDL3)
elseif(APPLE AND FTECH_BUILD_MACOS_APP)
  # @@ use .framework
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL3 REQUIRED SDL3)
  add_library(ftdep-SDL3 INTERFACE)
  target_include_directories(ftdep-SDL3 INTERFACE ${SDL3_INCLUDE_DIRS})
  target_compile_options(ftdep-SDL3 INTERFACE ${SDL3_CFLAGS_OTHER})
  target_link_options(ftdep-SDL3 INTERFACE ${SDL3_LDFLAGS})
endif()
