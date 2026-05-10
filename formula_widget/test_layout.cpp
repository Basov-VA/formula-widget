#include "mfl/layout.hpp"
#include "mfl/units.hpp"
#include "mfl/font_family.hpp"
#include "../mfl/tests/unit_tests/framework/mock_font_face.hpp"

#include <iostream>

using namespace mfl;
using namespace mfl::units_literals;

int main()
{
    try
    {
        const auto result = layout(R"(\frac{1}{x+y})", 10_pt, [](font_family) {
            return create_mock_font_face(font_family::roman);
        });

        if (result.error)
        {
            std::cout << "Layout error: " << result.error.value() << std::endl;
            return 1;
        }

        std::cout << "Layout successful!" << std::endl;
        std::cout << "Width: " << result.width << std::endl;
        std::cout << "Height: " << result.height << std::endl;
        std::cout << "Glyphs: " << result.glyphs.size() << std::endl;
        std::cout << "Lines: " << result.lines.size() << std::endl;

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
