/*
 * SPDX-FileCopyrightText: 2026 Rime AI Correction contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "overlaymenu.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using fcitx::rime::AiSlot;
using fcitx::rime::AiSlotState;
using fcitx::rime::OverlayCandidate;
using fcitx::rime::OverlayEntryKind;
using fcitx::rime::OverlayMenu;
using fcitx::rime::OverlayMenuEntry;
using fcitx::rime::OverlayMenuInput;

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void expectEntry(const OverlayMenuEntry &entry, OverlayEntryKind kind,
                 const std::string &text, bool selectable, int baseIndex) {
    expect(entry.kind == kind, "unexpected entry kind");
    expect(entry.text == text, "unexpected entry text");
    expect(entry.selectable == selectable, "unexpected selectability");
    if (baseIndex < 0) {
        expect(!entry.baseIndex, "expected no base candidate index");
    } else {
        expect(entry.baseIndex && *entry.baseIndex == baseIndex,
               "unexpected base candidate index");
    }
}

void localCorrectionOccupiesSecondPositionAndLoadingSlotIsInert() {
    OverlayMenuInput input{
        .baseCandidates = {{"嫩里", 0}, {"能理", 1}, {"能力", 2}, {"能量", 3}},
        .localCandidate = OverlayCandidate{"能力", std::nullopt},
        .aiSlot = AiSlot{AiSlotState::Loading, "AI 纠错中", std::nullopt},
    };

    const auto menu = OverlayMenu::compose(input);

    expect(menu.size() == 4,
           "expected the fixed AI slot to replace the final base candidate");
    expectEntry(menu[0], OverlayEntryKind::Base, "嫩里", true, 0);
    expectEntry(menu[1], OverlayEntryKind::Local, "能力", true, 2);
    expectEntry(menu[2], OverlayEntryKind::AiSlot, "AI 纠错中", false, -1);
    expectEntry(menu[3], OverlayEntryKind::Base, "能理", true, 1);
}

void aiSuggestionOccupiesThirdPositionAndPromotesExistingBaseCandidate() {
    OverlayMenuInput input{
        .baseCandidates = {{"嫩里", 0}, {"能理", 1}, {"能力", 2}, {"能量", 3}},
        .localCandidate = std::nullopt,
        .aiSlot = AiSlot{AiSlotState::Suggestion, "能力", std::nullopt},
    };

    const auto menu = OverlayMenu::compose(input);

    expect(menu.size() == 4,
           "expected base 1, original base 2, AI 3 and one remaining base");
    expectEntry(menu[0], OverlayEntryKind::Base, "嫩里", true, 0);
    expectEntry(menu[1], OverlayEntryKind::Base, "能理", true, 1);
    expectEntry(menu[2], OverlayEntryKind::AiSuggestion, "能力", true, 2);
    expectEntry(menu[3], OverlayEntryKind::Base, "能量", true, 3);
}

void duplicateAiSuggestionKeepsTheFirstTwoPositionsAndBecomesUnavailable() {
    OverlayMenuInput input{
        .baseCandidates = {{"嫩里", 0}, {"能力", 1}, {"能量", 2}},
        .localCandidate = std::nullopt,
        .aiSlot = AiSlot{AiSlotState::Suggestion, "能力", std::nullopt},
    };

    const auto menu = OverlayMenu::compose(input);

    expect(menu.size() == 3,
           "expected the unavailable third slot to reserve one base position");
    expectEntry(menu[0], OverlayEntryKind::Base, "嫩里", true, 0);
    expectEntry(menu[1], OverlayEntryKind::Base, "能力", true, 1);
    expectEntry(menu[2], OverlayEntryKind::AiSlot, "无纠错建议", false, -1);
}

void promotedLocalCandidateRetainsItsOriginalRimeSelectionTarget() {
    OverlayMenuInput input{
        .baseCandidates = {{"嫩里", 0}, {"能理", 1}, {"能力", 2}},
        .localCandidate = OverlayCandidate{"能力", 2},
        .aiSlot = AiSlot{AiSlotState::Loading, "AI 纠错中", std::nullopt},
    };

    const auto menu = OverlayMenu::compose(input);

    expectEntry(menu[1], OverlayEntryKind::Local, "能力", true, 2);
}

void unrelatedDuplicateBaseCandidatesAreNotRemovedByTheAiSlot() {
    OverlayMenuInput input{
        .baseCandidates = {{"甲", 0}, {"乙", 1}, {"乙", 2}, {"丙", 3}},
        .localCandidate = std::nullopt,
        .aiSlot = AiSlot{AiSlotState::Loading, "AI 纠错中", std::nullopt},
    };

    const auto menu = OverlayMenu::compose(input);

    expect(menu.size() == 4, "expected the original page capacity");
    expectEntry(menu[0], OverlayEntryKind::Base, "甲", true, 0);
    expectEntry(menu[1], OverlayEntryKind::Base, "乙", true, 1);
    expectEntry(menu[2], OverlayEntryKind::AiSlot, "AI 纠错中", false, -1);
    expectEntry(menu[3], OverlayEntryKind::Base, "乙", true, 2);
}

} // namespace

int main() {
    localCorrectionOccupiesSecondPositionAndLoadingSlotIsInert();
    aiSuggestionOccupiesThirdPositionAndPromotesExistingBaseCandidate();
    duplicateAiSuggestionKeepsTheFirstTwoPositionsAndBecomesUnavailable();
    promotedLocalCandidateRetainsItsOriginalRimeSelectionTarget();
    unrelatedDuplicateBaseCandidatesAreNotRemovedByTheAiSlot();
    return EXIT_SUCCESS;
}
