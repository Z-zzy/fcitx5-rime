/*
 * SPDX-FileCopyrightText: 2026 Rime AI Correction contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "overlaymenu.h"
#include <algorithm>
#include <unordered_set>

namespace fcitx::rime {

namespace {

std::optional<int>
baseIndexForText(const std::vector<OverlayCandidate> &baseCandidates,
                 const std::string &text, std::optional<int> claimedIndex) {
    if (claimedIndex) {
        const auto claimed = std::find_if(
            baseCandidates.begin(), baseCandidates.end(),
            [&text, claimedIndex](const OverlayCandidate &candidate) {
                return candidate.baseIndex == claimedIndex &&
                       candidate.text == text;
            });
        if (claimed != baseCandidates.end()) {
            return claimed->baseIndex;
        }
    }
    const auto matching = std::find_if(
        baseCandidates.begin(), baseCandidates.end(),
        [&text](const OverlayCandidate &candidate) {
            return candidate.text == text && candidate.baseIndex;
        });
    return matching == baseCandidates.end() ? std::nullopt
                                             : matching->baseIndex;
}

} // namespace

std::vector<OverlayMenuEntry>
OverlayMenu::compose(const OverlayMenuInput &input) {
    std::vector<OverlayMenuEntry> result;
    if (input.baseCandidates.empty()) {
        return result;
    }

    const auto appendBase = [&result](const OverlayCandidate &candidate) {
        result.push_back({OverlayEntryKind::Base, candidate.text, true,
                          candidate.baseIndex});
    };

    const auto &first = input.baseCandidates.front();
    appendBase(first);
    std::unordered_set<int> displayedBaseIndexes;
    std::unordered_set<int> hiddenBaseIndexes;
    if (first.baseIndex) {
        displayedBaseIndexes.insert(*first.baseIndex);
    }

    if (input.localCandidate && !input.localCandidate->text.empty() &&
        input.localCandidate->text != first.text) {
        const auto baseIndex =
            baseIndexForText(input.baseCandidates, input.localCandidate->text,
                             input.localCandidate->baseIndex);
        result.push_back({OverlayEntryKind::Local, input.localCandidate->text,
                          true, baseIndex});
        if (baseIndex) {
            hiddenBaseIndexes.insert(*baseIndex);
        }
    } else if (input.baseCandidates.size() > 1) {
        const auto &second = input.baseCandidates[1];
        appendBase(second);
        if (second.baseIndex) {
            displayedBaseIndexes.insert(*second.baseIndex);
        }
    }

    const auto isTextDisplayed = [&result](const std::string &text) {
        return std::any_of(result.begin(), result.end(),
                           [&text](const OverlayMenuEntry &entry) {
                               return entry.text == text;
                           });
    };
    const bool canShowAiSuggestion =
        input.aiSlot.state == AiSlotState::Suggestion &&
        !input.aiSlot.text.empty() && !isTextDisplayed(input.aiSlot.text);
    if (canShowAiSuggestion) {
        const auto baseIndex = baseIndexForText(
            input.baseCandidates, input.aiSlot.text, input.aiSlot.baseIndex);
        result.push_back({OverlayEntryKind::AiSuggestion, input.aiSlot.text,
                          true, baseIndex});
        if (baseIndex) {
            hiddenBaseIndexes.insert(*baseIndex);
        }
    } else {
        const auto &slotText = input.aiSlot.state == AiSlotState::Suggestion
                                   ? "无纠错建议"
                                   : input.aiSlot.text;
        result.push_back(
            {OverlayEntryKind::AiSlot, slotText, false, std::nullopt});
    }

    for (size_t index = 1; index < input.baseCandidates.size(); ++index) {
        const auto &candidate = input.baseCandidates[index];
        if (candidate.baseIndex &&
            (displayedBaseIndexes.count(*candidate.baseIndex) ||
             hiddenBaseIndexes.count(*candidate.baseIndex))) {
            continue;
        }
        appendBase(candidate);
    }

    if (result.size() > input.baseCandidates.size()) {
        result.resize(input.baseCandidates.size());
    }

    return result;
}

} // namespace fcitx::rime
