cmake_minimum_required(VERSION 3.14)
project(mfl)

set(CMAKE_CXX_STANDARD 23)

add_library(mfl_options INTERFACE)
include(cmake/Sanitizers.cmake)
enable_sanitizers(mfl_options)


option(mfl_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
add_library(mfl_warnings INTERFACE)
include(cmake/CompilerWarnings.cmake)
mfl_set_project_warnings(
        mfl_warnings
        ${mfl_WARNINGS_AS_ERRORS}
        ""
        ""
        ""
        "")

include(cmake/StaticAnalyzers.cmake)


add_subdirectory(src)
# Тесты mfl (doctest/FreeType/тестовые шрифты) строятся только при mfl_BUILD_TESTS.
# При использовании mfl как подпроекта (formula_widget) флаг выключается.
if (NOT DEFINED mfl_BUILD_TESTS OR mfl_BUILD_TESTS)
    if (NOT DEFINED mfl_BUILD_FONTS_FOR_TESTS OR mfl_BUILD_FONTS_FOR_TESTS)
        add_subdirectory(tests/fonts_for_tests)
    endif()
    add_subdirectory(tests/unit_tests)
    add_subdirectory(tests/approval_tests)
endif()

# Disabled fuzz tests - requires FreeType and other test dependencies
#if (UNIX AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
#    add_subdirectory(tests/fuzz_tests)
#endif ()

include(GNUInstallDirs)

install(TARGETS mfl mfl_options mfl_warnings EXPORT mfl-export LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT mfl-export DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mfl NAMESPACE mfl:: FILE mfl-config.cmake)

if (CMAKE_BUILD_TYPE STREQUAL Release)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN *.hpp)
endif ()
