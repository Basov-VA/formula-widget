#include <iostream>
#include <string>

int main() {
    std::string test = "\\frac{b}{c}";
    std::cout << "Original: " << test << std::endl;
    std::cout << "Length: " << test.length() << std::endl;

    // Test insertion at position 7 (after '}')
    test.insert(7, "*");
    std::cout << "After insert at 7: " << test << std::endl;

    // Test insertion at position 8 (after '}')
    test = "\\frac{b}{c}";
    test.insert(8, "*");
    std::cout << "After insert at 8: " << test << std::endl;

    return 0;
}
