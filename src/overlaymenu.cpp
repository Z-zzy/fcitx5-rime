/*
 * SPDX-FileCopyrightText: 2026 Rime AI Correction contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "overlaymenu.h"
#include <unordered_set>

namespace fcitx::rime {

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

    std::unordered_set<std::string> hiddenTexts;
    hiddenTexts.insert(first.text);

    if (input.localCandidate && !input.localCandidate->text.empty() &&
        input.localCandidate->text != first.text) {
        result.push_back({OverlayEntryKind::Local, input.localCandidate->text,
                          true, input.localCandidate->baseIndex});
        hiddenTexts.insert(input.localCandidate->text);
    } else if (input.baseCandidates.size() > 1) {
        const auto &second = input.baseCandidates[1];
        appendBase(second);
        hiddenTexts.insert(second.text);
    }

    const bool canShowAiSuggestion =
        input.aiSlot.state == AiSlotState::Suggestion &&
        !input.aiSlot.text.empty() &&
        hiddenTexts.find(input.aiSlot.text) == hiddenTexts.end();
    if (canShowAiSuggestion) {
        result.push_back({OverlayEntryKind::AiSuggestion, input.aiSlot.text,
                          true, input.aiSlot.baseIndex});
        hiddenTexts.insert(input.aiSlot.text);
    } else {
        const auto &slotText = input.aiSlot.state == AiSlotState::Suggestion
                                   ? "无纠错建议"
                                   : input.aiSlot.text;
        result.push_back(
            {OverlayEntryKind::AiSlot, slotText, false, std::nullopt});
    }

    for (size_t index = 1; index < input.baseCandidates.size(); ++index) {
        const auto &candidate = input.baseCandidates[index];
        if (hiddenTexts.insert(candidate.text).second) {
            appendBase(candidate);
        }
    }

    if (result.size() > input.baseCandidates.size()) {
        result.resize(input.baseCandidates.size());
    }

    return result;
}

} // namespace fcitx::rime
