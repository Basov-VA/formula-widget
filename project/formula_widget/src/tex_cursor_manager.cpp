#include "tex_cursor_manager.hpp"
#include <algorithm>
#include <cctype>

namespace formula {

    void TexCursorManager::tokenize() {
        tokens_.clear();
        glyph_to_token_.clear();

        const std::size_t n = tex_string_.size();
        std::size_t i = 0;

        while (i < n) {
            char ch = tex_string_[i];

            if (ch == '\\') {

                std::size_t start = i;
                ++i;
                if (i < n && std::isalpha(static_cast<unsigned char>(tex_string_[i]))) {

                    while (i < n && std::isalpha(static_cast<unsigned char>(tex_string_[i]))) {
                        ++i;
                    }
                } else if (i < n) {

                    ++i;
                }
                tokens_.push_back({TexTokenType::Command, start, i - start});
            } else if (ch == '{') {
                tokens_.push_back({TexTokenType::OpenBrace, i, 1});
                ++i;
            } else if (ch == '}') {
                tokens_.push_back({TexTokenType::CloseBrace, i, 1});
                ++i;
            } else if (ch == ' ') {
                tokens_.push_back({TexTokenType::Space, i, 1});
                ++i;
            } else if (ch == '^') {
                tokens_.push_back({TexTokenType::Caret, i, 1});
                ++i;
            } else if (ch == '_') {
                tokens_.push_back({TexTokenType::Underscore, i, 1});
                ++i;
            } else {

                tokens_.push_back({TexTokenType::Char, i, 1});
                ++i;
            }
        }

        rebuildGlyphMapping();
    }

    void TexCursorManager::rebuildGlyphMapping() {
        glyph_to_token_.clear();
        for (std::size_t i = 0; i < tokens_.size(); ++i) {
            if (tokens_[i].producesGlyph()) {
                glyph_to_token_.push_back(i);
            }
        }
    }

    void TexCursorManager::setTexString(const std::string& tex) {
        tex_string_ = tex;
        tokenize();
        cursor_token_pos_ = tokens_.size();
    }

    const std::string& TexCursorManager::texString() const {
        return tex_string_;
    }

    std::size_t TexCursorManager::cursorPosition() const {

        if (cursor_token_pos_ >= tokens_.size()) {
            return tex_string_.size();
        }
        return tokens_[cursor_token_pos_].start;
    }

    void TexCursorManager::setCursorPosition(std::size_t byte_pos) {

        for (std::size_t i = 0; i < tokens_.size(); ++i) {
            if (tokens_[i].start >= byte_pos) {
                cursor_token_pos_ = i;
                return;
            }
        }
        cursor_token_pos_ = tokens_.size();
    }

    void TexCursorManager::setCursorFromGlyph(std::size_t glyph_index, bool after) {
        if (glyph_index >= glyph_to_token_.size()) {
            cursor_token_pos_ = tokens_.size();
            return;
        }

        std::size_t token_idx = glyph_to_token_[glyph_index];
        if (after) {
            cursor_token_pos_ = token_idx + 1;
        } else {
            cursor_token_pos_ = token_idx;
        }
    }

    bool TexCursorManager::insertChar(char ch) {
        if (!isAllowedChar(ch)) return false;

        std::size_t byte_pos = cursorPosition();

        bool need_space_before = false;
        if (std::isalpha(static_cast<unsigned char>(ch)) && cursor_token_pos_ > 0) {
            const TexToken& prev = tokens_[cursor_token_pos_ - 1];
            if (prev.type == TexTokenType::Command) {

                if (prev.length >= 2 &&
                    std::isalpha(static_cast<unsigned char>(tex_string_[prev.start + 1]))) {
                    need_space_before = true;
                }
            }
        }

        if (need_space_before) {
            tex_string_.insert(byte_pos, 1, ' ');
            tex_string_.insert(byte_pos + 1, 1, ch);
        } else {
            tex_string_.insert(byte_pos, 1, ch);
        }

        tokenize();

        std::size_t char_pos = need_space_before ? byte_pos + 1 : byte_pos;
        for (std::size_t i = 0; i < tokens_.size(); ++i) {
            if (tokens_[i].start == char_pos) {
                cursor_token_pos_ = i + 1;
                return true;
            }
        }
        cursor_token_pos_ = tokens_.size();
        return true;
    }

    bool TexCursorManager::deleteBack() {
        if (cursor_token_pos_ == 0) return false;

        std::size_t token_idx = cursor_token_pos_ - 1;
        const TexToken& tok = tokens_[token_idx];

        tex_string_.erase(tok.start, tok.length);

        tokenize();

        cursor_token_pos_ = std::min(token_idx, tokens_.size());
        return true;
    }

    bool TexCursorManager::deleteForward() {
        if (cursor_token_pos_ >= tokens_.size()) return false;

        const TexToken& tok = tokens_[cursor_token_pos_];

        tex_string_.erase(tok.start, tok.length);

        tokenize();

        cursor_token_pos_ = std::min(cursor_token_pos_, tokens_.size());
        return true;
    }

    std::optional<std::size_t> TexCursorManager::glyphIndexAtCursor() const {
        if (glyph_to_token_.empty()) return std::nullopt;

        for (std::size_t g = 0; g < glyph_to_token_.size(); ++g) {
            if (glyph_to_token_[g] >= cursor_token_pos_) {
                return g;
            }
        }

        return std::nullopt;
    }

    bool TexCursorManager::isCursorAtEnd() const {
        return !glyphIndexAtCursor().has_value();
    }

    bool TexCursorManager::isCursorAtStart() const {
        return cursor_token_pos_ == 0;
    }

    std::size_t TexCursorManager::glyphCount() const {
        return glyph_to_token_.size();
    }

    std::optional<std::size_t> TexCursorManager::texPositionForGlyph(std::size_t glyph_index) const {
        if (glyph_index >= glyph_to_token_.size()) return std::nullopt;
        std::size_t token_idx = glyph_to_token_[glyph_index];
        return tokens_[token_idx].start;
    }

    bool TexCursorManager::isAllowedChar(char ch) {

        if (std::isalpha(static_cast<unsigned char>(ch))) return true;

        if (std::isdigit(static_cast<unsigned char>(ch))) return true;

        if (ch == '+' || ch == '-' || ch == '*' || ch == '/') return true;
        return false;
    }

}
