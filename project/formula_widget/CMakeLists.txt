cmake_minimum_required(VERSION 3.16)
project(formula_widget)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Freetype REQUIRED)
find_package(harfbuzz REQUIRED)

# mfl as subproject
set(mfl_BUILD_TESTS OFF CACHE BOOL "Disable mfl tests")
set(mfl_BUILD_FONTS_FOR_TESTS OFF CACHE BOOL "Disable mfl fonts for tests")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../mfl ${CMAKE_CURRENT_BINARY_DIR}/mfl)

# Collect source files
set(SOURCES
    src/ft_library.cpp
    src/ft_font_face.cpp
    src/formula_widget.cpp
    src/formula_cursor.cpp
    src/tex_cursor_manager.cpp
    src/math_ast.cpp
    src/ast_cursor.cpp
    src/math_semantics.cpp
    src/cas_bridge.cpp
    src/main.cpp
)

set(HEADERS
    src/ft_library.hpp
    src/ft_font_face.hpp
    src/formula_widget.hpp
    src/formula_cursor.hpp
    src/tex_cursor_manager.hpp
    src/math_ast.hpp
    src/ast_cursor.hpp
    src/math_semantics.hpp
    src/cas_bridge.hpp
)

# Create executable
add_executable(formula_widget
    ${SOURCES}
    ${HEADERS}
)

# Create test executable (smoke test of mfl layout with real STIX fonts)
add_executable(test_layout
    test_layout.cpp
    src/ft_library.cpp
    src/ft_font_face.cpp
)

# Link libraries
target_link_libraries(formula_widget PRIVATE
    mfl
    Qt6::Widgets
    Freetype::Freetype
    harfbuzz::harfbuzz
)

target_link_libraries(test_layout PRIVATE
    mfl
    Freetype::Freetype
    harfbuzz::harfbuzz
)

target_include_directories(test_layout PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/../mfl/include
)

# Include directories
target_include_directories(formula_widget PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/../mfl/include
)

# Compiler warnings
if(MSVC)
    target_compile_options(formula_widget PRIVATE /W4)
else()
    target_compile_options(formula_widget PRIVATE -Wall -Wextra -pedantic)
endif()

# Offscreen-харнесс для визуальной проверки каретки (не тест)
add_executable(caret_harness
    tests/caret_harness.cpp
    src/ft_library.cpp
    src/ft_font_face.cpp
    src/formula_widget.cpp
    src/formula_cursor.cpp
    src/math_ast.cpp
    src/ast_cursor.cpp
)
target_link_libraries(caret_harness PRIVATE
    mfl Qt6::Widgets Freetype::Freetype harfbuzz::harfbuzz)
target_include_directories(caret_harness PRIVATE
    src ${CMAKE_CURRENT_SOURCE_DIR}/../mfl/include)

# Enable testing
enable_testing()

# Smoke-тест верстки как ctest (шрифты ищутся в каталоге запуска)
add_test(NAME test_layout
         COMMAND test_layout
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Add tests subdirectory
add_subdirectory(tests)
