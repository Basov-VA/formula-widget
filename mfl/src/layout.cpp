#include "mfl/layout.hpp"
#include "mfl/units.hpp"

#include "font_library.hpp"
#include "node/box.hpp"
#include "node/glue.hpp"
#include "node/glue_spec.hpp"
#include "node/glyph.hpp"
#include "node/hlist.hpp"
#include "node/kern.hpp"
#include "node/node.hpp"
#include "node/rule.hpp"
#include "node/vlist.hpp"
#include "node/vstack.hpp"
#include "noad/noad.hpp"
#include "parser/parse.hpp"
#include "settings.hpp"
#include "mfl/units.hpp"
#include "utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mfl
{
    namespace
    {
        using namespace units_literals;
    }
    points dist_to_points(const dist_t d)
    {
        // dist_t uses unit_distance = 65536 sp/pt (TeX scaled points)
        // so 1 pt = 65536 dist units
        return points{static_cast<double>(d) / 65536.0};
    }

    namespace
    {
        dist_t shift(const node_variant& n)
        {
            return std::visit([](const auto& node) {
                if constexpr (std::is_same_v<std::decay_t<decltype(node)>, recursive_wrapper<box>>) {
                    return static_cast<const box&>(node).shift;
                } else {
                    return dist_t{0};
                }
            }, n);
        }

        dist_t scaled_width(const node_variant& n, const double glue_scale)
        {
            return std::visit(
                overload{[&](const kern& k) { return k.size; },
                         [&](const glue_spec& g) { return static_cast<dist_t>(g.size * glue_scale); },
                         [&](const glyph& g) { return g.width; },
                         [&](const rule& r) { return r.width; },
                         [&](const recursive_wrapper<box>& wrapped_box) {
                             return static_cast<const box&>(wrapped_box).dims.width;
                         }},
                n);
        }

        // Forward declarations
        void layout_box(const box& b, const points x, const points y, layout_elements& elements, formula_node& parent);

        void layout_glyph(const glyph& g, const points x, const points y, layout_elements& elements, formula_node& parent)
        {
            elements.glyphs.push_back(shaped_glyph{.family = g.family,
                                                   .index = g.index,
                                                   .size = g.size,
                                                   .x = x,
                                                   .y = y,
                                                   .advance = dist_to_points(g.width),
                                                   .height = dist_to_points(g.height),
                                                   .depth = dist_to_points(g.depth)});
            parent.glyph_indices.push_back(elements.glyphs.size() - 1);
        }

        void layout_rule(const rule& r, const points x, const points y, layout_elements& elements, formula_node& parent)
        {
            elements.lines.push_back(
                line{.x = x, .y = y - dist_to_points(r.depth), .length = dist_to_points(r.width),
                     .thickness = dist_to_points(r.height + r.depth)});
            parent.line_indices.push_back(elements.lines.size() - 1);
        }

        void layout_node(const node_variant& n, const points x, const points y, layout_elements& elements, formula_node& parent)
        {
            std::visit(overload{[&](const glyph& g) { layout_glyph(g, x, y, elements, parent); },
                                [&](const rule& r) { layout_rule(r, x, y, elements, parent); },
                                [&](const wrapped_box& wb) {
                                    const box& b = static_cast<const box&>(wb);
                                    // In an hbox, a child box's shift moves it vertically:
                                    // positive shift = down = subtract from y in mfl Y-up coords
                                    const auto shifted_y = y - dist_to_points(b.shift);
                                    layout_box(b, x, shifted_y, elements, parent);
                                },
                                [](const auto&) {}},
                       n);
        }

        void layout_hbox(const box& b, const points x, const points y, layout_elements& elements, formula_node& parent)
        {
            auto cur_x = x;
            for (const auto& node : b.nodes)
            {
                layout_node(node, cur_x, y, elements, parent);
                cur_x += dist_to_points(scaled_width(node, b.glue.scale));
            }
        }

        void layout_vbox(const box& b, const points x, const points y, layout_elements& elements, formula_node& parent)
        {
            // In a vbox, y is the baseline of the reference node.
            // Nodes are stored top-to-bottom.
            // The top of the box is at y + height(b).
            // We iterate from top to bottom, tracking the current top edge.
            auto cur_top = y + dist_to_points(b.dims.height);
            for (const auto& node : b.nodes)
            {
                const auto node_shift = dist_to_points(shift(node));
                const auto node_h = dist_to_points(vheight(node));
                const auto node_d = dist_to_points(depth(node));
                // Baseline of this node is at cur_top - node_h
                const auto node_baseline = cur_top - node_h;
                layout_node(node, x + node_shift, node_baseline, elements, parent);
                // Move down by the full vertical size of this node
                cur_top -= node_h + node_d;
            }
        }

        void layout_box(const box& b, const points x, const points y, layout_elements& elements, formula_node& parent)
        {
            // Создать дочерний узел если есть аннотация
            formula_node* target = &parent;
            if (b.annotation != formula_node_type::root)
            {
                parent.children.push_back({.type = b.annotation});
                target = &parent.children.back();
            }

            // Save the initial count of glyphs and lines to track what's added during layout
            const auto initial_glyph_count = target->glyph_indices.size();
            const auto initial_line_count = target->line_indices.size();

            if (b.kind == box_kind::hbox)
                layout_hbox(b, x, y, elements, *target);
            else
                layout_vbox(b, x, y, elements, *target);

            // Compute bounding box based on actual content
            const auto box_left = x;
            const auto box_right = x + dist_to_points(b.dims.width);
            // In MFL Y-up coordinates: y is the baseline
            // box_highest_y = y + height = highest Y value (visually above baseline = visual top)
            // box_lowest_y  = y - depth  = lowest Y value  (visually below baseline = visual bottom)
            const auto box_highest_y = y + dist_to_points(b.dims.height);
            const auto box_lowest_y  = y - dist_to_points(b.dims.depth);

            // Check if any glyphs or lines were added directly to this node (not to children)
            const auto glyphs_added = target->glyph_indices.size() > initial_glyph_count;
            const auto lines_added = target->line_indices.size() > initial_line_count;

            if (!target->children.empty() || glyphs_added || lines_added) {
                // Initialize bounds
                auto min_x = box_left;
                auto min_y = box_lowest_y;   // lowest Y in MFL Y-up = visual bottom
                auto max_x = box_right;
                auto max_y = box_highest_y;  // highest Y in MFL Y-up = visual top

                // Expand bounds to include all children
                for (const auto& child : target->children)
                {
                    const auto child_right = child.bbox_x + child.bbox_width;
                    const auto child_bottom = child.bbox_y + child.bbox_height;

                    min_x = std::min(min_x, child.bbox_x);
                    min_y = std::min(min_y, child.bbox_y);
                    max_x = std::max(max_x, child_right);
                    max_y = std::max(max_y, child_bottom);
                }

                // Expand bounds to include directly added glyphs
                for (size_t i = initial_glyph_count; i < target->glyph_indices.size(); ++i) {
                    const auto& glyph = elements.glyphs[target->glyph_indices[i]];
                    // Use actual glyph metrics from shaped_glyph
                    const auto glyph_left = glyph.x;
                    const auto glyph_right = glyph.x + glyph.advance;
                    // In MFL Y-up coordinates:
                    // glyph_top = baseline + height = highest Y value (visually above baseline)
                    // glyph_bottom = baseline - depth = lowest Y value (visually below baseline)
                    // min_y should track the LOWEST Y (most negative = visually lowest = bottom)
                    // max_y should track the HIGHEST Y (most positive = visually highest = top)
                    const auto glyph_top = glyph.y + glyph.height;    // highest Y in MFL Y-up
                    const auto glyph_bottom = glyph.y - glyph.depth;  // lowest Y in MFL Y-up

                    min_x = std::min(min_x, glyph_left);
                    min_y = std::min(min_y, glyph_bottom);  // min_y = lowest Y = visual bottom
                    max_x = std::max(max_x, glyph_right);
                    max_y = std::max(max_y, glyph_top);     // max_y = highest Y = visual top
                }

                // Expand bounds to include directly added lines
                for (size_t i = initial_line_count; i < target->line_indices.size(); ++i) {
                    const auto& line = elements.lines[target->line_indices[i]];
                    const auto line_left = line.x;
                    const auto line_right = line.x + line.length;
                    // For horizontal lines, thickness extends downward in mfl coordinate system
                    const auto line_top = line.y;
                    const auto line_bottom = line.y + line.thickness;

                    min_x = std::min(min_x, line_left);
                    min_y = std::min(min_y, line_top);
                    max_x = std::max(max_x, line_right);
                    max_y = std::max(max_y, line_bottom);
                }

                // Update target's bounding box
                target->bbox_x = min_x;
                target->bbox_y = min_y;
                target->bbox_width = max_x - min_x;
                target->bbox_height = max_y - min_y;
            } else {
                // No children or direct content, use box dimensions
                target->bbox_x = box_left;
                target->bbox_y = box_lowest_y;
                target->bbox_width = dist_to_points(b.dims.width);
                target->bbox_height = box_highest_y - box_lowest_y;
            }
        }
    }

    mfl::layout_elements layout(const std::string_view input, const mfl::points font_size,
                           const mfl::font_face_creator& create_font_face)
    {
        font_library fonts{font_size, create_font_face};

        const auto [noads, error] = parse(input);
        if (error) return {.error = error};

        const auto hbox =
            make_hbox(to_hlist({.style = formula_style::display, .fonts = &fonts}, cramping::off, false, noads));
        mfl::layout_elements result{.width = dist_to_points(hbox.dims.width), .height = dist_to_points(hbox.dims.height)};
        // Инициализировать корневой узел дерева
        result.tree = mfl::formula_node{.type = mfl::formula_node_type::root};
        layout_box(hbox, 0_pt, 0_pt, result, result.tree);
        return result;
    }
}
