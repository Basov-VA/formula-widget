#include "ast_cursor.hpp"

#include <algorithm>
#include <iterator>
#include <limits>

namespace formula {

    namespace {
        constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();

        using Diff = std::vector<MathNode>::difference_type;

        inline Diff D(std::size_t x) { return static_cast<Diff>(x); }

        bool allFieldsEmpty(const MathNode& atom) {
            for (const auto& field : atom.children)
                if (!field.children.empty()) return false;
            return true;
        }
    }

    AstCursor::AstCursor() : root_(MathNode::makeRow()) {}

    AstCursor::AstCursor(MathNode root) { setRoot(std::move(root)); }

    void AstCursor::setRoot(MathNode root) {
        root_ = std::move(root);
        if (root_.kind != MathKind::Row) {
            MathNode wrapper = MathNode::makeRow();
            wrapper.children.push_back(std::move(root_));
            root_ = std::move(wrapper);
        }
        path_.clear();
        offset_ = root_.children.size();
        clearSelection();
        clearHistory();
    }

    void AstCursor::setFromTex(const std::string& tex) {
        setRoot(parseTex(tex));
    }

    MathNode& AstCursor::resolveRow(const CursorPath& p) {
        MathNode* node = &root_;
        for (std::size_t k = 0; k + 1 < p.size(); k += 2) {
            MathNode& atom = node->children[p[k]];
            node = &atom.children[p[k + 1]];
        }
        return *node;
    }

    const MathNode& AstCursor::resolveRow(const CursorPath& p) const {
        const MathNode* node = &root_;
        for (std::size_t k = 0; k + 1 < p.size(); k += 2) {
            const MathNode& atom = node->children[p[k]];
            node = &atom.children[p[k + 1]];
        }
        return *node;
    }

    void AstCursor::descend(std::size_t atomIdx, std::size_t fieldIdx, bool toEnd) {
        path_.push_back(atomIdx);
        path_.push_back(fieldIdx);
        offset_ = toEnd ? curRow().children.size() : 0;
    }

    void AstCursor::moveToEnd() {
        path_.clear();
        offset_ = root_.children.size();
    }

    void AstCursor::moveToStart() {
        path_.clear();
        offset_ = 0;
    }

    bool AstCursor::inEmptyRow() const {
        return curRow().children.empty();
    }

    std::optional<FieldContext> AstCursor::fieldContext() const {
        if (path_.empty()) return std::nullopt;

        const CursorPath parent(path_.begin(), path_.end() - 2);
        const std::size_t atomIdx = path_[path_.size() - 2];
        const std::size_t fieldIdx = path_.back();

        std::size_t before = 0;
        const MathNode* node = &root_;
        for (std::size_t k = 0; k + 1 < parent.size(); k += 2) {
            const std::size_t ai = parent[k];
            const std::size_t fi = parent[k + 1];
            for (std::size_t a = 0; a < ai; ++a) before += glyphSpan(node->children[a]);
            const MathNode& at = node->children[ai];
            before += glyphsBeforeField(at, fi);
            node = &at.children[fi];
        }
        for (std::size_t a = 0; a < atomIdx; ++a) before += glyphSpan(node->children[a]);

        const MathNode& atom = node->children[atomIdx];
        FieldContext fc;
        fc.structKind = atom.kind;
        fc.fieldIndex = fieldIdx;
        fc.command = atom.command;
        fc.has_sup = atom.has_sup;
        fc.has_sub = atom.has_sub;
        fc.glyphStart = before;
        fc.glyphCount = glyphSpan(atom);
        return fc;
    }

    std::optional<AstCursor::CaretAnchor> AstCursor::caretAnchor() const {
        const MathNode& row = curRow();
        if (row.children.empty()) return std::nullopt;

        std::size_t rowStart = 0;
        {
            const MathNode* node = &root_;
            for (std::size_t k = 0; k + 1 < path_.size(); k += 2) {
                const std::size_t a = path_[k];
                const std::size_t f = path_[k + 1];
                for (std::size_t x = 0; x < a; ++x) rowStart += glyphSpan(node->children[x]);
                const MathNode& at = node->children[a];
                rowStart += glyphsBeforeField(at, f);
                node = &at.children[f];
            }
        }

        auto glyphsBefore = [&](std::size_t idx) {
            std::size_t s = rowStart;
            for (std::size_t x = 0; x < idx; ++x) s += glyphSpan(row.children[x]);
            return s;
        };

        auto makeAnchor = [&](std::size_t idx, bool after) -> CaretAnchor {
            const MathNode& atom = row.children[idx];
            const std::size_t span = glyphSpan(atom);
            CaretAnchor a;
            a.after = after;
            if (atom.isStructural()) {
                a.isStruct = true;
                a.structCtx.structKind = atom.kind;
                a.structCtx.fieldIndex = 0;
                a.structCtx.has_sup = atom.has_sup;
                a.structCtx.has_sub = atom.has_sub;
                a.structCtx.glyphStart = glyphsBefore(idx);
                a.structCtx.glyphCount = span;
            } else {

                a.isStruct = false;
                a.glyphIndex = after ? glyphsBefore(idx) + span - 1 : glyphsBefore(idx);
            }
            return a;
        };

        const std::size_t n = row.children.size();
        auto leftLeaf  = [&](std::size_t i) { return row.children[i].isLeaf() && glyphSpan(row.children[i]) > 0; };
        auto rightLeaf = leftLeaf;

        auto leftStruct  = [&](std::size_t i) { return row.children[i].isStructural(); };
        auto rightStruct = leftStruct;

        if (offset_ > 0 && leftLeaf(offset_ - 1))   return makeAnchor(offset_ - 1, true);
        if (offset_ < n && rightLeaf(offset_))      return makeAnchor(offset_,     false);
        if (offset_ > 0 && leftStruct(offset_ - 1)) return makeAnchor(offset_ - 1, true);
        if (offset_ < n && rightStruct(offset_))    return makeAnchor(offset_,     false);
        for (std::size_t i = offset_; i-- > 0;)
            if (glyphSpan(row.children[i]) > 0) return makeAnchor(i, true);
        for (std::size_t i = offset_; i < n; ++i)
            if (glyphSpan(row.children[i]) > 0) return makeAnchor(i, false);
        return std::nullopt;
    }

    bool AstCursor::move(Dir d) {
        lastWasTyping_ = false;
        if (d == Dir::Up || d == Dir::Down) {

            CursorPath p = path_;
            std::size_t carriedOffset = offset_;
            while (!p.empty()) {
                const std::size_t fieldIdx = p.back();
                const std::size_t atomIdx = p[p.size() - 2];
                CursorPath parent(p.begin(), p.end() - 2);
                const MathNode& atom = resolveRow(parent).children[atomIdx];
                const std::size_t target = verticalTarget(atom, fieldIdx, d);
                if (target != kNone) {
                    path_ = parent;
                    path_.push_back(atomIdx);
                    path_.push_back(target);
                    offset_ = std::min(carriedOffset, curRow().children.size());
                    return true;
                }
                p = parent;
            }
            return false;
        }

        MathNode& row = curRow();
        if (d == Dir::Right) {
            if (offset_ < row.children.size()) {
                MathNode& atom = row.children[offset_];
                if (atom.isStructural()) {
                    descend(offset_, 0, false);
                } else {
                    ++offset_;
                }
                return true;
            }

            if (path_.empty()) return false;
            const std::size_t fieldIdx = path_.back();
            const std::size_t atomIdx = path_[path_.size() - 2];
            CursorPath parent(path_.begin(), path_.end() - 2);
            const MathNode& atom = resolveRow(parent).children[atomIdx];
            if (fieldIdx + 1 < atom.children.size()) {
                path_.back() = fieldIdx + 1;
                offset_ = 0;
            } else {
                path_ = parent;
                offset_ = atomIdx + 1;
            }
            return true;
        }

        if (offset_ > 0) {
            MathNode& atom = row.children[offset_ - 1];
            if (atom.isStructural()) {
                descend(offset_ - 1, atom.children.size() - 1, true);
            } else {
                --offset_;
            }
            return true;
        }

        if (path_.empty()) return false;
        const std::size_t fieldIdx = path_.back();
        const std::size_t atomIdx = path_[path_.size() - 2];
        CursorPath parent(path_.begin(), path_.end() - 2);
        if (fieldIdx > 0) {
            path_.back() = fieldIdx - 1;
            offset_ = curRow().children.size();
        } else {
            path_ = parent;
            offset_ = atomIdx;
        }
        return true;
    }

    std::size_t AstCursor::verticalTarget(const MathNode& atom, std::size_t fieldIdx, Dir d) {
        if (atom.kind == MathKind::Matrix) {

            const std::size_t cols = atom.matrixCols ? atom.matrixCols : 1;
            if (d == Dir::Up   && fieldIdx >= cols)                     return fieldIdx - cols;
            if (d == Dir::Down && fieldIdx + cols < atom.children.size()) return fieldIdx + cols;
            return kNone;
        }
        if (atom.kind == MathKind::Frac) {

            if (d == Dir::Up   && fieldIdx == 1) return 0;
            if (d == Dir::Down && fieldIdx == 0) return 1;
            return kNone;
        }
        if (atom.kind == MathKind::Script) {
            const std::size_t sup = scriptSupIndex(atom);
            const std::size_t sub = scriptSubIndex(atom);

            if (d == Dir::Up) {
                if (fieldIdx == 0 && sup != kNone) return sup;
                if (fieldIdx == sub) return 0;
            } else {
                if (fieldIdx == 0 && sub != kNone) return sub;
                if (fieldIdx == sup) return 0;
            }
            return kNone;
        }
        return kNone;
    }

    bool AstCursor::setFromGlyph(std::size_t glyphIndex, bool after) {
        lastWasTyping_ = false;
        const std::size_t total = glyphSpan(root_);
        if (glyphIndex >= total) { moveToEnd(); return true; }

        path_.clear();
        std::size_t target = glyphIndex;
        MathNode* node = &root_;
        for (;;) {
            bool descended = false;
            for (std::size_t a = 0; a < node->children.size(); ++a) {
                MathNode& child = node->children[a];
                if (child.isLeaf()) {
                    const std::size_t span = glyphSpan(child);
                    if (span > 0 && target < span) {

                        const bool putAfter = after || (target * 2 >= span);
                        offset_ = a + (putAfter ? 1u : 0u);
                        return true;
                    }
                    target -= span;
                } else {
                    const std::size_t lc = glyphSpan(child);
                    if (target < lc) {

                        const std::size_t lead = leadingChromeGlyphs(child);
                        if (target < lead) { offset_ = a; return true; }
                        target -= lead;

                        for (std::size_t f = 0; f < child.children.size(); ++f) {
                            const std::size_t flc = glyphSpan(child.children[f]);
                            if (target < flc) {
                                path_.push_back(a);
                                path_.push_back(f);
                                node = &child.children[f];
                                descended = true;
                                break;
                            }
                            target -= flc;
                        }
                        break;
                    }
                    target -= lc;
                }
            }
            if (!descended) {
                offset_ = node->children.size();
                return true;
            }
        }
    }

    bool AstCursor::tabNextField() {
        lastWasTyping_ = false;
        CursorPath p = path_;
        while (!p.empty()) {
            const std::size_t fieldIdx = p.back();
            const std::size_t atomIdx = p[p.size() - 2];
            CursorPath parent(p.begin(), p.end() - 2);
            const MathNode& atom = resolveRow(parent).children[atomIdx];
            if (fieldIdx + 1 < atom.children.size()) {
                path_ = parent;
                path_.push_back(atomIdx);
                path_.push_back(fieldIdx + 1);
                offset_ = 0;
                return true;
            }
            p = parent;
        }

        moveToEnd();
        return true;
    }

    bool AstCursor::insertChar(char ch) {
        MathNode& row = curRow();
        row.children.insert(row.children.begin() + D(offset_), MathNode::makeChar(ch));
        ++offset_;
        return true;
    }

    bool AstCursor::insertSymbol(const std::string& command) {
        MathNode& row = curRow();
        row.children.insert(row.children.begin() + D(offset_), MathNode::makeSymbol(command));
        ++offset_;
        return true;
    }

    bool AstCursor::insertFraction() {
        MathNode frac{.kind = MathKind::Frac};
        frac.children.push_back(MathNode::makeRow());
        frac.children.push_back(MathNode::makeRow());
        MathNode& row = curRow();
        const std::size_t at = offset_;
        row.children.insert(row.children.begin() + D(at), std::move(frac));
        descend(at, 0, false);
        return true;
    }

    bool AstCursor::insertSqrt() {
        MathNode sq{.kind = MathKind::Sqrt};
        sq.children.push_back(MathNode::makeRow());
        MathNode& row = curRow();
        const std::size_t at = offset_;
        row.children.insert(row.children.begin() + D(at), std::move(sq));
        descend(at, 0, false);
        return true;
    }

    bool AstCursor::insertGroup() {
        MathNode g{.kind = MathKind::Group};
        g.children.push_back(MathNode::makeRow());
        MathNode& row = curRow();
        const std::size_t at = offset_;
        row.children.insert(row.children.begin() + D(at), std::move(g));
        descend(at, 0, false);
        return true;
    }

    bool AstCursor::insertBigOperator(const std::string& command) {
        MathNode script{.kind = MathKind::Script};
        MathNode baseRow = MathNode::makeRow();
        baseRow.children.push_back(MathNode::makeSymbol(command));
        script.children.push_back(std::move(baseRow));
        script.children.push_back(MathNode::makeRow());
        script.children.push_back(MathNode::makeRow());
        script.has_sup = true;
        script.has_sub = true;

        MathNode& row = curRow();
        const std::size_t at = offset_;
        row.children.insert(row.children.begin() + D(at), std::move(script));
        descend(at, 2, false);
        return true;
    }

    bool AstCursor::insertAccent(const std::string& command) {
        MathNode a = MathNode::makeAccent(command);
        MathNode& row = curRow();
        const std::size_t at = offset_;
        row.children.insert(row.children.begin() + D(at), std::move(a));
        descend(at, 0, false);
        return true;
    }

    bool AstCursor::insertMatrix(std::size_t rows, std::size_t cols, char open, char close) {
        if (rows == 0) rows = 1;
        if (cols == 0) cols = 1;
        MathNode m = MathNode::makeMatrix(rows, cols);
        m.matrixOpen = open;
        m.matrixClose = close;
        MathNode& row = curRow();
        const std::size_t at = offset_;
        row.children.insert(row.children.begin() + D(at), std::move(m));
        descend(at, 0, false);
        return true;
    }

    bool AstCursor::tabPrevField() {
        lastWasTyping_ = false;
        CursorPath p = path_;
        while (!p.empty()) {
            const std::size_t fieldIdx = p.back();
            const std::size_t atomIdx = p[p.size() - 2];
            CursorPath parent(p.begin(), p.end() - 2);
            if (fieldIdx > 0) {
                path_ = parent;
                path_.push_back(atomIdx);
                path_.push_back(fieldIdx - 1);
                offset_ = 0;
                return true;
            }
            p = parent;
        }
        moveToStart();
        return true;
    }

    bool AstCursor::insertScript(bool superscript) {
        MathNode& row = curRow();

        if (offset_ > 0 && row.children[offset_ - 1].kind == MathKind::Script) {
            MathNode& script = row.children[offset_ - 1];
            if (superscript) {
                if (!script.has_sup) {
                    script.children.insert(script.children.begin() + 1, MathNode::makeRow());
                    script.has_sup = true;
                }
                descend(offset_ - 1, scriptSupIndex(script), false);
            } else {
                if (!script.has_sub) {
                    script.children.push_back(MathNode::makeRow());
                    script.has_sub = true;
                }
                descend(offset_ - 1, scriptSubIndex(script), false);
            }
            return true;
        }

        if (offset_ > 0) {
            MathNode base = std::move(row.children[offset_ - 1]);
            MathNode script{.kind = MathKind::Script};
            MathNode baseRow = MathNode::makeRow();
            baseRow.children.push_back(std::move(base));
            script.children.push_back(std::move(baseRow));
            script.children.push_back(MathNode::makeRow());
            if (superscript) script.has_sup = true; else script.has_sub = true;
            row.children[offset_ - 1] = std::move(script);
            descend(offset_ - 1, 1, false);
            return true;
        }

        MathNode script{.kind = MathKind::Script};
        script.children.push_back(MathNode::makeRow());
        script.children.push_back(MathNode::makeRow());
        if (superscript) script.has_sup = true; else script.has_sub = true;
        row.children.insert(row.children.begin() + D(offset_), std::move(script));
        descend(offset_, 1, false);
        return true;
    }

    bool AstCursor::deleteBack() {
        MathNode& row = curRow();
        if (offset_ > 0) {

            row.children.erase(row.children.begin() + D(offset_ - 1));
            --offset_;
            return true;
        }

        if (path_.empty()) return false;
        const std::size_t atomIdx = path_[path_.size() - 2];
        CursorPath parent(path_.begin(), path_.end() - 2);
        MathNode& parentRow = resolveRow(parent);
        MathNode& atom = parentRow.children[atomIdx];
        if (allFieldsEmpty(atom)) {
            parentRow.children.erase(parentRow.children.begin() + D(atomIdx));
        }
        path_ = parent;
        offset_ = atomIdx;
        return true;
    }

    bool AstCursor::deleteForward() {
        MathNode& row = curRow();
        if (offset_ < row.children.size()) {

            row.children.erase(row.children.begin() + D(offset_));
            return true;
        }

        if (path_.empty()) return false;
        const std::size_t fieldIdx = path_.back();
        const std::size_t atomIdx = path_[path_.size() - 2];
        CursorPath parent(path_.begin(), path_.end() - 2);
        MathNode& parentRow = resolveRow(parent);
        MathNode& atom = parentRow.children[atomIdx];
        if (fieldIdx + 1 < atom.children.size()) {
            path_.back() = fieldIdx + 1;
            offset_ = 0;
        } else {
            if (allFieldsEmpty(atom))
                parentRow.children.erase(parentRow.children.begin() + D(atomIdx));
            path_ = parent;
            offset_ = atomIdx;
        }
        return true;
    }

    void AstCursor::snapshot(bool typing) {

        if (typing && lastWasTyping_ && !undo_.empty()) {
            redo_.clear();
            return;
        }
        undo_.push_back(Snapshot{root_, path_, offset_});
        if (undo_.size() > kHistoryLimit)
            undo_.erase(undo_.begin());
        redo_.clear();
        lastWasTyping_ = typing;
    }

    bool AstCursor::undo() {
        if (undo_.empty()) return false;
        redo_.push_back(Snapshot{root_, path_, offset_});
        Snapshot s = std::move(undo_.back());
        undo_.pop_back();
        root_ = std::move(s.root);
        path_ = std::move(s.path);
        offset_ = s.offset;
        lastWasTyping_ = false;
        clearSelection();
        return true;
    }

    bool AstCursor::redo() {
        if (redo_.empty()) return false;
        undo_.push_back(Snapshot{root_, path_, offset_});
        Snapshot s = std::move(redo_.back());
        redo_.pop_back();
        root_ = std::move(s.root);
        path_ = std::move(s.path);
        offset_ = s.offset;
        lastWasTyping_ = false;
        clearSelection();
        return true;
    }

    void AstCursor::clearHistory() {
        undo_.clear();
        redo_.clear();
        lastWasTyping_ = false;
    }

    void AstCursor::clearSelection() { selActive_ = false; }

    bool AstCursor::hasSelection() const {
        return selActive_ && selPath_ == path_ && selAnchor_ != offset_;
    }

    std::optional<std::pair<std::size_t, std::size_t>> AstCursor::selectionOffsets() const {
        if (!hasSelection()) return std::nullopt;
        return std::make_pair(std::min(selAnchor_, offset_), std::max(selAnchor_, offset_));
    }

    bool AstCursor::extendSelection(Dir d) {
        lastWasTyping_ = false;
        if (d != Dir::Left && d != Dir::Right) return false;

        if (!selActive_ || selPath_ != path_) {
            selActive_ = true;
            selPath_ = path_;
            selAnchor_ = offset_;
        }
        const MathNode& row = curRow();
        if (d == Dir::Right) {
            if (offset_ >= row.children.size()) return false;
            ++offset_;
            return true;
        }
        if (offset_ == 0) return false;
        --offset_;
        return true;
    }

    void AstCursor::selectAll() {
        lastWasTyping_ = false;
        const MathNode& row = curRow();
        if (row.children.empty()) { clearSelection(); return; }
        selActive_ = true;
        selPath_ = path_;
        selAnchor_ = 0;
        offset_ = row.children.size();
    }

    std::optional<std::pair<std::size_t, std::size_t>> AstCursor::selectedGlyphRange() const {
        const auto off = selectionOffsets();
        if (!off) return std::nullopt;
        const MathNode& row = curRow();

        std::size_t rowStart = 0;
        const MathNode* node = &root_;
        for (std::size_t k = 0; k + 1 < path_.size(); k += 2) {
            const std::size_t a = path_[k];
            const std::size_t f = path_[k + 1];
            for (std::size_t x = 0; x < a; ++x) rowStart += glyphSpan(node->children[x]);
            const MathNode& at = node->children[a];
            rowStart += glyphsBeforeField(at, f);
            node = &at.children[f];
        }
        std::size_t g0 = rowStart;
        for (std::size_t i = 0; i < off->first; ++i) g0 += glyphSpan(row.children[i]);
        std::size_t g1 = g0;
        for (std::size_t i = off->first; i < off->second; ++i) g1 += glyphSpan(row.children[i]);
        if (g0 == g1) return std::nullopt;
        return std::make_pair(g0, g1);
    }

    std::string AstCursor::selectedTex() const {
        const auto off = selectionOffsets();
        if (!off) return {};
        const MathNode& row = curRow();
        MathNode tmp = MathNode::makeRow();
        for (std::size_t i = off->first; i < off->second; ++i)
            tmp.children.push_back(row.children[i]);
        return formula::toTex(tmp);
    }

    bool AstCursor::deleteSelection() {
        const auto off = selectionOffsets();
        if (!off) return false;
        MathNode& row = curRow();
        row.children.erase(row.children.begin() + D(off->first),
                           row.children.begin() + D(off->second));
        offset_ = off->first;
        clearSelection();
        return true;
    }

    bool AstCursor::insertTex(const std::string& tex) {
        if (hasSelection()) deleteSelection();
        MathNode parsed = parseTex(tex);
        if (parsed.children.empty()) return false;
        const std::size_t n = parsed.children.size();
        MathNode& row = curRow();
        row.children.insert(row.children.begin() + D(offset_),
                            std::make_move_iterator(parsed.children.begin()),
                            std::make_move_iterator(parsed.children.end()));
        offset_ += n;
        return true;
    }

    std::size_t AstCursor::countLeavesBeforeCursor() const {
        std::size_t before = 0;
        const MathNode* node = &root_;
        for (std::size_t k = 0; k + 1 < path_.size(); k += 2) {
            const std::size_t atomIdx = path_[k];
            const std::size_t fieldIdx = path_[k + 1];
            for (std::size_t a = 0; a < atomIdx; ++a)
                before += glyphSpan(node->children[a]);
            const MathNode& atom = node->children[atomIdx];
            before += glyphsBeforeField(atom, fieldIdx);
            node = &atom.children[fieldIdx];
        }
        for (std::size_t a = 0; a < offset_ && a < node->children.size(); ++a)
            before += glyphSpan(node->children[a]);
        return before;
    }

    std::optional<std::size_t> AstCursor::glyphIndexAtCursor() const {
        const std::size_t total = glyphSpan(root_);
        if (total == 0) return std::nullopt;
        const std::size_t before = countLeavesBeforeCursor();
        if (before < total) return before;
        return std::nullopt;
    }

    bool AstCursor::cursorAfterGlyph() const {
        const std::size_t total = glyphSpan(root_);
        if (total == 0) return false;
        return countLeavesBeforeCursor() >= total;
    }

}
