#pragma once

#define _USE_MATH_DEFINES

#include <cmath>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "index_scorer.hpp"

namespace pisa {

// DEEP
template <typename Wand>
struct deep: public index_scorer<Wand> {
    using index_scorer<Wand>::index_scorer;

    term_scorer_t term_scorer(uint64_t term_id) const override
    {
        auto s = [&, term_id](uint32_t doc, uint32_t freq) {
            return (freq & 0xFFFFul);
        };
        return s;
    }

    term_scorer_t deep_term_scorer(uint64_t term_id) const override
    {
        auto s = [&, term_id](uint32_t doc, uint32_t freq) {
            return (freq >> 16) & (0xFFFFul);
        };
        return s;
    }
};

}  // namespace pisa
