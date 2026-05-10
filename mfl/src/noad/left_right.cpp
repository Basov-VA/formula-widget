#include "noad/left_right.hpp"

#include "node/box.hpp"
#include "node/hlist.hpp"
#include "settings.hpp"

namespace mfl
{
    hlist left_right_to_hlist(const settings s, const cramping cramp, const left_right& l)
    {
        auto content = clean_box(s, cramp, l.noads);
        content.annotation = formula_node_type::group;  // content inside left-right delimiters
        const auto requested_height = content.dims.depth + content.dims.height;
        hlist result;
        if (l.left_delim_code != 0) {
            auto left_delim = make_auto_height_glyph(s, font_family::roman, l.left_delim_code, requested_height).first;
            // No specific annotation for delimiter glyphs in this context
            result.nodes.emplace_back(std::move(left_delim));
        }

        result.nodes.emplace_back(std::move(content));

        if (l.right_delim_code != 0) {
            auto right_delim = make_auto_height_glyph(s, font_family::roman, l.right_delim_code, requested_height).first;
            // No specific annotation for delimiter glyphs in this context
            result.nodes.emplace_back(std::move(right_delim));
        }

        // Wrap the entire left-right construct in a box with appropriate annotation
        auto result_box = make_hbox(std::move(result));
        result_box.annotation = formula_node_type::left_right;
        return make_hlist(std::move(result_box));
    }
}
