// End-to-end проверка: правки через иерархическую модель (AST) → TeX → mfl.
//
// Убеждаемся, что TeX, порождаемый AST после набора структур и удалений,
// корректно верстается движком mfl (layout.error пуст, глифы получены).
// Тест headless (без Qt/GUI), но с настоящими шрифтами STIX2 через FreeType.
//
// Запускать из каталога formula_widget/ (там лежит папка fonts/).

#include "math_ast.hpp"
#include "ast_cursor.hpp"
#include "ft_library.hpp"
#include "ft_font_face.hpp"
#include "mfl/layout.hpp"

#include <cstdio>
#include <memory>
#include <string>

using namespace formula;

static int g_fail = 0;

static bool layoutOk(fw::FtLibrary& lib, const std::string& tex, std::size_t& glyphs, std::string& err) {
    auto creator = [&lib](const mfl::font_family fam) -> std::unique_ptr<mfl::abstract_font_face> {
        auto f = std::make_unique<fw::FtFontFace>(fam, lib);
        f->set_size(mfl::points{20.0});
        return f;
    };
    const auto layout = mfl::layout(tex, mfl::points{20.0}, creator);
    glyphs = layout.glyphs.size();
    if (layout.error) { err = *layout.error; return false; }
    return true;
}

static void expectOk(fw::FtLibrary& lib, const std::string& scenario, const std::string& tex) {
    std::size_t glyphs = 0;
    std::string err;
    const bool ok = layoutOk(lib, tex, glyphs, err);
    std::printf("  %-28s tex=\"%s\"  glyphs=%zu  %s\n",
                scenario.c_str(), tex.c_str(), glyphs,
                ok ? "OK" : ("FAIL: " + err).c_str());
    if (!ok || glyphs == 0) ++g_fail;
}

int main() {
    fw::FtLibrary lib;
    std::printf("AST -> TeX -> mfl integration\n");

    // 1) Ввод верхнего индекса: x, ^, 2
    {
        AstCursor c;
        c.insertChar('x'); c.insertScript(true); c.insertChar('2');
        expectOk(lib, "type x^2", c.toTex());
    }
    // 2) Ввод дроби: /, 1, Tab, 2
    {
        AstCursor c;
        c.insertFraction(); c.insertChar('1'); c.tabNextField(); c.insertChar('2');
        expectOk(lib, "type 1/2 fraction", c.toTex());
    }
    // 3) Корень: Ctrl+R, x
    {
        AstCursor c;
        c.insertSqrt(); c.insertChar('x');
        expectOk(lib, "type sqrt x", c.toTex());
    }
    // 4) Дробь с индексами внутри: (a^2)/(b_i)
    {
        AstCursor c;
        c.insertFraction();
        c.insertChar('a'); c.insertScript(true); c.insertChar('2');
        c.moveToStart();               // выйти на верхний уровень
        // перейти в знаменатель: сдвигаемся к дроби и вниз
        c.move(Dir::Right);            // в числитель
        c.move(Dir::Down);             // в знаменатель
        c.insertChar('b'); c.insertScript(false); c.insertChar('i');
        expectOk(lib, "fraction a^2 / b_i", c.toTex());
    }
    // 5) Разбор и верстка исходной сложной формулы
    {
        AstCursor c;
        c.setFromTex("\\frac{1}{2\\pi i}\\int_\\gamma \\frac{f(z)}{z-a}\\,dz");
        expectOk(lib, "roundtrip Cauchy", c.toTex());
    }
    // 6) Серия удалений сохраняет валидность
    {
        AstCursor c;
        c.setFromTex("\\frac{ab}{cd}");
        for (int k = 0; k < 6; ++k) c.deleteBack();
        expectOk(lib, "after deletions", c.toTex().empty() ? "x" : c.toTex());
    }

    // 7) Массовая проверка: множество формул проходит AST -> TeX -> верстку mfl
    //    без ошибок и с ненулевым числом глифов.
    const char* bulk[] = {
        "a", "abc", "a+b", "a-b*c", "1+2=3",
        "x^2", "x_i", "x^2_3", "x^{a+b}", "x_{i+1}",
        "\\frac{1}{2}", "\\frac{a+b}{c-d}", "\\frac{1}{2\\pi i}",
        "\\frac{\\frac{a}{b}}{c}", "\\frac{a}{\\frac{b}{c}}",
        "\\sqrt{x}", "\\sqrt{a+b}", "\\sqrt{\\frac{a}{b}}", "\\sqrt{x^2+y^2}",
        "\\sin(x)", "\\cos(2x)", "\\ln(x+1)", "\\exp(x)", "\\lim", "\\sum", "\\int",
        "\\alpha+\\beta", "\\pi r^2", "2\\pi i", "\\alpha^\\beta_\\gamma",
        "\\int_a^b", "\\int_\\gamma", "\\sum_{i=1}^{n}",
        "a\\,b", "(a+b)^2", "a_{i,j}",
        "\\frac{-b\\pm\\sqrt{b^2-4ac}}{2a}",
        "\\frac{1}{2\\pi i}\\int_\\gamma\\frac{f(z)}{z-a}\\,dz",
        "x^{\\frac{a}{b}}", "e^{i\\pi}+1", "\\nabla\\cdot F", "\\frac{\\sin(x)}{\\cos(x)}",
        "\\int^{}_{}", "\\sum^{}_{}", "|x|", "(x+y)", "[a,b]",
        "\\int^{1}_{0}x", "\\sum^{n}_{i=1}i",
        // Новые структуры
        "\\hat{x}", "\\vec{v}", "\\bar{y}", "\\tilde{a}", "\\widehat{xy}",
        "\\overline{a+b}", "\\underline{x}", "\\hat{x}+\\vec{y}",
        "\\matrix{a & b \\cr c & d}", "\\matrix{1 & 2 & 3}",
        "\\frac{\\hat{x}}{\\overline{y}}", "A=\\matrix{\\alpha & \\beta \\cr \\gamma & \\delta}",
        "\\iint", "\\iiint", "\\oint", "\\left(\\frac{a}{b}\\right)",
        "\\left(\\matrix{a & b \\cr c & d}\\right)", "\\left[\\matrix{x \\cr y}\\right]",
        "\\left\\{\\matrix{x+y=1 \\cr x-y=0}\\right.",
    };
    for (const char* f : bulk) {
        AstCursor c;
        c.setFromTex(f);
        expectOk(lib, f, c.toTex());
    }

    std::printf("\n%s (%d failures)\n", g_fail == 0 ? "ALL OK" : "FAILURES", g_fail);
    return g_fail == 0 ? 0 : 1;
}
