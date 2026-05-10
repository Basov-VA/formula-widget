#include "ft_library.hpp"

#include <filesystem>
#include <stdexcept>
#include <format>

namespace fw
{
    namespace
    {
        FT_Library load_library()
        {
            FT_Library result = nullptr;
            if (const auto err = FT_Init_FreeType(&result); err != 0)
            {
                throw std::runtime_error(std::format("Could not initialize FreeType library. Error code: {}", err));
            }
            return result;
        }

        std::string get_fonts_directory()
        {
            // Try to find fonts directory relative to executable
            const auto current_path = std::filesystem::current_path();

            // Check common locations for fonts
            const std::vector<std::string> font_paths = {
                "fonts",
                "UIR/formula_widget/fonts",
                "../formula_widget/fonts",
                "../../formula_widget/fonts"
            };

            for (const auto& path : font_paths)
            {
                if (std::filesystem::exists(path))
                {
                    return path;
                }
            }

            // If not found, return current path as fallback
            return current_path.string();
        }
    }

    FtLibrary::FtLibrary()
        : lib_(load_library(), FTLibraryDeleter{})
        , fonts_dir_(get_fonts_directory())
    {
    }

    FtLibrary::~FtLibrary() = default;

    FT_Face FtLibrary::load_face(const mfl::font_family family) const
    {
        const std::string font_file = [family]() -> std::string {
            switch (family)
            {
                case mfl::font_family::roman:
                    return "STIX2Math.otf";
                case mfl::font_family::italic:
                    return "STIX2Text-Italic.otf";
                case mfl::font_family::bold:
                    return "STIX2Text-Bold.otf";
                case mfl::font_family::mono:
                    return "DejaVuSansMono.ttf";
                case mfl::font_family::sans:
                    return "STIX2Text-Regular.otf";  // Using regular as sans
                default:
                    return "STIX2Math.otf";
            }
        }();

        const auto file_path = std::filesystem::path(fonts_dir_) / font_file;
        if (!std::filesystem::exists(file_path))
        {
            throw std::runtime_error(std::format("Font file not found: {}", file_path.string()));
        }

        FT_Face face = nullptr;
        if (const auto err = FT_New_Face(lib_.get(), file_path.string().c_str(), 0, &face); err != 0)
        {
            throw std::runtime_error(std::format("Could not load font face from {}. Error code: {}",
                                               file_path.string(), err));
        }

        return face;
    }
}
