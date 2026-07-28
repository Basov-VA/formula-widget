#include <iostream>
#include <string>

int main() {
    std::string test1 = "\\frac{b}{c}";
    std::cout << "test1 = \"\\\\frac{b}{c}\"" << std::endl;
    std::cout << "Length: " << test1.length() << std::endl;
    for (size_t i = 0; i < test1.length(); i++) {
        std::cout << "Position " << i << ": '" << test1[i] << "'" << std::endl;
    }
    std::cout << std::endl;

    std::string test2 = "\\sqrt{x}";
    std::cout << "test2 = \"\\\\sqrt{x}\"" << std::endl;
    std::cout << "Length: " << test2.length() << std::endl;
    for (size_t i = 0; i < test2.length(); i++) {
        std::cout << "Position " << i << ": '" << test2[i] << "'" << std::endl;
    }
    std::cout << std::endl;

    std::string test3 = "a \\frac {b} {c} d";
    std::cout << "test3 = \"a \\\\frac {b} {c} d\"" << std::endl;
    std::cout << "Length: " << test3.length() << std::endl;
    for (size_t i = 0; i < test3.length(); i++) {
        std::cout << "Position " << i << ": '" << test3[i] << "'" << std::endl;
    }

    return 0;
}
