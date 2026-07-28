# Formula Widget Tests
cmake_minimum_required(VERSION 3.16)

# Find Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)

# Create test executable for widget tests.
# Тесты создают настоящий FormulaWidget, поэтому нужны исходники виджета и
# зависимости FreeType/HarfBuzz (см. target_* ниже).
add_executable(formula_widget_tests
    main.cpp
    formula_widget_test.cpp
    ../src/formula_widget.cpp
    ../src/formula_cursor.cpp
    ../src/ft_library.cpp
    ../src/ft_font_face.cpp
    ../src/math_ast.cpp
    ../src/ast_cursor.cpp
)

# Create test executable for cursor tests
add_executable(formula_cursor_tests
    test_cursor.cpp
    ../src/formula_cursor.cpp
)

# Create test executable for navigation tests
add_executable(navigation_tests
    test_navigation.cpp
    ../src/formula_cursor.cpp
)

# Create test executable for coordinate conversion tests
add_executable(coordinate_conversion_tests
    test_coordinate_conversion.cpp
)

# Create test executable for TexCursorManager tests
add_executable(tex_cursor_manager_tests
    test_tex_cursor_manager.cpp
    ../src/tex_cursor_manager.cpp
)

# Create test executable for AST cursor tests (standalone, no Qt/mfl)
add_executable(ast_cursor_tests
    test_ast_cursor_standalone.cpp
    ../src/math_ast.cpp
    ../src/ast_cursor.cpp
)
target_include_directories(ast_cursor_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Семантика: MathNode -> SymPy (чистый C++, без Qt/mfl)
add_executable(math_semantics_tests
    test_math_semantics.cpp
    ../src/math_ast.cpp
    ../src/math_semantics.cpp
)
target_include_directories(math_semantics_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# End-to-end тест: AST -> TeX -> mfl (с настоящими шрифтами STIX2, без Qt)
add_executable(ast_mfl_integration_tests
    test_ast_mfl_integration.cpp
    ../src/math_ast.cpp
    ../src/ast_cursor.cpp
    ../src/ft_library.cpp
    ../src/ft_font_face.cpp
)
target_include_directories(ast_mfl_integration_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mfl/include
)
target_link_libraries(ast_mfl_integration_tests PRIVATE
    mfl
    Freetype::Freetype
    harfbuzz::harfbuzz
)

# Link libraries for widget tests
target_link_libraries(formula_widget_tests
    PRIVATE
        mfl
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
        Freetype::Freetype
        harfbuzz::harfbuzz
)

target_include_directories(formula_widget_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mfl/include
)

# Link libraries for cursor tests
target_link_libraries(formula_cursor_tests
    PRIVATE
        mfl
        Qt6::Core
        Qt6::Test
)

# Link libraries for navigation tests
target_link_libraries(navigation_tests
    PRIVATE
        mfl
        Qt6::Core
        Qt6::Test
)

# Link libraries for coordinate conversion tests
target_link_libraries(coordinate_conversion_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

# Link libraries for TexCursorManager tests
target_link_libraries(tex_cursor_manager_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

# Include directories
target_include_directories(formula_cursor_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../mfl/include
)

target_include_directories(navigation_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../mfl/include
)

target_include_directories(coordinate_conversion_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_include_directories(tex_cursor_manager_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Add tests
# Виджет-тесты создают QWidget и грузят шрифты: offscreen-платформа + каталог с fonts/
add_test(NAME formula_widget_tests
         COMMAND formula_widget_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(formula_widget_tests PROPERTIES
         ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME formula_cursor_tests COMMAND formula_cursor_tests)
add_test(NAME navigation_tests COMMAND navigation_tests)
add_test(NAME coordinate_conversion_tests COMMAND coordinate_conversion_tests)
add_test(NAME tex_cursor_manager_tests COMMAND tex_cursor_manager_tests)
add_test(NAME ast_cursor_tests COMMAND ast_cursor_tests)
add_test(NAME math_semantics_tests COMMAND math_semantics_tests)
add_test(NAME ast_mfl_integration_tests
         COMMAND ast_mfl_integration_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
