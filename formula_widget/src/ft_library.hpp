#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include "mfl/font_family.hpp"

#include <memory>
#include <string>

namespace fw
{
    class FtLibrary
    {
    public:
        FtLibrary();
        ~FtLibrary();

        // Non-copyable
        FtLibrary(const FtLibrary&) = delete;
        FtLibrary& operator=(const FtLibrary&) = delete;

        // Movable
        FtLibrary(FtLibrary&&) = default;
        FtLibrary& operator=(FtLibrary&&) = default;

        [[nodiscard]] FT_Face load_face(const mfl::font_family family) const;

    private:
        struct FTLibraryDeleter
        {
            void operator()(FT_Library library) const
            {
                if (library) FT_Done_FreeType(library);
            }
        };

        std::unique_ptr<FT_LibraryRec_, FTLibraryDeleter> lib_;
        std::string fonts_dir_;
    };
}
