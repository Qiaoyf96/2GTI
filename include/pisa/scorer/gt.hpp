#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <iostream>
#include "index_scorer.hpp"
namespace pisa {

template <typename Wand>
struct gt: public index_scorer<Wand> {
    using index_scorer<Wand>::index_scorer;

    gt(const Wand& wdata, const float b, const float k1)
        : index_scorer<Wand>(wdata), m_b(b), m_k1(k1)
    {}

    float doc_term_weight(uint64_t freq, float norm_len) const
    {
        auto f = static_cast<float>(freq);
        return f / (f + m_k1 * (1.0F - m_b + m_b * norm_len));
    }

    // IDF (inverse document frequency)
    float query_term_weight(uint64_t df, uint64_t num_docs) const
    {
        auto fdf = static_cast<float>(df);
        float idf = std::log((float(num_docs) - fdf + 0.5F) / (fdf + 0.5F));
        static const float epsilon_score = 1.0E-6;
        return std::max(epsilon_score, idf) * (1.0F + m_k1);
    }

    term_scorer_t term_scorer(uint64_t term_id) const override
    {
        auto term_len = this->m_wdata.term_posting_count(term_id);
        auto term_weight = query_term_weight(term_len, this->m_wdata.num_docs());
        auto s = [&, term_weight](uint32_t doc, uint32_t freq) {
            return term_weight * doc_term_weight(((freq >> 9) & 0x1ff), this->m_wdata.norm_len(doc));
        };
        return s;
    }

    term_scorer_t deep_term_scorer(uint64_t term_id) const override
    {
        auto s = [&, term_id](uint32_t doc, uint32_t freq) {
            return (freq & (0x1ff));
        };
        return s;
    }

  private:
    float m_b;
    float m_k1;
};
}  // namespace pisa
