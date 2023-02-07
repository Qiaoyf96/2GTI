#pragma once

#include <vector>

#include "cursor/scored_cursor.hpp"
#include "query/queries.hpp"
#include "wand_data.hpp"

namespace pisa {

template <typename Cursor>
class MaxScoredCursor: public ScoredCursor<Cursor> {
  public:
    using base_cursor_type = Cursor;

    MaxScoredCursor(Cursor cursor, TermScorer term_scorer, TermScorer deep_term_scorer, float query_weight, float max_score, float max_deep_score)
        : ScoredCursor<Cursor>(std::move(cursor), std::move(term_scorer), std::move(deep_term_scorer), query_weight),
          m_max_score(max_score), m_max_deep_score(max_deep_score)
    {}
    MaxScoredCursor(MaxScoredCursor const&) = delete;
    MaxScoredCursor(MaxScoredCursor&&) = default;
    MaxScoredCursor& operator=(MaxScoredCursor const&) = delete;
    MaxScoredCursor& operator=(MaxScoredCursor&&) = default;
    ~MaxScoredCursor() = default;

    [[nodiscard]] PISA_ALWAYSINLINE auto max_score() const noexcept -> float { return m_max_score; }
    [[nodiscard]] PISA_ALWAYSINLINE auto max_deep_score() const noexcept -> float { return m_max_deep_score; }

  private:
    float m_max_score;
    float m_max_deep_score;
};

template <typename Index, typename WandType, typename Scorer>
[[nodiscard]] auto make_max_scored_cursors(
    Index const& index, WandType const& wdata, Scorer const& scorer, Query query, bool weighted = false)
{
    auto terms = query.terms;
    auto query_term_freqs = query_freqs(terms);

    std::vector<MaxScoredCursor<typename Index::document_enumerator>> cursors;
    cursors.reserve(query_term_freqs.size());
    std::transform(
        query_term_freqs.begin(), query_term_freqs.end(), std::back_inserter(cursors), [&](auto&& term) {
            auto term_weight = 1.0f;
            auto term_id = term.first;
            auto max_weight = wdata.max_term_weight(term_id);
            auto max_deep_weight = wdata.max_deep_term_weight(term_id);

            if (weighted) {
                term_weight = term.second;
                max_weight = term_weight * max_weight;
                max_deep_weight = term_weight * max_deep_weight;
                return MaxScoredCursor<typename Index::document_enumerator>(
                    index[term_id],
                    [scorer = scorer.term_scorer(term_id), weight = term_weight](
                        uint32_t doc, uint32_t freq) { return weight * scorer(doc, freq); },
                    [scorer = scorer.deep_term_scorer(term_id), weight = term_weight](
                        uint32_t doc, uint32_t freq) { return weight * scorer(doc, freq); },
                    term_weight,
                    max_weight,
                    max_deep_weight);
            }

            return MaxScoredCursor<typename Index::document_enumerator>(
                index[term_id], scorer.term_scorer(term_id), scorer.deep_term_scorer(term_id), term_weight, max_weight, max_deep_weight);
        });
    return cursors;
}

}  // namespace pisa
