#include "tex_cursor_manager.hpp"
#include <iostream>

int main() {
    formula::TexCursorManager mgr;

    // Test 1: Simple insertion
    mgr.setTexString("abc");
    std::cout << "Test 1 - Initial string: '" << mgr.texString() << "'" << std::endl;
    std::cout << "Test 1 - Cursor position: " << mgr.cursorPosition() << std::endl;

    mgr.setCursorPosition(1);
    std::cout << "Test 1 - Cursor after set to 1: " << mgr.cursorPosition() << std::endl;

    mgr.insertChar('x');
    std::cout << "Test 1 - After inserting 'x': '" << mgr.texString() << "'" << std::endl;
    std::cout << "Test 1 - Cursor after insert: " << mgr.cursorPosition() << std::endl;
    std::cout << std::endl;

    // Test 2: Complex formula
    mgr.setTexString("\\frac{1}{2}");
    std::cout << "Test 2 - Initial string: '" << mgr.texString() << "'" << std::endl;
    std::cout << "Test 2 - Cursor position: " << mgr.cursorPosition() << std::endl;

    mgr.setCursorPosition(6);
    std::cout << "Test 2 - Cursor after set to 6: " << mgr.cursorPosition() << std::endl;

    mgr.insertChar('x');
    std::cout << "Test 2 - After inserting 'x': '" << mgr.texString() << "'" << std::endl;
    std::cout << "Test 2 - Cursor after insert: " << mgr.cursorPosition() << std::endl;

    return 0;
}
