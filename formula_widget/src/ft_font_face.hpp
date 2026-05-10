#pragma once

#include "ft_library.hpp"

#include "mfl/abstract_font_face.hpp"
#include "mfl/font_family.hpp"
#include "mfl/units.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>

#include <memory>

namespace fw
{
    class FtFontFace : public mfl::abstract_font_face
    {
    public:
        FtFontFace(const mfl::font_family family, const FtLibrary& lib);
        ~FtFontFace() override;

        // Non-copyable
        FtFontFace(const FtFontFace&) = delete;
        FtFontFace& operator=(const FtFontFace&) = delete;

        // Movable
        FtFontFace(FtFontFace&&) = default;
        FtFontFace& operator=(FtFontFace&&) = default;

        [[nodiscard]] mfl::math_constants constants() const override;
        [[nodiscard]] mfl::math_glyph_info glyph_info(const std::size_t glyph_index) const override;
        [[nodiscard]] std::size_t glyph_index_from_code_point(const mfl::code_point char_code,
                                                              const bool use_large_variant) const override;
        [[nodiscard]] std::vector<mfl::size_variant> horizontal_size_variants(const mfl::code_point char_code) const override;
        [[nodiscard]] std::vector<mfl::size_variant> vertical_size_variants(const mfl::code_point char_code) const override;
        [[nodiscard]] std::optional<mfl::glyph_assembly> horizontal_assembly(const mfl::code_point char_code) const override;
        [[nodiscard]] std::optional<mfl::glyph_assembly> vertical_assembly(const mfl::code_point char_code) const override;
        void set_size(const mfl::points size) override;
        [[nodiscard]] mfl::abstract_font_face* clone() const override;

        // Method to get glyph outline as QPainterPath for rendering
        // Returns outline in font units (Y-up), caller must scale and flip Y
        [[nodiscard]] std::vector<std::pair<double, double>> get_glyph_outline(std::size_t glyph_index) const;

        // Returns the number of font units per EM (needed for scaling outlines)
        [[nodiscard]] int units_per_em() const;

        // Get glyph outline as a list of contours with move/line/curve commands
        // Each element: (type, x, y) where type: 0=moveTo, 1=lineTo, 2=conicTo(ctrl), 3=cubicTo(ctrl1), 4=cubicTo(ctrl2)
        struct OutlinePoint {
            int type;  // 0=moveTo, 1=lineTo, 2=conicTo_ctrl, 3=cubicTo_ctrl1, 4=cubicTo_ctrl2, 5=cubicTo_end, 6=conicTo_end
            double x, y;
        };
        [[nodiscard]] std::vector<OutlinePoint> get_glyph_outline_path(std::size_t glyph_index) const;

    private:
        struct FTFaceDeleter
        {
            void operator()(FT_Face face) const
            {
                if (face) FT_Done_Face(face);
            }
        };

        struct HBFontDeleter
        {
            void operator()(hb_font_t* font) const
            {
                if (font) hb_font_destroy(font);
            }
        };

        std::unique_ptr<FT_FaceRec_, FTFaceDeleter> ft_face_;
        mfl::font_family family_;

        static constexpr int font_unit_factor = 1024;

        [[nodiscard]] static std::int32_t font_units_to_dist(const int u);
        [[nodiscard]] std::vector<mfl::size_variant> get_size_variants(const std::size_t glyph_index,
                                                                      const hb_direction_t dir) const;
        [[nodiscard]] std::optional<mfl::glyph_assembly> get_assembly(const std::size_t glyph_index,
                                                                     const hb_direction_t dir) const;
    };
}
