#include <iostream>
#include <string>

int main() {
    std::string test1 = "x+\\beta";
    std::cout << "test1 = \"x+\\\\beta\"" << std::endl;
    std::cout << "Length: " << test1.length() << std::endl;
    for (size_t i = 0; i < test1.length(); i++) {
        std::cout << "Position " << i << ": '" << test1[i] << "'" << std::endl;
    }
    std::cout << std::endl;

    std::string test2 = "\\fra";
    std::cout << "test2 = \"\\\\fra\"" << std::endl;
    std::cout << "Length: " << test2.length() << std::endl;
    for (size_t i = 0; i < test2.length(); i++) {
        std::cout << "Position " << i << ": '" << test2[i] << "'" << std::endl;
    }
    std::cout << std::endl;

    std::string test3 = "\\alpha beta";
    std::cout << "test3 = \"\\\\alpha beta\"" << std::endl;
    std::cout << "Length: " << test3.length() << std::endl;
    for (size_t i = 0; i < test3.length(); i++) {
        std::cout << "Position " << i << ": '" << test3[i] << "'" << std::endl;
    }

    return 0;
}
