#include "math_ast.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>

namespace formula {

    namespace {
        bool isAlphaByte(char c) {
            return std::isalpha(static_cast<unsigned char>(c)) != 0;
        }
        bool isCommandAlphabetic(const std::string& cmd) {
            return !cmd.empty() && isAlphaByte(cmd[0]);
        }
    }

    std::size_t scriptSupIndex(const MathNode& script) {

        return script.has_sup ? 1u : std::numeric_limits<std::size_t>::max();
    }

    std::size_t scriptSubIndex(const MathNode& script) {
        if (!script.has_sub) return std::numeric_limits<std::size_t>::max();
        return script.has_sup ? 2u : 1u;
    }

    std::size_t symbolGlyphCount(const std::string& command) {

        static const std::set<std::string> zero = {
            ",", "!", ";", ":", " ", "quad", "qquad",
            "thinspace", "medspace", "thickspace", "negthinspace",
        };
        if (zero.count(command)) return 0;

        static const std::set<std::string> funcs = {
            "sin", "cos", "tan", "cot", "sec", "csc",
            "sinh", "cosh", "tanh", "coth",
            "arcsin", "arccos", "arctan",
            "ln", "log", "lg", "exp", "lim", "limsup", "liminf",
            "max", "min", "sup", "inf", "det", "gcd",
            "deg", "dim", "ker", "hom", "arg", "mod", "bmod",
        };
        if (funcs.count(command)) return command.size();
        return 1;
    }

    bool accentHasMarkGlyph(const std::string& command) {

        return command != "overline" && command != "underline";
    }

    std::size_t leadingChromeGlyphs(const MathNode& atom) {
        if (atom.kind == MathKind::Accent && accentHasMarkGlyph(atom.command)) return 1;
        if (atom.kind == MathKind::Matrix && atom.matrixOpen) return 1;
        return 0;
    }

    static std::size_t trailingChromeGlyphs(const MathNode& atom) {
        if (atom.kind == MathKind::Matrix && atom.matrixClose) return 1;
        return 0;
    }

    std::size_t glyphSpan(const MathNode& node) {
        if (node.kind == MathKind::Char) return 1;
        if (node.kind == MathKind::Symbol) return symbolGlyphCount(node.command);
        std::size_t total = leadingChromeGlyphs(node) + trailingChromeGlyphs(node);
        for (const auto& c : node.children) total += glyphSpan(c);
        return total;
    }

    std::size_t glyphsBeforeField(const MathNode& atom, std::size_t fieldIdx) {
        std::size_t before = leadingChromeGlyphs(atom);
        for (std::size_t f = 0; f < fieldIdx && f < atom.children.size(); ++f)
            before += glyphSpan(atom.children[f]);
        return before;
    }

    namespace {
        std::string firstTexChar(const MathNode& n);

        bool needsSeparatingSpace(const MathNode& prev, const MathNode& next) {
            if (prev.kind != MathKind::Symbol) return false;
            if (!isCommandAlphabetic(prev.command)) return false;
            const std::string nf = firstTexChar(next);
            return !nf.empty() && isAlphaByte(nf[0]);
        }

        std::string firstTexChar(const MathNode& n) {
            switch (n.kind) {
                case MathKind::Char:   return std::string(1, n.ch);
                case MathKind::Symbol: return "\\";
                case MathKind::Group:  return "{";
                case MathKind::Frac:   return "\\";
                case MathKind::Sqrt:   return "\\";
                case MathKind::Accent: return "\\";
                case MathKind::Matrix: return "\\";
                case MathKind::Script:
                    return n.children.empty() ? std::string()
                                              : firstTexChar(n.children[0]);
                case MathKind::Row:
                    return n.children.empty() ? std::string()
                                              : firstTexChar(n.children[0]);
            }
            return {};
        }

        std::string serialize(const MathNode& node, bool render);

        std::string fieldTex(const MathNode& row, bool render) {
            if (render && row.children.empty()) return "\\enspace";
            return serialize(row, render);
        }

        std::string baseToTex(const MathNode& baseRow, bool render) {
            if (render && baseRow.children.empty()) return "{\\enspace}";
            if (baseRow.children.size() == 1) {
                const MathNode& only = baseRow.children[0];
                if (only.kind == MathKind::Char)   return std::string(1, only.ch);
                if (only.kind == MathKind::Symbol) return "\\" + only.command;
                if (only.kind == MathKind::Group)  return serialize(only, render);
            }
            return "{" + serialize(baseRow, render) + "}";
        }

        std::string serialize(const MathNode& node, bool render) {
            switch (node.kind) {
                case MathKind::Char:
                    return std::string(1, node.ch);

                case MathKind::Symbol:
                    return "\\" + node.command;

                case MathKind::Group:
                    return "{" + fieldTex(node.children[0], render) + "}";

                case MathKind::Frac:
                    return "\\frac{" + fieldTex(node.children[0], render) + "}{"
                                     + fieldTex(node.children[1], render) + "}";

                case MathKind::Sqrt:
                    return "\\sqrt{" + fieldTex(node.children[0], render) + "}";

                case MathKind::Accent:
                    return "\\" + node.command + "{" + fieldTex(node.children[0], render) + "}";

                case MathKind::Matrix: {
                    const std::size_t cols = node.matrixCols ? node.matrixCols : 1;

                    const auto delim = [](char c) -> std::string {
                        if (c == 0 || c == '.') return ".";
                        if (c == '{') return "\\{";
                        if (c == '}') return "\\}";
                        return std::string(1, c);
                    };
                    std::string body = "\\matrix{";
                    for (std::size_t i = 0; i < node.children.size(); ++i) {
                        if (i > 0) body += (i % cols == 0) ? " \\cr " : " & ";
                        body += fieldTex(node.children[i], render);
                    }
                    body += "}";
                    if (node.matrixOpen || node.matrixClose)
                        return "\\left" + delim(node.matrixOpen) + body
                             + "\\right" + delim(node.matrixClose);
                    return body;
                }

                case MathKind::Script: {
                    std::string r = baseToTex(node.children[0], render);
                    const std::size_t sup = scriptSupIndex(node);
                    const std::size_t sub = scriptSubIndex(node);
                    if (sup != std::numeric_limits<std::size_t>::max())
                        r += "^{" + fieldTex(node.children[sup], render) + "}";
                    if (sub != std::numeric_limits<std::size_t>::max())
                        r += "_{" + fieldTex(node.children[sub], render) + "}";
                    return r;
                }

                case MathKind::Row: {
                    std::string r;
                    for (std::size_t i = 0; i < node.children.size(); ++i) {
                        if (i > 0 && needsSeparatingSpace(node.children[i - 1], node.children[i]))
                            r += ' ';
                        r += serialize(node.children[i], render);
                    }
                    return r;
                }
            }
            return {};
        }
    }

    std::string toTex(const MathNode& node) { return serialize(node, false); }

    std::string toRenderTex(const MathNode& node) { return serialize(node, true); }

    namespace {
        struct Parser {
            const std::string& s;
            std::size_t i = 0;

            explicit Parser(const std::string& str) : s(str) {}

            bool eof() const { return i >= s.size(); }
            char peek() const { return i < s.size() ? s[i] : '\0'; }

            void skipSpaces() {
                while (i < s.size() && s[i] == ' ') ++i;
            }

            std::string readCommand() {
                ++i;
                std::string name;
                if (i < s.size() && isAlphaByte(s[i])) {
                    while (i < s.size() && isAlphaByte(s[i])) name += s[i++];
                } else if (i < s.size()) {
                    name += s[i++];
                }
                return name;
            }

            char readDelim() {
                skipSpaces();
                if (peek() == '\\') {
                    const std::string nm = readCommand();
                    if (nm == "{") return '{';
                    if (nm == "}") return '}';
                    if (nm == "|") return '|';
                    return 0;
                }
                if (eof()) return 0;
                const char c = s[i++];
                return c == '.' ? char(0) : c;
            }

            MathNode parseMatrixBody() {
                skipSpaces();
                MathNode m{.kind = MathKind::Matrix};
                std::vector<std::vector<MathNode>> rows;
                std::vector<MathNode> cur;
                if (peek() == '{') {
                    ++i;
                    for (;;) {
                        cur.push_back(parseRow(true, true));
                        skipSpaces();
                        const char nc = peek();
                        if (nc == '&') { ++i; continue; }
                        if (nc == '\\') {
                            const std::size_t save = i;
                            if (readCommand() == "cr") {
                                rows.push_back(std::move(cur));
                                cur.clear();
                                continue;
                            }
                            i = save;
                        }
                        break;
                    }
                    if (peek() == '}') ++i;
                }
                rows.push_back(std::move(cur));
                std::size_t cols = 0;
                for (const auto& r : rows) cols = std::max(cols, r.size());
                if (cols == 0) cols = 1;
                m.matrixCols = cols;
                for (auto& r : rows)
                    for (std::size_t k = 0; k < cols; ++k)
                        m.children.push_back(k < r.size() ? std::move(r[k]) : MathNode::makeRow());
                return m;
            }

            MathNode parseArg() {
                skipSpaces();
                MathNode row = MathNode::makeRow();
                if (peek() == '{') {
                    ++i;
                    row = parseRow(true);
                    if (peek() == '}') ++i;
                } else if (!eof() && peek() != '}') {
                    MathNode atom = parseAtom();
                    row.children.push_back(std::move(atom));
                }
                return row;
            }

            MathNode parseAtom() {
                const char c = peek();
                if (c == '{') {
                    ++i;
                    MathNode inner = parseRow(true);
                    if (peek() == '}') ++i;
                    MathNode g{.kind = MathKind::Group};
                    g.children.push_back(std::move(inner));
                    return g;
                }
                if (c == '\\') {
                    const std::string cmd = readCommand();
                    if (cmd == "frac") {
                        MathNode num = parseArg();
                        MathNode den = parseArg();
                        MathNode f{.kind = MathKind::Frac};
                        f.children.push_back(std::move(num));
                        f.children.push_back(std::move(den));
                        return f;
                    }

                    static const std::set<std::string> accentCmds = {
                        "hat", "bar", "vec", "tilde", "check", "acute", "grave",
                        "breve", "dot", "ddot", "widehat", "widetilde", "mathring",
                        "overline", "underline",
                    };
                    if (accentCmds.count(cmd)) {
                        MathNode a{.kind = MathKind::Accent};
                        a.command = cmd;
                        a.children.push_back(parseArg());
                        return a;
                    }
                    if (cmd == "matrix") {
                        return parseMatrixBody();
                    }

                    if (cmd == "left") {
                        const std::size_t afterLeft = i;
                        const char L = readDelim();
                        skipSpaces();
                        if (peek() == '\\') {
                            const std::size_t save = i;
                            if (readCommand() == "matrix") {
                                MathNode m = parseMatrixBody();
                                skipSpaces();
                                char R = 0;
                                if (peek() == '\\') {
                                    const std::size_t save2 = i;
                                    if (readCommand() == "right") R = readDelim();
                                    else i = save2;
                                }
                                m.matrixOpen = L ? L : 0;
                                m.matrixClose = R ? R : 0;
                                if (!m.matrixOpen && !m.matrixClose) m.matrixOpen = '(';
                                return m;
                            }
                            i = save;
                        }
                        i = afterLeft;
                        return MathNode::makeSymbol("left");
                    }
                    if (cmd == "sqrt") {
                        skipSpaces();
                        if (peek() == '[') {
                            while (i < s.size() && s[i] != ']') ++i;
                            if (peek() == ']') ++i;
                        }
                        MathNode rad = parseArg();
                        MathNode sq{.kind = MathKind::Sqrt};
                        sq.children.push_back(std::move(rad));
                        return sq;
                    }
                    return MathNode::makeSymbol(cmd);
                }

                ++i;
                return MathNode::makeChar(c);
            }

            MathNode attachScript(MathNode base, bool sup) {
                ++i;
                MathNode arg = parseArg();

                if (base.kind == MathKind::Script) {

                    if (sup && !base.has_sup) {
                        base.children.insert(base.children.begin() + 1, std::move(arg));
                        base.has_sup = true;
                    } else if (!sup && !base.has_sub) {
                        base.children.push_back(std::move(arg));
                        base.has_sub = true;
                    }
                    return base;
                }

                MathNode script{.kind = MathKind::Script};
                MathNode baseRow = MathNode::makeRow();
                baseRow.children.push_back(std::move(base));
                script.children.push_back(std::move(baseRow));
                script.children.push_back(std::move(arg));
                if (sup) script.has_sup = true; else script.has_sub = true;
                return script;
            }

            MathNode parseRow(bool stopAtBrace, bool matrixCell = false) {
                MathNode row = MathNode::makeRow();
                while (i < s.size()) {
                    const char c = s[i];
                    if (c == '}') {
                        if (stopAtBrace) break;
                        ++i;
                        continue;
                    }

                    if (matrixCell) {
                        if (c == '&') break;
                        if (c == '\\') {
                            const std::size_t save = i;
                            const std::string nm = readCommand();
                            i = save;
                            if (nm == "cr") break;
                        }
                    }
                    if (c == ' ') { ++i; continue; }
                    if (c == '^' || c == '_') {

                        MathNode base = row.children.empty()
                            ? MathNode::makeRow()
                            : std::move(row.children.back());
                        if (!row.children.empty()) row.children.pop_back();
                        if (base.kind == MathKind::Row && base.children.empty()) {

                            MathNode script{.kind = MathKind::Script};
                            script.children.push_back(MathNode::makeRow());
                            ++i;
                            MathNode arg = parseArg();
                            script.children.push_back(std::move(arg));
                            if (c == '^') script.has_sup = true; else script.has_sub = true;
                            row.children.push_back(std::move(script));
                        } else {
                            row.children.push_back(attachScript(std::move(base), c == '^'));
                        }
                        continue;
                    }
                    row.children.push_back(parseAtom());
                }
                return row;
            }
        };
    }

    MathNode parseTex(const std::string& tex) {
        Parser p(tex);
        return p.parseRow(false);
    }

}
