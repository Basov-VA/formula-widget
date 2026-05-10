#include "ft_font_face.hpp"

#include <algorithm>
#include <ranges>
#include <format>

namespace fw
{
    namespace
    {
        using namespace mfl::units_literals;

        constexpr auto max_number_of_variants = 20;
        constexpr auto max_number_of_parts = 8;
    }

    FtFontFace::FtFontFace(const mfl::font_family family, const FtLibrary& lib)
        : ft_face_(lib.load_face(family), FTFaceDeleter{})
        , family_(family)
    {
    }

    FtFontFace::~FtFontFace() = default;

    std::int32_t FtFontFace::font_units_to_dist(const int u)
    {
        return u * font_unit_factor;
    }

    mfl::math_constants FtFontFace::constants() const
    {
        // Create a fresh hb_font from the current FT face state so that
        // hb_ot_math_get_constant returns values in the same 26.6 scaled units
        // as FreeType's glyph metrics (consistent with glyph_info).
        std::unique_ptr<hb_font_t, HBFontDeleter> hb_font(hb_ft_font_create(ft_face_.get(), nullptr), HBFontDeleter{});
        const auto get_constant = [&](const hb_ot_math_constant_t c) {
            return font_units_to_dist(hb_ot_math_get_constant(hb_font.get(), c));
        };

        // The standard values for non-display style fraction shifts are huge so we divide them
        // by four here to get a more typical non-display style tex aesthetic
        const auto default_fraction_params =
            mfl::fraction_constants{get_constant(HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_SHIFT_UP) / 4,
                                   get_constant(HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_SHIFT_DOWN) / 4,
                                   get_constant(HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_GAP_MIN),
                                   get_constant(HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_GAP_MIN)};

        const auto display_style_fraction_params =
            mfl::fraction_constants{get_constant(HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP),
                                   get_constant(HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN),
                                   get_constant(HB_OT_MATH_CONSTANT_FRACTION_NUM_DISPLAY_STYLE_GAP_MIN),
                                   get_constant(HB_OT_MATH_CONSTANT_FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN)};

        const auto default_atop_params = mfl::fraction_constants{get_constant(HB_OT_MATH_CONSTANT_STACK_TOP_SHIFT_UP),
                                                                 get_constant(HB_OT_MATH_CONSTANT_STACK_BOTTOM_SHIFT_DOWN),
                                                                 get_constant(HB_OT_MATH_CONSTANT_STACK_GAP_MIN)};

        const auto display_style_atop_params =
            mfl::fraction_constants{get_constant(HB_OT_MATH_CONSTANT_STACK_TOP_DISPLAY_STYLE_SHIFT_UP),
                                   get_constant(HB_OT_MATH_CONSTANT_STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN),
                                   get_constant(HB_OT_MATH_CONSTANT_STACK_DISPLAY_STYLE_GAP_MIN)};

        return {.axis_height = get_constant(HB_OT_MATH_CONSTANT_AXIS_HEIGHT),
                .fraction_rule_thickness = get_constant(HB_OT_MATH_CONSTANT_FRACTION_RULE_THICKNESS),
                .subscript_drop = get_constant(HB_OT_MATH_CONSTANT_SUBSCRIPT_BASELINE_DROP_MIN),
                .subscript_shift_down = get_constant(HB_OT_MATH_CONSTANT_SUBSCRIPT_SHIFT_DOWN),
                .superscript_drop = get_constant(HB_OT_MATH_CONSTANT_SUPERSCRIPT_BASELINE_DROP_MAX),
                .superscript_shift_up = get_constant(HB_OT_MATH_CONSTANT_SUPERSCRIPT_SHIFT_UP),
                .superscript_shift_up_cramped = get_constant(HB_OT_MATH_CONSTANT_SUPERSCRIPT_SHIFT_UP_CRAMPED),
                .minimum_dual_script_gap = get_constant(HB_OT_MATH_CONSTANT_SUB_SUPERSCRIPT_GAP_MIN),
                .maximum_superscript_bottom_in_dual_script =
                    get_constant(HB_OT_MATH_CONSTANT_SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT),
                .space_after_script = get_constant(HB_OT_MATH_CONSTANT_SPACE_AFTER_SCRIPT),
                .radical_vertical_gap = get_constant(HB_OT_MATH_CONSTANT_RADICAL_VERTICAL_GAP),
                .radical_rule_thickness = get_constant(HB_OT_MATH_CONSTANT_RADICAL_RULE_THICKNESS),
                .radical_extra_ascender = get_constant(HB_OT_MATH_CONSTANT_RADICAL_EXTRA_ASCENDER),
                .radical_kern_before_degree = get_constant(HB_OT_MATH_CONSTANT_RADICAL_KERN_BEFORE_DEGREE),
                .radical_kern_after_degree = get_constant(HB_OT_MATH_CONSTANT_RADICAL_KERN_AFTER_DEGREE),
                .radical_degree_bottom_raise_percent =
                    hb_ot_math_get_constant(hb_font.get(), HB_OT_MATH_CONSTANT_RADICAL_DEGREE_BOTTOM_RAISE_PERCENT),
                .overline_gap = get_constant(HB_OT_MATH_CONSTANT_OVERBAR_VERTICAL_GAP),
                .overline_padding = get_constant(HB_OT_MATH_CONSTANT_OVERBAR_EXTRA_ASCENDER),
                .overline_thickness = get_constant(HB_OT_MATH_CONSTANT_OVERBAR_RULE_THICKNESS),
                .underline_gap = get_constant(HB_OT_MATH_CONSTANT_UNDERBAR_VERTICAL_GAP),
                .underline_padding = get_constant(HB_OT_MATH_CONSTANT_UNDERBAR_EXTRA_DESCENDER),
                .underline_thickness = get_constant(HB_OT_MATH_CONSTANT_UNDERBAR_RULE_THICKNESS),
                .lower_limit_min_gap = get_constant(HB_OT_MATH_CONSTANT_LOWER_LIMIT_GAP_MIN),
                .upper_limit_min_gap = get_constant(HB_OT_MATH_CONSTANT_UPPER_LIMIT_GAP_MIN),
                .default_fraction = default_fraction_params,
                .display_style_fraction = display_style_fraction_params,
                .default_atop = default_atop_params,
                .display_style_atop = display_style_atop_params};
    }

    mfl::math_glyph_info FtFontFace::glyph_info(const std::size_t glyph_index) const
    {
        std::unique_ptr<hb_font_t, HBFontDeleter> hb_font(hb_ft_font_create(ft_face_.get(), nullptr), HBFontDeleter{});
        FT_Load_Char(ft_face_.get(), static_cast<FT_ULong>(glyph_index), FT_LOAD_DEFAULT);
        const auto& metrics = ft_face_->glyph->metrics;

        const auto glyph_codepoint = static_cast<hb_codepoint_t>(glyph_index);
        const auto italic_correction = hb_ot_math_get_glyph_italics_correction(hb_font.get(), glyph_codepoint);

        // TODO - for the StixFonts, the horizontal advance seems to include the italic correction. This is
        // generally exactly what we want and expect (positioning a tall non-slanted character after a
        // slanted character should take that italic correction into account). We can clearly see that
        // the italic correction is already in the horizontal advance on tall slanted symbols like
        // integrals, but if we subtract the italic correction
        // from the width on all symbols then superscripts are drawn too close to the top right corner of
        // italic base symbols. So we only subtract the italic correction for tall slanted symbols. This
        // appears to means that when taking, say, an italic X, the italic correction is already in the
        // width and then when positioning a superscript on that X the italic correction is applied again,
        // but this prevents superscripts from overlapping with italic symbols in the nucleus.
        const auto integral_indices = std::array{1699, 1705, 1711, 1717, 1723, 1729};
        const auto integral_fix = std::find(integral_indices.begin(), integral_indices.end(), glyph_index) != integral_indices.end() ? -italic_correction : 0;

        hb_glyph_extents_t extents;
        hb_font_get_glyph_extents_for_origin(hb_font.get(), glyph_codepoint, HB_DIRECTION_LTR,
                                             &extents);  // TODO what does the boolean return tell us?

        const auto height = metrics.horiBearingY;
        const auto depth =
            metrics.height - height;  // metrics height is total glyph height - our height is height from the baseline
        return {glyph_index,
                font_units_to_dist(static_cast<int>(metrics.horiAdvance + integral_fix)),
                font_units_to_dist(static_cast<int>(height)),
                font_units_to_dist(static_cast<int>(depth)),
                font_units_to_dist(italic_correction),
                font_units_to_dist(hb_ot_math_get_glyph_top_accent_attachment(hb_font.get(), glyph_codepoint))};
    }

    std::size_t FtFontFace::glyph_index_from_code_point(const mfl::code_point char_code, const bool use_large_variant) const
    {
        // hard wired values from the stylistic set 04 in Stix2Math which contains nicer looking primes
        constexpr auto prime_index = 8242U;
        constexpr auto offset_to_better_primes = 6792U;
        if ((char_code >= prime_index) && (char_code < prime_index + 3)) return char_code - offset_to_better_primes;

        std::unique_ptr<hb_font_t, HBFontDeleter> hb_font(hb_ft_font_create(ft_face_.get(), nullptr), HBFontDeleter{});
        hb_codepoint_t base_index = 0;
        hb_font_get_glyph(hb_font.get(), char_code, 0, &base_index);

        return base_index + (use_large_variant ? 1 : 0);
    }

    std::vector<mfl::size_variant> FtFontFace::get_size_variants(const std::size_t glyph_index,
                                                                 const hb_direction_t dir) const
    {
        std::unique_ptr<hb_font_t, HBFontDeleter> hb_font(hb_ft_font_create(ft_face_.get(), nullptr), HBFontDeleter{});
        auto variants = std::array<hb_ot_math_glyph_variant_t, max_number_of_variants>{};
        std::uint32_t num_variants = max_number_of_variants;
        const auto glyph_codepoint = static_cast<hb_codepoint_t>(glyph_index);
        hb_ot_math_get_glyph_variants(hb_font.get(), glyph_codepoint, dir, 0, &num_variants, variants.data());

        if (num_variants == 0) return {};

        const auto to_size_variant = [&](const hb_ot_math_glyph_variant_t& v) {
            hb_glyph_extents_t extents;
            hb_font_get_glyph_extents(hb_font.get(), v.glyph, &extents);
            const auto size = (dir == HB_DIRECTION_LTR) ? extents.width : extents.height;
            return mfl::size_variant{.glyph_index = v.glyph, .size = font_units_to_dist(std::abs(size))};
        };

        // todo: clang on ubuntu 24.04 cannot compile this correctly ...
        // return variants                                  //
        //       | std::views::take(num_variants)          //
        //       | std::views::transform(to_size_variant)  //
        //       | std::ranges::to<std::vector>();

        // ...so we have to do this instead:
        auto result = std::vector<mfl::size_variant>(num_variants);
        std::ranges::copy(variants                              //
                          | std::views::take(num_variants)  //
                          | std::views::transform(to_size_variant),
                          result.begin());
        return result;
    }

    std::vector<mfl::size_variant> FtFontFace::horizontal_size_variants(const mfl::code_point char_code) const
    {
        const auto glyph_index = glyph_index_from_code_point(char_code, false);
        return get_size_variants(glyph_index, HB_DIRECTION_LTR);
    }

    std::vector<mfl::size_variant> FtFontFace::vertical_size_variants(const mfl::code_point char_code) const
    {
        const auto glyph_index = glyph_index_from_code_point(char_code, false);
        return get_size_variants(glyph_index, HB_DIRECTION_BTT);
    }

    std::optional<mfl::glyph_assembly> FtFontFace::get_assembly(const std::size_t glyph_index,
                                                                const hb_direction_t dir) const
    {
        std::unique_ptr<hb_font_t, HBFontDeleter> hb_font(hb_ft_font_create(ft_face_.get(), nullptr), HBFontDeleter{});
        auto parts = std::array<hb_ot_math_glyph_part_t, max_number_of_parts>{};
        std::uint32_t num_parts = max_number_of_parts;
        hb_position_t italic_correction = 0;
        const auto glyph_codepoint = static_cast<hb_codepoint_t>(glyph_index);
        hb_ot_math_get_glyph_assembly(hb_font.get(), glyph_codepoint, dir, 0, &num_parts, parts.data(), &italic_correction);

        if (num_parts == 0) return std::nullopt;

        const auto to_part = [&](const hb_ot_math_glyph_part_t& p) {
            return mfl::glyph_part{.glyph_index = p.glyph,
                                  .start_connector_length = font_units_to_dist(p.start_connector_length),
                                  .end_connector_length = font_units_to_dist(p.end_connector_length),
                                  .full_advance = font_units_to_dist(p.full_advance),
                                  .is_extender = (p.flags & HB_OT_MATH_GLYPH_PART_FLAG_EXTENDER) != 0};
        };

        // todo: clang on ubuntu 24.04 cannot compile this correctly ...
        // return glyph_assembly{.parts = parts                             //
        //                               | std::views::take(num_parts)     //
        //                               | std::views::transform(to_part)  //
        //                               | std::views::reverse             //
        //                               | std::ranges::to<std::vector>(),
        //                      .italic_correction = font_units_to_dist(italic_correction)};

        // ...so we have to do this instead:
        auto assembly_parts = std::vector<mfl::glyph_part>(num_parts);
        std::ranges::copy(parts                                 //
                          | std::views::take(num_parts)     //
                          | std::views::transform(to_part)  //
                          | std::views::reverse,
                          assembly_parts.begin());
        return mfl::glyph_assembly{.parts = assembly_parts, .italic_correction = font_units_to_dist(italic_correction)};
    }

    std::optional<mfl::glyph_assembly> FtFontFace::horizontal_assembly(const mfl::code_point char_code) const
    {
        const auto glyph_index = glyph_index_from_code_point(char_code, false);
        return get_assembly(glyph_index, HB_DIRECTION_LTR);
    }

    std::optional<mfl::glyph_assembly> FtFontFace::vertical_assembly(const mfl::code_point char_code) const
    {
        const auto glyph_index = glyph_index_from_code_point(char_code, false);
        return get_assembly(glyph_index, HB_DIRECTION_BTT);
    }

    void FtFontFace::set_size(const mfl::points size)
    {
        const auto fp_size = static_cast<FT_F26Dot6>(size.value() * 64.0);
        if (const auto err = FT_Set_Char_Size(ft_face_.get(), fp_size, fp_size, 0, 0); err != 0)
        {
            throw std::invalid_argument(std::format("Could not set font size. Error code: {}", err));
        }
    }

    mfl::abstract_font_face* FtFontFace::clone() const
    {
        // For this implementation, we'll create a new instance with the same family
        // Note: This is not ideal as it creates a new FT_Face, but it works for now
        // In a production implementation, you'd want to share the FT_Face resources
        static FtLibrary static_lib;  // Static library instance
        return new FtFontFace(family_, static_lib);
    }

    int FtFontFace::units_per_em() const
    {
        return ft_face_->units_per_EM;
    }

    std::vector<FtFontFace::OutlinePoint> FtFontFace::get_glyph_outline_path(std::size_t glyph_index) const
    {
        if (FT_Load_Glyph(ft_face_.get(), static_cast<FT_UInt>(glyph_index), FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP) != 0)
            return {};
        if (ft_face_->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
            return {};

        struct CallbackData {
            std::vector<OutlinePoint>* points;
        };

        std::vector<OutlinePoint> result;
        CallbackData data{&result};

        FT_Outline_Funcs funcs;
        funcs.move_to = [](const FT_Vector* to, void* user) -> int {
            auto* d = static_cast<CallbackData*>(user);
            d->points->push_back({0, static_cast<double>(to->x), static_cast<double>(to->y)});
            return 0;
        };
        funcs.line_to = [](const FT_Vector* to, void* user) -> int {
            auto* d = static_cast<CallbackData*>(user);
            d->points->push_back({1, static_cast<double>(to->x), static_cast<double>(to->y)});
            return 0;
        };
        funcs.conic_to = [](const FT_Vector* control, const FT_Vector* to, void* user) -> int {
            auto* d = static_cast<CallbackData*>(user);
            d->points->push_back({2, static_cast<double>(control->x), static_cast<double>(control->y)});
            d->points->push_back({6, static_cast<double>(to->x), static_cast<double>(to->y)});
            return 0;
        };
        funcs.cubic_to = [](const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) -> int {
            auto* d = static_cast<CallbackData*>(user);
            d->points->push_back({3, static_cast<double>(control1->x), static_cast<double>(control1->y)});
            d->points->push_back({4, static_cast<double>(control2->x), static_cast<double>(control2->y)});
            d->points->push_back({5, static_cast<double>(to->x), static_cast<double>(to->y)});
            return 0;
        };
        funcs.shift = 0;
        funcs.delta = 0;

        FT_Outline_Decompose(&ft_face_->glyph->outline, &funcs, &data);
        return result;
    }

    std::vector<std::pair<double, double>> FtFontFace::get_glyph_outline(std::size_t glyph_index) const
    {
        // Load the glyph without scaling to get raw font units
        // FT_LOAD_NO_SCALE returns coordinates in font design units (e.g. 1000 units/EM for STIX2)
        if (FT_Load_Glyph(ft_face_.get(), static_cast<FT_UInt>(glyph_index), FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP) != 0)
        {
            return {};  // Return empty vector if glyph loading fails
        }

        if (ft_face_->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
            return {};

        // Get the glyph outline
        FT_Outline* outline = &ft_face_->glyph->outline;
        std::vector<std::pair<double, double>> points;
        points.reserve(outline->n_points);

        // Extract points from the outline
        // FreeType outline points are in font units with Y-up convention
        for (int i = 0; i < outline->n_points; ++i)
        {
            FT_Vector point = outline->points[i];
            points.emplace_back(static_cast<double>(point.x), static_cast<double>(point.y));
        }

        return points;
    }
}
