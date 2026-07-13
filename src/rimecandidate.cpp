/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimecandidate.h"
#include "rimeengine.h"
#include <algorithm>
#include <cstring>
#include <fcitx-utils/log.h>
#include <fcitx/candidatelist.h>
#include <memory>
#include <rime_api.h>
#include <stdexcept>

namespace fcitx::rime {

namespace {

struct SelectionKey {
    std::string label;
    KeySym sym;
};

SelectionKey selectionKeyForIndex(const RimeContext &context,
                                  int numSelectKeys, bool hasLabel,
                                  size_t index) {
    const auto &menu = context.menu;
    if (index < static_cast<size_t>(menu.page_size) && hasLabel) {
        return {context.select_labels[index],
                index < static_cast<size_t>(numSelectKeys)
                    ? static_cast<KeySym>(menu.select_keys[index])
                    : static_cast<KeySym>('0' + (index + 1) % 10)};
    }
    if (index < static_cast<size_t>(numSelectKeys)) {
        return {std::string(1, menu.select_keys[index]),
                static_cast<KeySym>(menu.select_keys[index])};
    }
    const auto number = (index + 1) % 10;
    return {std::to_string(number), static_cast<KeySym>('0' + number)};
}

} // namespace

RimeCandidateWord::RimeCandidateWord(RimeEngine *engine,
                                     const RimeCandidate &candidate, KeySym sym,
                                     int idx)
    : engine_(engine), sym_(sym), idx_(idx) {
    setText(Text{candidate.text});
    if (candidate.comment && candidate.comment[0]) {
        setComment(Text{candidate.comment});
    }
}

void RimeCandidateWord::select(InputContext *inputContext) const {
    if (auto *state = engine_->state(inputContext)) {
        state->selectCandidate(inputContext, idx_, /*global=*/false);
    }
}

void RimeCandidateWord::forget(RimeState *state) const {
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    state->deleteCandidate(idx_, /*global=*/false);
#endif
}

RimeGlobalCandidateWord::RimeGlobalCandidateWord(RimeEngine *engine,
                                                 const RimeCandidate &candidate,
                                                 int idx)
    : engine_(engine), idx_(idx) {
    setText(Text{candidate.text});
    if (candidate.comment && candidate.comment[0]) {
        setComment(Text{candidate.comment});
    }
}

void RimeGlobalCandidateWord::select(InputContext *inputContext) const {
    if (auto *state = engine_->state(inputContext)) {
        state->selectCandidate(inputContext, idx_, /*global=*/true);
    }
}

void RimeGlobalCandidateWord::forget(RimeState *state) const {
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    state->deleteCandidate(idx_, /*global=*/true);
#endif
}

OverlayCandidateWord::OverlayCandidateWord(RimeEngine *engine,
                                           const OverlayMenuEntry &entry)
    : engine_(engine), kind_(entry.kind), baseIndex_(entry.baseIndex) {
    setText(Text{entry.text});
    if (kind_ == OverlayEntryKind::Local) {
        setComment(Text{"本地纠错"});
    } else if (kind_ == OverlayEntryKind::AiSuggestion) {
        setComment(Text{"AI 建议"});
    }
}

void OverlayCandidateWord::select(InputContext *inputContext) const {
    if (kind_ == OverlayEntryKind::AiSlot) {
        return;
    }
    if (auto *state = engine_->state(inputContext)) {
        if (baseIndex_) {
            state->selectCandidate(inputContext, *baseIndex_,
                                   /*global=*/false);
        } else {
            const auto source = kind_ == OverlayEntryKind::AiSuggestion
                                    ? OverlayCandidateSource::Ai
                                    : OverlayCandidateSource::Local;
            state->commitOverlayCandidate(inputContext, text().toString(),
                                          source);
        }
    }
}

RimeCandidateList::RimeCandidateList(RimeEngine *engine, InputContext *ic,
                                     const RimeContext &context)
    : engine_(engine), ic_(ic), hasPrev_(context.menu.page_no != 0),
      hasNext_(!context.menu.is_last_page) {
    setPageable(this);
    setBulk(this);
    setActionable(this);
#ifndef FCITX_RIME_NO_HIGHLIGHT_CANDIDATE
    setBulkCursor(this);
#endif

    const auto &menu = context.menu;

    int num_select_keys = menu.select_keys ? strlen(menu.select_keys) : 0;
    bool has_label = RIME_STRUCT_HAS_MEMBER(context, context.select_labels) &&
                     context.select_labels;

    auto *state = engine_->state(ic);
    const bool overlayEnabled =
        engine_->aiOverlayEnabled() && state &&
        state->currentSchema() == *engine_->config().aiOverlaySchema;

    if (!overlayEnabled) {
        int i;
        for (i = 0; i < menu.num_candidates; ++i) {
            auto selection = selectionKeyForIndex(
                context, num_select_keys, has_label, static_cast<size_t>(i));
            selection.label.append(" ");
            labels_.emplace_back(selection.label);
            candidateWords_.emplace_back(std::make_unique<RimeCandidateWord>(
                engine, menu.candidates[i], selection.sym, i));

            if (i == menu.highlighted_candidate_index) {
                cursor_ = i;
            }
        }
        return;
    }

    OverlayMenuInput overlayInput;
    for (int i = 0; i < menu.num_candidates; ++i) {
        overlayInput.baseCandidates.push_back(
            {menu.candidates[i].text ? menu.candidates[i].text : "", i});
    }
    const auto &demoLocalCandidate =
        *engine_->config().aiOverlayDemoLocalCandidate;
    const auto session = state->session(false);
    const auto *rawInput =
        session ? engine_->api()->get_input(session) : nullptr;
    const bool matchesDemoInput =
        rawInput && *engine_->config().aiOverlayDemoRawInput == rawInput;
    if (!demoLocalCandidate.empty() && matchesDemoInput) {
        const auto iter = std::find_if(
            overlayInput.baseCandidates.begin(),
            overlayInput.baseCandidates.end(),
            [&demoLocalCandidate](const OverlayCandidate &candidate) {
                return candidate.text == demoLocalCandidate;
            });
        overlayInput.localCandidate =
            OverlayCandidate{demoLocalCandidate,
                             iter == overlayInput.baseCandidates.end()
                                 ? std::nullopt
                                 : iter->baseIndex};
    }
    overlayInput.aiSlot = {AiSlotState::Loading, "AI 纠错中", std::nullopt};

    const auto overlayEntries = OverlayMenu::compose(overlayInput);
    for (size_t i = 0; i < overlayEntries.size(); ++i) {
        auto selection =
            selectionKeyForIndex(context, num_select_keys, has_label, i);
        selection.label.append(" ");
        labels_.emplace_back(selection.label);
        overlaySelectKeys_.push_back(selection.sym);

        const auto &entry = overlayEntries[i];
        if (entry.kind == OverlayEntryKind::Base && entry.baseIndex) {
            candidateWords_.emplace_back(std::make_unique<RimeCandidateWord>(
                engine, menu.candidates[*entry.baseIndex], selection.sym,
                *entry.baseIndex));
        } else {
            candidateWords_.emplace_back(
                std::make_unique<OverlayCandidateWord>(engine, entry));
        }

        if (entry.baseIndex &&
            *entry.baseIndex == menu.highlighted_candidate_index) {
            cursor_ = static_cast<int>(i);
        }
    }
}

bool RimeCandidateList::selectOverlayCandidate(InputContext *inputContext,
                                                KeySym sym) const {
    for (size_t index = 0; index < overlaySelectKeys_.size(); ++index) {
        if (overlaySelectKeys_[index] == sym) {
            candidateWords_[index]->select(inputContext);
            return true;
        }
    }
    return false;
}

const CandidateWord &RimeCandidateList::candidateFromAll(int idx) const {
    if (idx < 0 || empty()) {
        throw std::invalid_argument("Invalid global index");
    }

    auto session = engine_->state(ic_)->session(false);
    if (!session) {
        throw std::invalid_argument("Invalid session");
    }

    auto index = static_cast<size_t>(idx);

    auto *api = engine_->api();

    RimeCandidateListIterator iter;
    if (index >= globalCandidateWords_.size()) {
        if (index >= maxSize_) {
            throw std::invalid_argument("Invalid global index");
        }
    } else {
        if (globalCandidateWords_[index]) {
            return *globalCandidateWords_[index];
        }
    }

    if (!api->candidate_list_from_index(session, &iter, idx) ||
        !api->candidate_list_next(&iter)) {
        maxSize_ = std::min(index, maxSize_);
        throw std::invalid_argument("Invalid global index");
    }

    if (index >= globalCandidateWords_.size()) {
        globalCandidateWords_.resize(index + 1);
    }
    globalCandidateWords_[index] =
        std::make_unique<RimeGlobalCandidateWord>(engine_, iter.candidate, idx);
    api->candidate_list_end(&iter);
    return *globalCandidateWords_[index];
}

int RimeCandidateList::totalSize() const { return -1; }

bool RimeCandidateList::hasAction(const CandidateWord &candidate) const {
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    if (dynamic_cast<const OverlayCandidateWord *>(&candidate)) {
        return false;
    }
    // We can always reset rime candidate's frequency.
    return true;
#else
    return false;
#endif
}

std::vector<CandidateAction>
RimeCandidateList::candidateActions(const CandidateWord &candidate) const {
    std::vector<CandidateAction> actions;
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    if (dynamic_cast<const OverlayCandidateWord *>(&candidate)) {
        return actions;
    }
    CandidateAction action;
    action.setId(0);
    action.setText(_("Forget word"));
    actions.push_back(std::move(action));
#endif
    return actions;
}

void RimeCandidateList::triggerAction(const CandidateWord &candidate, int id) {
    if (id != 0) {
        return;
    }
    if (auto state = engine_->state(ic_)) {
        if (const auto *rimeCandidate =
                dynamic_cast<const RimeGlobalCandidateWord *>(&candidate)) {
            rimeCandidate->forget(state);
        } else if (const auto *rimeCandidate =
                       dynamic_cast<const RimeCandidateWord *>(&candidate)) {
            rimeCandidate->forget(state);
        }
    }
}

#ifndef FCITX_RIME_NO_HIGHLIGHT_CANDIDATE
int RimeCandidateList::globalCursorIndex() const {
    return -1; // No API available.
}

void RimeCandidateList::setGlobalCursorIndex(int index) {
    auto session = engine_->state(ic_)->session(false);
    if (!session) {
        return;
    }
    auto *api = engine_->api();
    api->highlight_candidate(session, index);
}
#endif
} // namespace fcitx::rime
