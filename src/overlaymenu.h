/*
 * SPDX-FileCopyrightText: 2026 Rime AI Correction contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_RIME_OVERLAYMENU_H_
#define _FCITX_RIME_OVERLAYMENU_H_

#include <optional>
#include <string>
#include <vector>

namespace fcitx::rime {

enum class AiSlotState { Loading, Unavailable, Suggestion };

struct OverlayCandidate {
    std::string text;
    std::optional<int> baseIndex;
};

struct AiSlot {
    AiSlotState state = AiSlotState::Loading;
    std::string text;
    std::optional<int> baseIndex;
};

struct OverlayMenuInput {
    std::vector<OverlayCandidate> baseCandidates;
    std::optional<OverlayCandidate> localCandidate;
    AiSlot aiSlot;
};

enum class OverlayEntryKind { Base, Local, AiSlot, AiSuggestion };

struct OverlayMenuEntry {
    OverlayEntryKind kind;
    std::string text;
    bool selectable;
    std::optional<int> baseIndex;
};

class OverlayMenu {
public:
    static std::vector<OverlayMenuEntry> compose(const OverlayMenuInput &input);
};

} // namespace fcitx::rime

#endif // _FCITX_RIME_OVERLAYMENU_H_
