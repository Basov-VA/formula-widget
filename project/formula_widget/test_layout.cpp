// Небольшой смоук-тест верстки mfl с настоящими шрифтами STIX2.
//
// Раньше использовал mock-шрифт mfl (create_mock_font_face), который доступен
// только при mfl_BUILD_FONTS_FOR_TESTS=ON. Переведён на реальный FtFontFace,
// чтобы собираться и работать в обычной конфигурации.
//
// Запускать из каталога formula_widget/ (там лежит папка fonts/).

#include "mfl/layout.hpp"
#include "mfl/units.hpp"
#include "mfl/font_family.hpp"

#include "src/ft_library.hpp"
#include "src/ft_font_face.hpp"

#include <iostream>
#include <memory>

using namespace mfl;
using namespace mfl::units_literals;

int main()
{
    try
    {
        fw::FtLibrary lib;
        const auto result = layout(R"(\frac{1}{x+y})", 10_pt,
            [&lib](const font_family fam) -> std::unique_ptr<abstract_font_face> {
                auto face = std::make_unique<fw::FtFontFace>(fam, lib);
                face->set_size(10_pt);
                return face;
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

        return (result.glyphs.empty()) ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
