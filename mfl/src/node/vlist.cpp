#include "node/vlist.hpp"

#include <algorithm>
#include <numeric>
#include <ranges>

namespace mfl
{
    dist_t vlist_size(const vlist& l)
    {
        if (l.nodes.empty()) return dist_t{};
        auto sizes = l.nodes | std::views::transform([](const node_variant& n) { return vsize(n); });
        return std::accumulate(std::ranges::begin(sizes), std::ranges::end(sizes), dist_t{});
    }
}
