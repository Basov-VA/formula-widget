#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace formula {

    enum class MathKind {
        Row,
        Char,
        Symbol,
        Group,
        Frac,
        Sqrt,
        Script,
        Accent,
        Matrix,
    };

    struct MathNode {
        MathKind kind = MathKind::Row;
        char ch = '\0';
        std::string command;
        bool has_sup = false;
        bool has_sub = false;
        std::size_t matrixCols = 0;
        char matrixOpen = 0;
        char matrixClose = 0;
        std::vector<MathNode> children;

        [[nodiscard]] bool isLeaf() const {
            return kind == MathKind::Char || kind == MathKind::Symbol;
        }
        [[nodiscard]] bool isStructural() const {
            return kind == MathKind::Group  || kind == MathKind::Frac
                || kind == MathKind::Sqrt   || kind == MathKind::Script
                || kind == MathKind::Accent || kind == MathKind::Matrix;
        }

        [[nodiscard]] std::size_t fieldCount() const {
            return isStructural() ? children.size() : 0;
        }

        static MathNode makeRow() { return MathNode{.kind = MathKind::Row}; }
        static MathNode makeChar(char c) { return MathNode{.kind = MathKind::Char, .ch = c}; }
        static MathNode makeSymbol(std::string cmd) {
            MathNode n{.kind = MathKind::Symbol};
            n.command = std::move(cmd);
            return n;
        }
        static MathNode makeAccent(std::string cmd) {
            MathNode n{.kind = MathKind::Accent};
            n.command = std::move(cmd);
            n.children.push_back(makeRow());
            return n;
        }
        static MathNode makeMatrix(std::size_t rows, std::size_t cols) {
            MathNode n{.kind = MathKind::Matrix};
            n.matrixCols = cols;
            for (std::size_t i = 0; i < rows * cols; ++i) n.children.push_back(makeRow());
            return n;
        }
    };

    bool accentHasMarkGlyph(const std::string& command);

    std::size_t leadingChromeGlyphs(const MathNode& atom);

    std::size_t glyphsBeforeField(const MathNode& atom, std::size_t fieldIdx);

    std::size_t scriptSupIndex(const MathNode& script);

    std::size_t scriptSubIndex(const MathNode& script);

    std::size_t glyphSpan(const MathNode& node);

    std::size_t symbolGlyphCount(const std::string& command);

    std::string toTex(const MathNode& root);

    std::string toRenderTex(const MathNode& root);

    MathNode parseTex(const std::string& tex);

}
