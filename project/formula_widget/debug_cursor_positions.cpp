#include <iostream>
#include <string>
#include <optional>
#include "tex_cursor_manager.hpp"

void debugCursorPositions(const std::string& tex_string, std::size_t test_position) {
    formula::TexCursorManager mgr;
    mgr.setTexString(tex_string);

    std::cout << "String: \"" << tex_string << "\" (length: " << tex_string.length() << ")" << std::endl;

    for (std::size_t i = 0; i <= tex_string.length(); i++) {
        mgr.setCursorPosition(i);
        bool inside = mgr.isInsideTexCommand();
        std::cout << "Position " << i << ": '";
        if (i < tex_string.length()) {
            std::cout << tex_string[i];
        } else {
            std::cout << "END";
        }
        std::cout << "' - Inside command: " << (inside ? "YES" : "NO") << std::endl;
    }

    // Test the specific position from the failing test
    mgr.setCursorPosition(test_position);
    std::cout << "\nTest position " << test_position << ": Inside command: "
              << (mgr.isInsideTexCommand() ? "YES" : "NO") << std::endl;
    std::cout << "Can insert: " << (mgr.insertChar('+') ? "YES" : "NO") << std::endl;
    std::cout << "Result: \"" << mgr.texString() << "\"" << std::endl;
}

int main() {
    std::cout << "=== Test 1: \\alphabeta (position 6) ===" << std::endl;
    debugCursorPositions("\\alphabeta", 6);

    std::cout << "\n=== Test 2: x+\\beta (position 7) ===" << std::endl;
    debugCursorPositions("x+\\beta", 7);

    std::cout << "\n=== Test 3: \\fra (position 4) ===" << std::endl;
    debugCursorPositions("\\fra", 4);

    return 0;
}
