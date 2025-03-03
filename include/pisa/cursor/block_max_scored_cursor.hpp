#pragma once

#include <vector>

#include "cursor/max_scored_cursor.hpp"
#include "query/queries.hpp"
#include "scorer/index_scorer.hpp"
#include "wand_data.hpp"

namespace pisa {

template <typename Cursor, typename Wand>
class BlockMaxScoredCursor: public MaxScoredCursor<Cursor> {
  public:
    using base_cursor_type = Cursor;

    BlockMaxScoredCursor(
        Cursor cursor,
        TermScorer term_scorer,
        TermScorer deep_term_scorer,
        float weight,
        float max_score,
        float max_deep_score,
        typename Wand::wand_data_enumerator wdata)
        : MaxScoredCursor<Cursor>(std::move(cursor), std::move(term_scorer), std::move(deep_term_scorer), weight, max_score, max_deep_score),
          m_wdata(std::move(wdata))
    {}
    BlockMaxScoredCursor(BlockMaxScoredCursor const&) = delete;
    BlockMaxScoredCursor(BlockMaxScoredCursor&&) = default;
    BlockMaxScoredCursor& operator=(BlockMaxScoredCursor const&) = delete;
    BlockMaxScoredCursor& operator=(BlockMaxScoredCursor&&) = default;
    ~BlockMaxScoredCursor() = default;

    [[nodiscard]] PISA_ALWAYSINLINE auto block_max_score() -> float
    {
        // NOTE: we do not multiply because the algorithms explicitly multiply this by the value
        // taken from `query_weight()` method of `ScoredCursor`. We might want to refactor this to
        // be more consistent.
        return m_wdata.score();
    }
    
    [[nodiscard]] PISA_ALWAYSINLINE auto block_max_deep_score() -> float
    {
        // NOTE: we do not multiply because the algorithms explicitly multiply this by the value
        // taken from `query_weight()` method of `ScoredCursor`. We might want to refactor this to
        // be more consistent.
        return m_wdata.deep_score();
    }

    [[nodiscard]] PISA_ALWAYSINLINE auto block_max_docid() -> std::uint32_t
    {
        return m_wdata.docid();
    }

    PISA_ALWAYSINLINE void block_max_next_geq(std::uint32_t docid) { m_wdata.next_geq(docid); }

  private:
    typename Wand::wand_data_enumerator m_wdata;
};

template <typename Index, typename WandType, typename Scorer>
[[nodiscard]] auto make_block_max_scored_cursors(
    Index const& index, WandType const& wdata, Scorer const& scorer, Query query, bool weighted = false)
{
    auto terms = query.terms;
    auto query_term_freqs = query_freqs(terms);

    std::vector<BlockMaxScoredCursor<typename Index::document_enumerator, WandType>> cursors;
    cursors.reserve(query_term_freqs.size());
    std::transform(
        query_term_freqs.begin(), query_term_freqs.end(), std::back_inserter(cursors), [&](auto&& term) {
            auto term_weight = 1.0F;
            auto term_id = term.first;
            auto max_weight = wdata.max_term_weight(term_id);
            auto max_deep_weight = wdata.max_deep_term_weight(term_id);

            if (weighted) {
                term_weight = term.second;
                max_weight = term_weight * max_weight;
                max_deep_weight *= term_weight;
                return BlockMaxScoredCursor<typename Index::document_enumerator, WandType>(
                    std::move(index[term_id]),
                    [scorer = scorer.term_scorer(term_id), weight = term_weight](
                        uint32_t doc, uint32_t freq) { return weight * scorer(doc, freq); },
                    [scorer = scorer.deep_term_scorer(term_id), weight = term_weight](
                        uint32_t doc, uint32_t freq) { return weight * scorer(doc, freq); },
                    term_weight,
                    max_weight,
                    max_deep_weight,
                    wdata.getenum(term_id));
            }
            

            return BlockMaxScoredCursor<typename Index::document_enumerator, WandType>(
                std::move(index[term_id]),
                scorer.term_scorer(term_id),
                scorer.deep_term_scorer(term_id),
                term_weight,
                max_weight,
                max_deep_weight,
                wdata.getenum(term_id));
        });

    return cursors;
}

}  // namespace pisa
