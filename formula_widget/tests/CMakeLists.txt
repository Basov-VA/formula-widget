# Formula Widget Tests
cmake_minimum_required(VERSION 3.16)

# Find Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)

# Create test executable for widget tests
add_executable(formula_widget_tests
    main.cpp
    formula_widget_test.cpp
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

# Link libraries for widget tests
target_link_libraries(formula_widget_tests
    PRIVATE
        mfl
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
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

# Add tests
add_test(NAME formula_widget_tests COMMAND formula_widget_tests)
add_test(NAME formula_cursor_tests COMMAND formula_cursor_tests)
add_test(NAME navigation_tests COMMAND navigation_tests)
add_test(NAME coordinate_conversion_tests COMMAND coordinate_conversion_tests)
