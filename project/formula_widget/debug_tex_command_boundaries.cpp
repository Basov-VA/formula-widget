#include "tex_cursor_manager.hpp"
#include <iostream>

int main() {
    formula::TexCursorManager mgr;

    std::cout << "=== Testing TeX command boundary issues ===" << std::endl;

    // Test 1: Insert 'a' after \frac
    std::cout << "\nTest 1: Insert 'a' after \\\\frac" << std::endl;
    mgr.setTexString("\\frac");
    mgr.setCursorPosition(5); // after 'c' in \frac

    std::cout << "Before: " << mgr.texString() << std::endl;
    std::cout << "Cursor position: " << mgr.cursorPosition() << std::endl;
    std::cout << "Is inside TeX command: " << mgr.isInsideTexCommand() << std::endl;

    bool result = mgr.insertChar('a');
    std::cout << "Insert result: " << result << std::endl;
    std::cout << "After: " << mgr.texString() << std::endl;

    // Test 2: Insert 'a' after \frac{1}{2}
    std::cout << "\nTest 2: Insert 'a' after \\\\frac{1}{2}" << std::endl;
    mgr.setTexString("\\frac{1}{2}");
    mgr.setCursorPosition(11); // at the end

    std::cout << "Before: " << mgr.texString() << std::endl;
    std::cout << "Cursor position: " << mgr.cursorPosition() << std::endl;
    std::cout << "Is inside TeX command: " << mgr.isInsideTexCommand() << std::endl;

    result = mgr.insertChar('a');
    std::cout << "Insert result: " << result << std::endl;
    std::cout << "After: " << mgr.texString() << std::endl;

    // Test 3: Insert 'a' inside \frac (after 'f')
    std::cout << "\nTest 3: Insert 'a' inside \\\\frac (after 'f')" << std::endl;
    mgr.setTexString("\\frac");
    mgr.setCursorPosition(2); // after 'f' in \frac

    std::cout << "Before: " << mgr.texString() << std::endl;
    std::cout << "Cursor position: " << mgr.cursorPosition() << std::endl;
    std::cout << "Is inside TeX command: " << mgr.isInsideTexCommand() << std::endl;

    result = mgr.insertChar('a');
    std::cout << "Insert result: " << result << std::endl;
    std::cout << "After: " << mgr.texString() << std::endl;

    return 0;
}
