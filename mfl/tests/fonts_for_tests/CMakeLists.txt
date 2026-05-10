cmake_minimum_required(VERSION 3.14)

project(fonts_for_tests)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
find_package(expat CONFIG REQUIRED)
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)

if (UNIX AND NOT APPLE)
    find_path(LIBUUID_INCLUDE_DIR uuid.h PATH_SUFFIXES uuid)
    find_library(LIBUUID_LIBRARY libuuid.a)
endif ()

add_library(fonts_for_tests
        src/font_face.cpp
        src/freetype.cpp
        src/freetype_error.cpp
)

set_property(TARGET fonts_for_tests PROPERTY CXX_STANDARD 23)

# Отключаем предупреждения об old-style cast для внешних библиотек (HarfBuzz, FreeType)
target_compile_options(fonts_for_tests PRIVATE -Wno-old-style-cast)

target_compile_definitions(fonts_for_tests PRIVATE MFL_FONT_DIR="${CMAKE_CURRENT_LIST_DIR}/otf/")

target_include_directories(fonts_for_tests PUBLIC
        ./src
        ./include
        ./../../include
)

# Помечаем внешние библиотеки как SYSTEM, чтобы отключить для них строгие предупреждения
target_include_directories(fonts_for_tests SYSTEM PUBLIC
        ${FREETYPE_INCLUDE_DIRS}
        ${HARFBUZZ_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
)

target_link_libraries(fonts_for_tests PUBLIC
        ${FREETYPE_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${HARFBUZZ_LIBRARIES}
        expat::expat
        mfl_options
        mfl_warnings
)

target_link_directories(fonts_for_tests PUBLIC
        ${FREETYPE_LIBRARY_DIRS}
        ${HARFBUZZ_LIBRARY_DIRS}
        ${FONTCONFIG_LIBRARY_DIRS}
)

if (UNIX AND NOT APPLE)
    target_link_libraries(fonts_for_tests PUBLIC ${LIBUUID_LIBRARY})
endif ()
