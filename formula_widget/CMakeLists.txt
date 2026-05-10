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
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../mfl ${CMAKE_CURRENT_BINARY_DIR}/mfl)

# Collect source files
set(SOURCES
    src/ft_library.cpp
    src/ft_font_face.cpp
    src/formula_widget.cpp
    src/main.cpp
)

set(HEADERS
    src/ft_library.hpp
    src/ft_font_face.hpp
    src/formula_widget.hpp
)

# Create executable
add_executable(formula_widget
    ${SOURCES}
    ${HEADERS}
)

# Create test executable
add_executable(test_layout
    test_layout.cpp
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
    mfl_tests_unit_tests
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

# Enable testing
enable_testing()

# Add tests subdirectory
add_subdirectory(tests)
