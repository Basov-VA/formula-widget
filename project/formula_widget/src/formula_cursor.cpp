#include "formula_cursor.hpp"
#include <limits>
#include <cmath>

namespace formula {

    void FormulaCursor::setLayout(const mfl::layout_elements* layout) {
        layout_ = layout;
        current_highlight_.reset();
    }

    double FormulaCursor::distanceToBBox(double px, double py, const mfl::shaped_glyph& g) {

        double left   = g.x.value();
        double right  = g.x.value() + g.advance.value();
        double bottom = g.y.value() - g.depth.value();
        double top    = g.y.value() + g.height.value();

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

    std::optional<std::size_t> FormulaCursor::findGlyphInDirection(
        std::size_t current_index, NavigationDirection direction) const
    {
        if (!layout_ || layout_->glyphs.empty()) return std::nullopt;
        if (current_index >= layout_->glyphs.size()) return std::nullopt;

        const auto [cx, cy] = glyphBBoxCenter(layout_->glyphs[current_index]);

        std::optional<std::size_t> best_index;

        std::optional<std::size_t> same_line_candidate;
        double same_line_score = std::numeric_limits<double>::max();

        std::optional<std::size_t> other_line_candidate;
        double other_line_score = std::numeric_limits<double>::max();

        for (std::size_t i = 0; i < layout_->glyphs.size(); ++i) {
            if (i == current_index) continue;

            const auto [gx, gy] = glyphBBoxCenter(layout_->glyphs[i]);
            double dx = gx - cx;
            double dy = gy - cy;

            bool is_candidate = false;
            double score = 0.0;

            switch (direction) {
                case NavigationDirection::Left:
                    is_candidate = (dx < -1e-6);
                    score = std::abs(dx);
                    break;
                case NavigationDirection::Right:
                    is_candidate = (dx > 1e-6);
                    score = std::abs(dx);
                    break;
                case NavigationDirection::Up:
                    is_candidate = (dy > 1e-6);
                    score = std::abs(dy);
                    break;
                case NavigationDirection::Down:
                    is_candidate = (dy < -1e-6);
                    score = std::abs(dy);
                    break;
            }

            if (is_candidate) {

                bool is_same_line = false;
                switch (direction) {
                    case NavigationDirection::Left:
                    case NavigationDirection::Right:
                        is_same_line = (std::abs(dy) < 1e-3);
                        break;
                    case NavigationDirection::Up:
                    case NavigationDirection::Down:
                        is_same_line = (std::abs(dx) < 1e-3);
                        break;
                }

                if (is_same_line) {

                    if (score < same_line_score) {
                        same_line_score = score;
                        same_line_candidate = i;
                    }
                } else {

                    double penalized_score = score + 1000.0 * (std::abs(dy) + std::abs(dx));
                    if (penalized_score < other_line_score) {
                        other_line_score = penalized_score;
                        other_line_candidate = i;
                    }
                }
            }
        }

        if (same_line_candidate) {
            return same_line_candidate;
        }

        return other_line_candidate;

        return best_index;
    }

    bool FormulaCursor::moveToDirection(NavigationDirection direction) {
        if (!current_highlight_) return false;

        auto target = findGlyphInDirection(current_highlight_->glyph_index, direction);
        if (!target) return false;

        setGlyphIndex(*target);
        return true;
    }

    void FormulaCursor::setGlyphIndex(std::size_t index) {
        if (!layout_ || index >= layout_->glyphs.size()) return;

        const auto& g = layout_->glyphs[index];
        current_highlight_ = glyph_hit_result{
            .glyph_index = index,
            .distance = 0.0,
            .bbox_left   = g.x,
            .bbox_top    = g.y + g.height,
            .bbox_right  = g.x + g.advance,
            .bbox_bottom = g.y - g.depth,
        };
    }

    std::optional<std::size_t> FormulaCursor::currentGlyphIndex() const {
        if (current_highlight_) return current_highlight_->glyph_index;
        return std::nullopt;
    }

}
