# Formula Widget Tests
cmake_minimum_required(VERSION 3.16)

# Find Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)

# Create test executable
add_executable(formula_widget_tests
    main.cpp
    formula_widget_test.cpp
)

# Link libraries
target_link_libraries(formula_widget_tests
    PRIVATE
        mfl
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
)

# Add test
add_test(NAME formula_widget_tests COMMAND formula_widget_tests)
