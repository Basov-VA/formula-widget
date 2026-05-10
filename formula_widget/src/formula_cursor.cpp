#include "formula_cursor.hpp"
#include <limits>
#include <cmath>

namespace formula {

    void FormulaCursor::setLayout(const mfl::layout_elements* layout) {
        layout_ = layout;
        current_highlight_.reset();
    }

    double FormulaCursor::distanceToBBox(double px, double py, const mfl::shaped_glyph& g) {
        // Bounding box глифа в mfl координатах (Y-up):
        double left   = g.x.value();
        double right  = g.x.value() + g.advance.value();
        double bottom = g.y.value() - g.depth.value();   // наинизшая точка
        double top    = g.y.value() + g.height.value();   // наивысшая точка

        // Расстояние от точки до прямоугольника:
        // dx = max(left - px, 0, px - right)
        // dy = max(bottom - py, 0, py - top)
        // distance = sqrt(dx² + dy²)
        double dx = std::max({left - px, 0.0, px - right});
        double dy = std::max({bottom - py, 0.0, py - top});
        return std::sqrt(dx * dx + dy * dy);
    }

    std::optional<glyph_hit_result> FormulaCursor::findNearestGlyph(mfl::points x, mfl::points y) const {
        if (!layout_ || layout_->glyphs.empty()) return std::nullopt;

        double px = x.value();
        double py = y.value();

        std::size_t best_index = 0;
        double best_distance = std::numeric_limits<double>::max();

        for (std::size_t i = 0; i < layout_->glyphs.size(); ++i) {
            double dist = distanceToBBox(px, py, layout_->glyphs[i]);
            if (dist < best_distance) {
                best_distance = dist;
                best_index = i;
            }
        }

        const auto& g = layout_->glyphs[best_index];
        return glyph_hit_result{
            .glyph_index = best_index,
            .distance = best_distance,
            .bbox_left   = g.x,
            .bbox_top    = g.y + g.height,
            .bbox_right  = g.x + g.advance,
            .bbox_bottom = g.y - g.depth,
        };
    }

    void FormulaCursor::setPosition(mfl::points x, mfl::points y) {
        current_highlight_ = findNearestGlyph(x, y);
    }

    std::optional<glyph_hit_result> FormulaCursor::currentHighlight() const {
        return current_highlight_;
    }

    void FormulaCursor::clearHighlight() {
        current_highlight_.reset();
    }

    bool FormulaCursor::hasHighlight() const {
        return current_highlight_.has_value();
    }

    std::pair<double, double> FormulaCursor::glyphBBoxCenter(const mfl::shaped_glyph& g) {
        double center_x = g.x.value() + g.advance.value() / 2.0;
        double center_y = g.y.value();
        return {center_x, center_y};
    }

} // namespace formula
