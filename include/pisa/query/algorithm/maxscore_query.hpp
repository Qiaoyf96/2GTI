#pragma once

#include <algorithm>
#include <numeric>
#include <vector>

#include "query/queries.hpp"
#include "topk_queue.hpp"
#include "util/compiler_attribute.hpp"
#include "util/util.hpp"

namespace pisa {

struct maxscore_query {
    explicit maxscore_query(topk_queue& topk) : m_topk(topk) {}

    template <typename Cursors>
    [[nodiscard]] PISA_ALWAYSINLINE auto sorted(Cursors&& cursors)
        -> std::vector<typename std::decay_t<Cursors>::value_type>
    {
        std::vector<std::size_t> term_positions(cursors.size());
        std::iota(term_positions.begin(), term_positions.end(), 0);
        std::sort(term_positions.begin(), term_positions.end(), [&](auto&& lhs, auto&& rhs) {
            return cursors[lhs].max_score() > cursors[rhs].max_score();
        });
        std::vector<typename std::decay_t<Cursors>::value_type> sorted;
        for (auto pos: term_positions) {
            sorted.push_back(std::move(cursors[pos]));
        };
        return sorted;
    }

    template <typename Cursors>
    [[nodiscard]] PISA_ALWAYSINLINE auto calc_upper_bounds(Cursors&& cursors, double alpha) -> std::vector<float>
    {
        std::vector<float> upper_bounds(cursors.size());
        auto out = upper_bounds.rbegin();
        float bound = 0.0;
        for (auto pos = cursors.rbegin(); pos != cursors.rend(); ++pos) {
            bound += pos->max_score() + pos->max_deep_score() * alpha;
            *out++ = bound;
        }
        return upper_bounds;
    }

    template <typename Cursors>
    [[nodiscard]] PISA_ALWAYSINLINE auto min_docid(Cursors&& cursors) -> std::uint32_t
    {
        return std::min_element(
                   cursors.begin(),
                   cursors.end(),
                   [](auto&& lhs, auto&& rhs) { return lhs.docid() < rhs.docid(); })
            ->docid();
    }

    enum class UpdateResult : bool { Continue, ShortCircuit };
    enum class DocumentStatus : bool { Insert, Skip };

    template <typename Cursors>
    PISA_ALWAYSINLINE void run_sorted(Cursors&& cursors, uint64_t max_docid, topk_queue &topk_pivot, topk_queue &topk_jump)
    {
        auto upper_bounds = calc_upper_bounds(cursors, alphad);
        auto upper_deep_bounds = calc_upper_bounds(cursors, betad);
        auto above_threshold = [&](topk_queue &m_topk, auto score) { return m_topk.would_enter(score / thresd); };

        auto first_upper_bound = upper_bounds.end();
        auto first_deep_upper_bound = upper_deep_bounds.end();
        auto first_lookup = cursors.end();
        auto next_docid = min_docid(cursors);

        auto update_non_essential_lists = [&] {
            while (first_lookup != cursors.begin()
                   && !above_threshold(topk_pivot, *std::prev(first_upper_bound))) {
                --first_lookup;
                --first_upper_bound;
                --first_deep_upper_bound;
                if (first_lookup == cursors.begin()) {
                    return UpdateResult::ShortCircuit;
                }
            }
            return UpdateResult::Continue;
        };

        if (update_non_essential_lists() == UpdateResult::ShortCircuit) {
            return;
        }

        float current_score_pivot, current_score_evaluate, current_score_jump = 0;
        std::uint32_t current_docid = 0;

        int evaluated = 0;

        while (current_docid < max_docid) {
            auto status = DocumentStatus::Skip;
            while (status == DocumentStatus::Skip) {
                if (PISA_UNLIKELY(next_docid >= max_docid)) {
                    // printf("e: %d\n", evaluated);
                    return;
                }

                current_score_pivot = 0;
                current_score_evaluate = 0;
                current_score_jump = 0;
                current_docid = std::exchange(next_docid, max_docid);

                std::for_each(cursors.begin(), first_lookup, [&](auto& cursor) {
                    if (cursor.docid() == current_docid) {
                        auto score = cursor.score(), deep_score = cursor.deep_score();
                        current_score_pivot += score + alphad * deep_score;
                        current_score_evaluate += (1 - gammad) * deep_score + gammad * score;
                        current_score_jump += score + betad * deep_score;
                        cursor.next();
                    }
                    if (auto docid = cursor.docid(); docid < next_docid) {
                        next_docid = docid;
                    }
                });

                status = DocumentStatus::Insert;
                auto lookup_bound = first_deep_upper_bound;
                for (auto pos = first_lookup; pos != cursors.end(); ++pos, ++lookup_bound) {
                    auto& cursor = *pos;
                    if (not above_threshold(topk_jump, current_score_jump + *lookup_bound)) {
                        status = DocumentStatus::Skip;
                        m_topk.insert(current_score_evaluate, current_docid);
                        break;
                    }
                    cursor.next_geq(current_docid);
                    if (cursor.docid() == current_docid) {
                        auto score = cursor.score(), deep_score = cursor.deep_score();
                        current_score_pivot += score + alphad * deep_score;
                        current_score_evaluate += (1 - gammad) * deep_score + gammad * score;
                        current_score_jump += score + betad * deep_score;
                    }
                }
            }

            evaluated += 1;

            topk_jump.insert(current_score_jump, current_docid);
            topk_pivot.insert(current_score_pivot, current_docid);
            m_topk.insert(current_score_evaluate, current_docid);

            if (topk_pivot.insert(current_score_pivot, current_docid)
                && update_non_essential_lists() == UpdateResult::ShortCircuit) {
                    // printf("e: %d\n", evaluated);
                return;
            }
        }
    }

    template <typename Cursors>
    void operator()(Cursors&& cursors_, uint64_t max_docid)
    {
        if (cursors_.empty()) {
            return;
        }

        topk_queue topk_pivot(m_topk.capacity());
        topk_queue topk_jump(m_topk.capacity());

        auto cursors = sorted(cursors_);
        run_sorted(cursors, max_docid, topk_pivot, topk_jump);
        std::swap(cursors, cursors_);
    }

    std::vector<typename topk_queue::entry_type> const& topk() const { return m_topk.topk(); }

  private:
    topk_queue& m_topk;
};

}  // namespace pisa
