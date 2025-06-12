cmake_minimum_required(VERSION 3.19)
project(Hycov VERSION "0.2")
set(CMAKE_CXX_STANDARD 23)

add_compile_definitions(WLR_USE_UNSTABLE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED hyprland pixman-1 libdrm pango pangocairo)

add_library(hycov SHARED
        src/main.cpp
        src/dispatchers.cpp
        src/OvGridLayout.cpp
        src/globaleventhook.cpp
)

target_include_directories(hycov PRIVATE ${DEPS_INCLUDE_DIRS})

# Enable ASan
# target_compile_options(hycov PRIVATE -fsanitize=address -fno-omit-frame-pointer -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing -Wno-pointer-arith)
# target_link_options(hycov PRIVATE -fsanitize=address)

install(TARGETS hycov LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
