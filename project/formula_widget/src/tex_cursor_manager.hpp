#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace formula {

    enum class TexTokenType {
        Char,
        Command,
        OpenBrace,
        CloseBrace,
        Space,
        Caret,
        Underscore,
    };

    struct TexToken {
        TexTokenType type;
        std::size_t start;
        std::size_t length;

        bool producesGlyph() const {
            return type == TexTokenType::Char;
        }
    };

    class TexCursorManager {
    public:
        TexCursorManager() = default;

        void setTexString(const std::string& tex);

        const std::string& texString() const;

        std::size_t cursorPosition() const;

        void setCursorPosition(std::size_t byte_pos);

        void setCursorFromGlyph(std::size_t glyph_index, bool after = false);

        bool insertChar(char ch);

        bool deleteBack();

        bool deleteForward();

        std::optional<std::size_t> glyphIndexAtCursor() const;

        bool isCursorAtEnd() const;

        bool isCursorAtStart() const;

        std::size_t glyphCount() const;

        std::optional<std::size_t> texPositionForGlyph(std::size_t glyph_index) const;

        static bool isAllowedChar(char ch);

        bool isInsideTexCommand() const { return false; }

        const std::vector<TexToken>& tokens() const { return tokens_; }

        std::size_t cursorTokenPos() const { return cursor_token_pos_; }

    private:

        void tokenize();

        void rebuildGlyphMapping();

        std::string tex_string_;
        std::size_t cursor_token_pos_ = 0;

        std::vector<TexToken> tokens_;

        std::vector<std::size_t> glyph_to_token_;
    };

}
