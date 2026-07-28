#pragma once

#include "mfl/layout.hpp"
#include <optional>
#include <cstddef>

namespace formula {

    enum class NavigationDirection {
        Left,
        Right,
        Up,
        Down
    };

    struct glyph_hit_result {
        std::size_t glyph_index;
        double distance;

        mfl::points bbox_left;
        mfl::points bbox_top;
        mfl::points bbox_right;
        mfl::points bbox_bottom;
    };

    class FormulaCursor {
    public:
        FormulaCursor() = default;

        void setLayout(const mfl::layout_elements* layout);

        std::optional<glyph_hit_result> findNearestGlyph(mfl::points x, mfl::points y) const;

        void setPosition(mfl::points x, mfl::points y);

        std::optional<glyph_hit_result> currentHighlight() const;

        void clearHighlight();

        bool hasHighlight() const;

        static double distanceToBBox(double px, double py, const mfl::shaped_glyph& g);

        bool moveToDirection(NavigationDirection direction);

        std::optional<std::size_t> currentGlyphIndex() const;

        void setGlyphIndex(std::size_t index);

        std::optional<std::size_t> findGlyphInDirection(
            std::size_t current_index, NavigationDirection direction) const;

    private:
        const mfl::layout_elements* layout_ = nullptr;
        std::optional<glyph_hit_result> current_highlight_;

        static std::pair<double, double> glyphBBoxCenter(const mfl::shaped_glyph& g);
    };

}
