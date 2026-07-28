#pragma once

#include "math_ast.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace formula {

    enum class Dir { Left, Right, Up, Down };

    using CursorPath = std::vector<std::size_t>;

    struct FieldContext {
        MathKind structKind;
        std::size_t fieldIndex;
        std::string command;
        bool has_sup = false;
        bool has_sub = false;
        std::size_t glyphStart;
        std::size_t glyphCount;
    };

    class AstCursor {
    public:
        AstCursor();
        explicit AstCursor(MathNode root);

        void setRoot(MathNode root);

        void setFromTex(const std::string& tex);

        [[nodiscard]] const MathNode& root() const { return root_; }
        [[nodiscard]] std::string toTex() const { return formula::toTex(root_); }

        [[nodiscard]] std::string toRenderTex() const { return formula::toRenderTex(root_); }

        bool move(Dir d);
        bool tabNextField();
        bool tabPrevField();
        void moveToEnd();
        void moveToStart();

        bool setFromGlyph(std::size_t glyphIndex, bool after);

        void snapshot(bool typing = false);
        bool undo();
        bool redo();
        [[nodiscard]] bool canUndo() const { return !undo_.empty(); }
        [[nodiscard]] bool canRedo() const { return !redo_.empty(); }

        void discardLastUndo() { if (!undo_.empty()) undo_.pop_back(); }

        void clearHistory();

        bool insertChar(char ch);
        bool insertSymbol(const std::string& command);
        bool insertFraction();
        bool insertSqrt();
        bool insertScript(bool superscript);
        bool insertGroup();

        bool insertBigOperator(const std::string& command);

        bool insertAccent(const std::string& command);

        bool insertMatrix(std::size_t rows, std::size_t cols, char open = 0, char close = 0);
        bool deleteBack();
        bool deleteForward();

        bool extendSelection(Dir d);
        void selectAll();
        void clearSelection();
        [[nodiscard]] bool hasSelection() const;

        [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> selectionOffsets() const;

        [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> selectedGlyphRange() const;

        [[nodiscard]] std::string selectedTex() const;

        bool deleteSelection();

        bool insertTex(const std::string& tex);

        [[nodiscard]] std::optional<std::size_t> glyphIndexAtCursor() const;

        [[nodiscard]] bool cursorAfterGlyph() const;
        [[nodiscard]] std::size_t glyphCount() const { return glyphSpan(root_); }

        [[nodiscard]] bool inEmptyRow() const;

        [[nodiscard]] std::optional<FieldContext> fieldContext() const;

        struct CaretAnchor {
            bool isStruct = false;
            bool after = false;
            std::size_t glyphIndex = 0;
            FieldContext structCtx;
        };

        [[nodiscard]] std::optional<CaretAnchor> caretAnchor() const;

        [[nodiscard]] const CursorPath& path() const { return path_; }
        [[nodiscard]] std::size_t offset() const { return offset_; }
        [[nodiscard]] std::size_t depth() const { return path_.size() / 2; }

    private:
        MathNode root_;
        CursorPath path_;
        std::size_t offset_ = 0;

        struct Snapshot {
            MathNode root;
            CursorPath path;
            std::size_t offset;
        };
        std::vector<Snapshot> undo_;
        std::vector<Snapshot> redo_;
        bool lastWasTyping_ = false;
        static constexpr std::size_t kHistoryLimit = 500;

        bool selActive_ = false;
        CursorPath selPath_;
        std::size_t selAnchor_ = 0;

        MathNode& resolveRow(const CursorPath& p);
        const MathNode& resolveRow(const CursorPath& p) const;
        MathNode& curRow() { return resolveRow(path_); }
        const MathNode& curRow() const { return resolveRow(path_); }

        void descend(std::size_t atomIdx, std::size_t fieldIdx, bool toEnd);

        static std::size_t verticalTarget(const MathNode& atom, std::size_t fieldIdx, Dir d);

        std::size_t countLeavesBeforeCursor() const;
    };

}
