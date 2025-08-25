cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(FantaTech C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/bin")

include("${CMAKE_CURRENT_LIST_DIR}/thirdparty/CMakeLists.txt")

#
# Base library
#
add_library(ftbase STATIC
  ftbase.cc
)

# Maximum warnings
if(MSVC)
  target_compile_options(ftbase PUBLIC
    /W3
  )
else()
  target_compile_options(ftbase PUBLIC
    -Wall -Wextra -Wpedantic
    -Wno-newline-eof
    -Wno-unused-parameter
  )
endif()

# Pretty much everything needs SDL and cp932
target_link_libraries(ftbase PUBLIC
  ftdep-cp932
  ftdep-SDL3
)

# Use C++20
target_compile_features(ftbase PUBLIC cxx_std_20)

#
# Format parser library
#
add_library(ftformat STATIC
  ftformat.cc
)
target_link_libraries(ftformat PUBLIC
  ftbase
)

#
# ftconv
#
add_executable(ftconv
  ftconv.cc
)
target_link_libraries(ftconv PRIVATE
  ftformat
)

#
# Game executable
#
add_executable(fantatech
  ftgame.cc
)
target_link_libraries(fantatech PRIVATE
  ftformat

  ftdep-dear-imgui
)

