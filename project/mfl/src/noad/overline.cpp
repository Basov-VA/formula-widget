#include "noad/overline.hpp"

#include "node/box.hpp"
#include "node/hlist.hpp"
#include "node/vlist.hpp"
#include "settings.hpp"

namespace mfl
{
    hlist overline_to_hlist(const settings s, const cramping cramp, const overline& ol)
    {
        if (ol.noads.empty()) return {};

        auto content = clean_box(s, cramp, ol.noads);
        content.annotation = formula_node_type::group;  // or appropriate type for overlined content
        const auto w = content.dims.width;

        auto l =
            make_vlist(kern{.size = overline_gap(s)}, rule{.width = w, .height = overline_thickness(s), .depth = 0},
                       kern{.size = overline_padding(s)});
        auto result_box = make_up_vbox(w, std::move(content), std::move(l));
        result_box.annotation = formula_node_type::overline;
        return make_hlist(std::move(result_box));
    }
}
