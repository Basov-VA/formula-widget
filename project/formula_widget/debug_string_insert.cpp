#include <iostream>
#include <string>

int main() {
    std::string tex = "\\frac{1}{2}";
    std::size_t cursor_pos = 6;

    std::cout << "String: " << tex << std::endl;
    std::cout << "Length: " << tex.length() << std::endl;
    std::cout << "Cursor position: " << cursor_pos << std::endl;

    // Print each character with its position
    for (std::size_t i = 0; i < tex.length(); ++i) {
        std::cout << "Position " << i << ": '" << tex[i] << "' (ASCII: " << (int)tex[i] << ")" << std::endl;
    }

    // Test the insert operation
    std::cout << "\nTesting insert at position " << cursor_pos << ":" << std::endl;
    tex.insert(cursor_pos, 1, 'x');
    std::cout << "Result: " << tex << std::endl;

    return 0;
}
