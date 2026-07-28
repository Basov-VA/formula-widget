#include "math_semantics.hpp"

#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace formula {

    namespace {

        struct Tok {
            std::string str;
            bool startsVal = false;
            bool endsVal = false;
        };

        struct Ctx {
            std::set<std::string> vars;
            std::string pickMainVar() const {
                if (vars.count("x")) return "x";
                if (vars.count("t")) return "t";
                if (vars.count("y")) return "y";
                if (!vars.empty()) return *vars.begin();
                return "x";
            }
        };

        const std::map<std::string, std::string>& funcMap() {
            static const std::map<std::string, std::string> m = {
                {"sin", "sin"}, {"cos", "cos"}, {"tan", "tan"}, {"cot", "cot"},
                {"sec", "sec"}, {"csc", "csc"}, {"sinh", "sinh"}, {"cosh", "cosh"},
                {"tanh", "tanh"}, {"coth", "coth"}, {"exp", "exp"},
                {"arcsin", "asin"}, {"arccos", "acos"}, {"arctan", "atan"},
                {"ln", "log"}, {"log", "log"}, {"lg", "log"},
                {"max", "Max"}, {"min", "Min"}, {"gcd", "gcd"},
            };
            return m;
        }

        const std::map<std::string, std::string>& symbolMap() {
            static const std::map<std::string, std::string> m = {
                {"pi", "pi"}, {"infty", "oo"}, {"partial", "Symbol('partial')"},
                {"alpha", "alpha"}, {"beta", "beta"}, {"gamma", "gamma"}, {"delta", "delta"},
                {"epsilon", "epsilon"}, {"varepsilon", "epsilon"}, {"zeta", "zeta"},
                {"eta", "eta"}, {"theta", "theta"}, {"iota", "iota"}, {"kappa", "kappa"},
                {"lambda", "lamda"}, {"mu", "mu"}, {"nu", "nu"}, {"xi", "xi"},
                {"rho", "rho"}, {"sigma", "sigma"}, {"tau", "tau"}, {"phi", "phi"},
                {"varphi", "phi"}, {"chi", "chi"}, {"psi", "psi"}, {"omega", "omega"},
                {"Gamma", "Gamma"}, {"Delta", "Delta"}, {"Theta", "Theta"},
                {"Lambda", "Lamda"}, {"Sigma", "Sigma"}, {"Phi", "Phi"}, {"Omega", "Omega"},
            };
            return m;
        }

        const std::map<std::string, std::string>& operatorMap() {
            static const std::map<std::string, std::string> m = {
                {"cdot", "*"}, {"times", "*"}, {"div", "/"}, {"ast", "*"},
            };
            return m;
        }

        std::string translateSeq(const std::vector<const MathNode*>& atoms, Ctx& ctx);

        std::string bigOpName(const MathNode& a) {
            if (a.kind == MathKind::Symbol) return a.command;
            if (a.kind == MathKind::Script && !a.children.empty()) {
                const MathNode& base = a.children[0];
                if (base.kind == MathKind::Row && base.children.size() == 1 &&
                    base.children[0].kind == MathKind::Symbol)
                    return base.children[0].command;
            }
            return "";
        }
        bool isIntegral(const std::string& n) {
            return n == "int" || n == "iint" || n == "iiint" || n == "oint";
        }

        std::string plainName(const MathNode& row) {
            std::string s;
            for (const auto& c : row.children) {
                if (c.kind == MathKind::Char && std::isalnum(static_cast<unsigned char>(c.ch)))
                    s += c.ch;
            }
            return s;
        }

        Tok translateAtom(const MathNode& a, Ctx& ctx);

        Tok translateScript(const MathNode& a, Ctx& ctx) {
            std::string base = translateSeq({&a.children[0]}, ctx);

            const std::size_t subI = scriptSubIndex(a);
            if (subI != SIZE_MAX) {
                const std::string sub = plainName(a.children[subI]);
                if (!sub.empty()) { base = "Symbol('" + base + "_" + sub + "')"; }
            }
            const std::size_t supI = scriptSupIndex(a);
            if (supI != SIZE_MAX) {
                const std::string sup = translateSeq({&a.children[supI]}, ctx);
                return {"(" + base + ")**(" + sup + ")", true, true};
            }
            return {base, true, true};
        }

        Tok translateAtom(const MathNode& a, Ctx& ctx) {
            switch (a.kind) {
                case MathKind::Char: {
                    const char c = a.ch;
                    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.')
                        return {std::string(1, c), true, true};
                    if (std::isalpha(static_cast<unsigned char>(c))) {
                        ctx.vars.insert(std::string(1, c));
                        return {std::string(1, c), true, true};
                    }
                    switch (c) {
                        case '+': case '-': return {std::string(1, c), false, false};
                        case '*': case '/': return {std::string(1, c), false, false};
                        case '(': case '[': return {"(", true, false};
                        case ')': case ']': return {")", false, true};
                        case ',': return {",", false, false};
                        default:  return {std::string(1, c), false, false};
                    }
                }
                case MathKind::Symbol: {
                    const std::string& cmd = a.command;
                    if (auto it = operatorMap().find(cmd); it != operatorMap().end())
                        return {it->second, false, false};
                    if (auto it = symbolMap().find(cmd); it != symbolMap().end())
                        return {it->second, true, true};
                    if (auto it = funcMap().find(cmd); it != funcMap().end())
                        return {it->second, true, false};

                    static const std::set<std::string> spaces =
                        {",", "!", ";", ":", " ", "quad", "qquad", "thinspace", "medspace", "thickspace"};
                    if (spaces.count(cmd)) return {"", false, false};

                    ctx.vars.insert(cmd);
                    return {cmd, true, true};
                }
                case MathKind::Group:
                    return {"(" + translateSeq({&a.children[0]}, ctx) + ")", true, true};
                case MathKind::Frac:
                    return {"((" + translateSeq({&a.children[0]}, ctx) + ")/(" +
                                   translateSeq({&a.children[1]}, ctx) + "))", true, true};
                case MathKind::Sqrt:
                    return {"sqrt(" + translateSeq({&a.children[0]}, ctx) + ")", true, true};
                case MathKind::Accent:

                    return {translateSeq({&a.children[0]}, ctx), true, true};
                case MathKind::Matrix: {
                    const std::size_t cols = a.matrixCols ? a.matrixCols : 1;
                    std::string s = "Matrix([[";
                    for (std::size_t i = 0; i < a.children.size(); ++i) {
                        if (i > 0) s += (i % cols == 0) ? "], [" : ", ";
                        s += translateSeq({&a.children[i]}, ctx);
                    }
                    s += "]])";
                    return {s, true, true};
                }
                case MathKind::Script:
                    return translateScript(a, ctx);
                case MathKind::Row:
                    return {translateSeq({&a}, ctx), true, true};
            }
            return {"", false, false};
        }

        std::string translateSeq(const std::vector<const MathNode*>& rows, Ctx& ctx) {

            std::vector<const MathNode*> ch;
            if (rows.size() == 1 && rows[0]->kind == MathKind::Row) {
                for (const auto& c : rows[0]->children) ch.push_back(&c);
            } else {
                ch = rows;
            }

            std::vector<Tok> toks;
            for (std::size_t i = 0; i < ch.size();) {
                const MathNode& a = *ch[i];
                const std::string bop = bigOpName(a);
                const bool isScript = (a.kind == MathKind::Script);

                if (isIntegral(bop)) {
                    const std::size_t supI = isScript ? scriptSupIndex(a) : SIZE_MAX;
                    const std::size_t subI = isScript ? scriptSubIndex(a) : SIZE_MAX;
                    const std::string upper = supI != SIZE_MAX ? translateSeq({&a.children[supI]}, ctx) : "";
                    const std::string lower = subI != SIZE_MAX ? translateSeq({&a.children[subI]}, ctx) : "";

                    std::vector<const MathNode*> integ;
                    std::string var;
                    std::size_t j = i + 1;
                    for (; j < ch.size(); ++j) {
                        if (ch[j]->kind == MathKind::Char && ch[j]->ch == 'd' &&
                            j + 1 < ch.size() && ch[j + 1]->kind == MathKind::Char &&
                            std::isalpha(static_cast<unsigned char>(ch[j + 1]->ch))) {
                            var = std::string(1, ch[j + 1]->ch);
                            j += 2;
                            break;
                        }
                        integ.push_back(ch[j]);
                    }
                    std::string integrand = translateSeq(integ, ctx);
                    if (integrand.empty()) integrand = "1";
                    if (var.empty()) var = ctx.pickMainVar();
                    ctx.vars.insert(var);
                    std::string s = (!lower.empty() && !upper.empty())
                        ? "Integral(" + integrand + ", (" + var + ", " + lower + ", " + upper + "))"
                        : "Integral(" + integrand + ", " + var + ")";
                    toks.push_back({s, true, true});
                    i = j;
                    continue;
                }

                if (bop == "sum" || bop == "prod") {
                    const bool isProd = (bop == "prod");
                    const std::size_t supI = isScript ? scriptSupIndex(a) : SIZE_MAX;
                    const std::size_t subI = isScript ? scriptSubIndex(a) : SIZE_MAX;
                    std::string upper = supI != SIZE_MAX ? translateSeq({&a.children[supI]}, ctx) : "";

                    std::string idx = "i", start = "1";
                    if (subI != SIZE_MAX) {
                        const std::string lo = translateSeq({&a.children[subI]}, ctx);
                        const auto eq = lo.find('=');
                        if (eq != std::string::npos) { idx = lo.substr(0, eq); start = lo.substr(eq + 1); }
                        else start = lo;
                    }

                    std::vector<const MathNode*> body;
                    for (std::size_t k = i + 1; k < ch.size(); ++k) body.push_back(ch[k]);
                    std::string term = translateSeq(body, ctx);
                    if (term.empty()) term = "1";
                    if (upper.empty()) upper = "n";
                    const char* fn = isProd ? "Product" : "Sum";
                    toks.push_back({std::string(fn) + "(" + term + ", (" + idx + ", " + start + ", " + upper + "))",
                                    true, true});
                    i = ch.size();
                    continue;
                }

                toks.push_back(translateAtom(a, ctx));
                ++i;
            }

            std::string out;
            bool prevEndsVal = false;
            for (const auto& t : toks) {
                if (t.str.empty()) continue;
                if (!out.empty() && prevEndsVal && t.startsVal) out += "*";
                out += t.str;
                prevEndsVal = t.endsVal;
            }
            return out;
        }

    }

    namespace {

        bool isLoneIdent(const std::string& s) {
            if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0]))) return false;
            for (char c : s)
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
            return true;
        }
    }

    SemanticResult toSympy(const MathNode& root) {
        SemanticResult r;

        MathNode lhsRow = MathNode::makeRow(), rhsRow = MathNode::makeRow();
        bool hasEq = false;
        for (const auto& c : root.children) {
            if (!hasEq && c.kind == MathKind::Char && c.ch == '=') { hasEq = true; continue; }
            (hasEq ? rhsRow : lhsRow).children.push_back(c);
        }

        Ctx ctxL, ctxR;
        const std::string L = translateSeq({&lhsRow}, ctxL);
        const std::string R = hasEq ? translateSeq({&rhsRow}, ctxR) : std::string();

        std::string s;
        Ctx* ctx = &ctxL;
        if (!hasEq) { s = L; ctx = &ctxL; }
        else if (isLoneIdent(L) && !R.empty()) { s = R; ctx = &ctxR; }
        else if (isLoneIdent(R)) { s = L; ctx = &ctxL; }
        else { s = L; ctx = &ctxL; }

        if (s.empty()) { r.error = "пустое выражение"; return r; }
        r.ok = true;
        r.sympy = s;
        r.mainVar = ctx->pickMainVar();
        return r;
    }

}
