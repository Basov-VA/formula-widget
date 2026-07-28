// Автономные тесты иерархической модели курсора (AST).
//
// Не зависят от Qt/mfl — компилируются обычным clang++/g++:
//   clang++ -std=c++20 -I../src test_ast_cursor_standalone.cpp \
//       ../src/math_ast.cpp ../src/ast_cursor.cpp -o test_ast && ./test_ast
//
// Проверяют разбор/сериализацию (round-trip), навигацию по иерархии,
// вставку структур и структурно-корректное удаление.

#include "math_ast.hpp"
#include "ast_cursor.hpp"

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <functional>

using namespace formula;

static int g_failures = 0;
static int g_checks = 0;

static void check(bool cond, const char* expr, const char* file, int line) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("  FAIL [%s:%d]: %s\n", file, line, expr);
    }
}
#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

static void checkEq(const std::string& got, const std::string& want,
                    const char* file, int line) {
    ++g_checks;
    if (got != want) {
        ++g_failures;
        std::printf("  FAIL [%s:%d]: got \"%s\", want \"%s\"\n",
                    file, line, got.c_str(), want.c_str());
    }
}
#define CHECK_EQ(got, want) checkEq((got), (want), __FILE__, __LINE__)

// -------------------------------------------------------------------------- //

static void test_roundtrip() {
    std::printf("test_roundtrip\n");
    const char* formulas[] = {
        "a+b",
        "a+b*c",
        "\\frac{1}{2}",
        "\\frac{a+b}{c-d}",
        "\\sqrt{x}",
        "x^{2}",
        "x_{i}",
        "x^{2}_{i}",
        "\\frac{1}{2\\pi i}\\int_{\\gamma}\\frac{f(z)}{z-a}\\,dz",
    };
    for (const char* f : formulas) {
        MathNode ast = parseTex(f);
        const std::string out = toTex(ast);
        // повторный разбор должен дать ту же строку (устойчивость)
        const std::string out2 = toTex(parseTex(out));
        CHECK_EQ(out2, out);
    }
    // конкретные ожидания
    CHECK_EQ(toTex(parseTex("a+b")), "a+b");
    CHECK_EQ(toTex(parseTex("\\frac{1}{2}")), "\\frac{1}{2}");
    CHECK_EQ(toTex(parseTex("x^2")), "x^{2}");
    CHECK_EQ(toTex(parseTex("2\\pi i")), "2\\pi i");   // пробел-разделитель
    CHECK_EQ(toTex(parseTex("\\int_\\gamma")), "\\int_{\\gamma}");
}

static void test_parse_structure() {
    std::printf("test_parse_structure\n");
    MathNode f = parseTex("\\frac{a}{b}");
    CHECK(f.kind == MathKind::Row);
    CHECK(f.children.size() == 1);
    CHECK(f.children[0].kind == MathKind::Frac);
    CHECK(f.children[0].children.size() == 2);
    CHECK(f.children[0].children[0].children.size() == 1);  // числитель 'a'

    MathNode s = parseTex("x^2_3");
    CHECK(s.children.size() == 1);
    CHECK(s.children[0].kind == MathKind::Script);
    CHECK(s.children[0].has_sup);
    CHECK(s.children[0].has_sub);
}

static void test_linear_typing() {
    std::printf("test_linear_typing\n");
    AstCursor c;
    c.insertChar('a');
    c.insertChar('+');
    c.insertChar('b');
    CHECK_EQ(c.toTex(), "a+b");
    CHECK(c.glyphCount() == 3);
    // курсор в конце → после последнего глифа
    CHECK(c.cursorAfterGlyph());
}

static void test_navigation_linear() {
    std::printf("test_navigation_linear\n");
    AstCursor c;
    c.setFromTex("abc");
    c.moveToStart();
    CHECK(c.offset() == 0);
    CHECK(c.move(Dir::Right));
    CHECK(c.offset() == 1);
    CHECK(c.move(Dir::Right));
    CHECK(c.move(Dir::Right));
    CHECK(c.offset() == 3);
    CHECK(!c.move(Dir::Right));   // дальше некуда
    CHECK(c.move(Dir::Left));
    CHECK(c.offset() == 2);
}

static void test_enter_fraction() {
    std::printf("test_enter_fraction\n");
    AstCursor c;
    c.setFromTex("\\frac{a}{b}");
    c.moveToStart();          // перед дробью
    CHECK(c.depth() == 0);
    CHECK(c.move(Dir::Right)); // войти в числитель
    CHECK(c.depth() == 1);
    // курсор перед 'a' числителя → это глиф 0
    CHECK(c.glyphIndexAtCursor().value() == 0);
}

static void test_enter_empty_field() {
    std::printf("test_enter_empty_field\n");
    AstCursor c;                 // пустой корень
    CHECK(c.insertFraction());   // вставить \frac, курсор в пустом числителе
    CHECK(c.depth() == 1);
    CHECK(c.inEmptyRow());       // заход в ПУСТОЕ поле
    c.insertChar('a');           // ввод в числитель
    CHECK(c.tabNextField());     // Tab → знаменатель
    CHECK(c.inEmptyRow());
    c.insertChar('b');
    CHECK_EQ(c.toTex(), "\\frac{a}{b}");
}

static void test_insert_script() {
    std::printf("test_insert_script\n");
    AstCursor c;
    c.insertChar('x');
    CHECK(c.insertScript(true));   // ^, курсор в пустом верхнем индексе
    CHECK(c.inEmptyRow());
    c.insertChar('2');
    CHECK_EQ(c.toTex(), "x^{2}");
    // добавить нижний индекс к тому же x
    c.move(Dir::Right);            // выйти из sup (после структуры)
    // курсор после Script; чтобы добавить sub, встанем сразу после базы:
    // проще заново: соберём x^2_3 через ввод
    AstCursor c2;
    c2.insertChar('x');
    c2.insertScript(true);
    c2.insertChar('2');
    c2.move(Dir::Right);           // выйти из скрипта
    c2.insertScript(false);        // _ на том же x (перед курсором Script)
    c2.insertChar('3');
    CHECK_EQ(c2.toTex(), "x^{2}_{3}");
}

static void test_vertical_nav_fraction() {
    std::printf("test_vertical_nav_fraction\n");
    AstCursor c;
    c.setFromTex("\\frac{a}{b}");
    c.moveToStart();
    c.move(Dir::Right);            // в числитель
    CHECK(c.depth() == 1);
    CHECK(c.move(Dir::Down));      // числитель → знаменатель
    CHECK(c.depth() == 1);
    CHECK(c.inEmptyRow() == false);
    CHECK(c.move(Dir::Up));        // знаменатель → числитель
    CHECK(c.depth() == 1);
}

static void test_backspace_leaf() {
    std::printf("test_backspace_leaf\n");
    AstCursor c;
    c.setFromTex("abc");
    CHECK(c.deleteBack());
    CHECK_EQ(c.toTex(), "ab");
    CHECK(c.deleteBack());
    CHECK(c.deleteBack());
    CHECK_EQ(c.toTex(), "");
    CHECK(!c.deleteBack());       // пусто — no-op
}

static void test_backspace_empty_structure() {
    std::printf("test_backspace_empty_structure\n");
    AstCursor c;
    c.insertFraction();           // \frac{}{} курсор в числителе
    // курсор в начале пустого числителя; Backspace должен удалить всю дробь
    CHECK(c.deleteBack());
    CHECK_EQ(c.toTex(), "");
    CHECK(c.depth() == 0);
}

static void test_delete_keeps_valid_tex() {
    std::printf("test_delete_keeps_valid_tex\n");
    // Любая последовательность удалений не должна ломать баланс скобок:
    // сериализация из AST всегда сбалансирована по построению.
    AstCursor c;
    c.setFromTex("\\frac{ab}{cd}");
    for (int i = 0; i < 20; ++i) {
        c.deleteBack();
        const std::string tex = c.toTex();
        // проверка баланса фигурных скобок
        int bal = 0;
        for (char ch : tex) { if (ch == '{') ++bal; else if (ch == '}') --bal; }
        CHECK(bal == 0);
    }
}

static void test_glyph_index_mapping() {
    std::printf("test_glyph_index_mapping\n");
    AstCursor c;
    c.setFromTex("abc");
    c.moveToStart();
    CHECK(c.glyphIndexAtCursor().value() == 0);
    c.move(Dir::Right);
    CHECK(c.glyphIndexAtCursor().value() == 1);
    c.moveToEnd();
    CHECK(!c.glyphIndexAtCursor().has_value());  // после всех глифов
    CHECK(c.cursorAfterGlyph());

    // в дроби \frac{a}{b}: числитель 'a' = глиф 0, знаменатель 'b' = глиф 1
    AstCursor f;
    f.setFromTex("\\frac{a}{b}");
    f.moveToStart();
    f.move(Dir::Right);          // в числитель, перед 'a'
    CHECK(f.glyphIndexAtCursor().value() == 0);
    f.tabNextField();            // знаменатель, перед 'b'
    CHECK(f.glyphIndexAtCursor().value() == 1);
}

// Компактное описание опоры каретки в текущей позиции — для проверки навигации.
static std::string anchorDesc(const AstCursor& c) {
    if (c.inEmptyRow()) return "emptyField";
    auto a = c.caretAnchor();
    if (!a) return "none";
    char buf[48];
    if (a->isStruct)
        std::snprintf(buf, sizeof buf, "struct,after=%d", (int)a->after);
    else
        std::snprintf(buf, sizeof buf, "g%zu,after=%d", a->glyphIndex, (int)a->after);
    return buf;
}

// Прогнать последовательность направлений и вернуть след опор каретки.
static std::vector<std::string> walk(AstCursor& c, const std::vector<Dir>& dirs) {
    std::vector<std::string> trace;
    trace.push_back(anchorDesc(c));
    for (Dir d : dirs) { c.move(d); trace.push_back(anchorDesc(c)); }
    return trace;
}

static void checkTrace(const std::vector<std::string>& got,
                       const std::vector<std::string>& want,
                       const char* name, const char* file, int line) {
    ++g_checks;
    bool ok = got.size() == want.size();
    for (std::size_t i = 0; ok && i < got.size(); ++i) ok = (got[i] == want[i]);
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL [%s:%d]: %s\n    got : ", file, line, name);
        for (auto& s : got) std::printf("[%s] ", s.c_str());
        std::printf("\n    want: ");
        for (auto& s : want) std::printf("[%s] ", s.c_str());
        std::printf("\n");
    }
}
#define CHECK_TRACE(got, want, name) checkTrace((got), (want), (name), __FILE__, __LINE__)

// Сценарий: навигация вокруг интеграла со скриптом a ∫_γ b
// Глифы (порядок чтения): a=0, ∫=1, γ=2, b=3.
// Раньше «после γ» каретка ошибочно прыгала на b (вправо на основную строку),
// а половина нажатий не двигала каретку. Проверяем корректную опору на каждом шаге.
static void test_nav_integral() {
    std::printf("test_nav_integral\n");
    AstCursor c;
    c.setFromTex("a\\int_\\gamma b");
    c.moveToStart();
    auto trace = walk(c, { Dir::Right, Dir::Right, Dir::Right, Dir::Right,
                           Dir::Right, Dir::Right, Dir::Right });
    checkTrace(trace, std::vector<std::string>{
        "g0,after=0",      // перед a
        "g0,after=1",      // после a
        "g1,after=0",      // перед ∫ (в базе скрипта)
        "g1,after=1",      // после ∫
        "g2,after=0",      // перед γ (в нижнем индексе)
        "g2,after=1",      // ПОСЛЕ γ — привязка к γ, а НЕ к b (это и был баг)
        "g3,after=0",      // вышли вправо: ПЕРЕД b (у левого края b, отдельная позиция)
        "g3,after=1",      // после b
    }, "integral Right", __FILE__, __LINE__);

    // Обратный ход: из конца влево — зеркальная последовательность.
    c.moveToEnd();
    auto back = walk(c, { Dir::Left, Dir::Left, Dir::Left, Dir::Left,
                          Dir::Left, Dir::Left, Dir::Left });
    checkTrace(back, std::vector<std::string>{
        "g3,after=1",      // после b (конец)
        "g3,after=0",      // перед b (отдельная позиция, не совпадает с «после γ»)
        "g2,after=1",      // после γ
        "g2,after=0",      // перед γ
        "g1,after=1",      // после ∫
        "g1,after=0",      // перед ∫
        "g0,after=1",      // после a
        "g0,after=0",      // перед a (начало)
    }, "integral Left", __FILE__, __LINE__);
}

// Сценарий: навигация вокруг дроби a \frac{b}{c} d
// Глифы: a=0, b=1(числитель), c=2(знаменатель), d=3.
static void test_nav_fraction_boundary() {
    std::printf("test_nav_fraction_boundary\n");
    AstCursor c;
    c.setFromTex("a\\frac{b}{c}d");
    c.moveToStart();
    auto trace = walk(c, { Dir::Right, Dir::Right, Dir::Right, Dir::Right,
                           Dir::Right, Dir::Right, Dir::Right });
    checkTrace(trace, std::vector<std::string>{
        "g0,after=0",      // перед a
        "g0,after=1",      // после a
        "g1,after=0",      // перед b (в числителе)
        "g1,after=1",      // после b (числитель)
        "g2,after=0",      // перед c (в знаменателе)
        "g2,after=1",      // после c (знаменатель)
        "g3,after=0",      // вышли вправо: перед d (отдельная позиция)
        "g3,after=1",      // после d
    }, "fraction Right", __FILE__, __LINE__);
}

// Сценарий: многоглифная функция \sin(x). glyphSpan(\sin)=3 (s,i,n).
// Раньше «после sin» каретка указывала на неверный глиф.
static void test_nav_function_multiglyph() {
    std::printf("test_nav_function_multiglyph\n");
    AstCursor c;
    c.setFromTex("\\sin(x)");
    CHECK(c.glyphCount() == 6);   // s,i,n,(,x,)
    c.moveToStart();
    auto trace = walk(c, { Dir::Right, Dir::Right, Dir::Right, Dir::Right, Dir::Right });
    checkTrace(trace, std::vector<std::string>{
        "g0,after=0",   // перед s
        "g2,after=1",   // ПОСЛЕ sin = правый край 'n' (последний глиф функции)
        "g3,after=1",   // после '('
        "g4,after=1",   // после x
        "g5,after=1",   // после ')'
        "g5,after=1",   // дальше некуда (нет хода) — трасса повторяет последнюю
    }, "sin(x) Right", __FILE__, __LINE__);
}

// Сценарий: тонкий пробел \, (glyphSpan=0). Раньше каретка исчезала.
static void test_nav_thinspace() {
    std::printf("test_nav_thinspace\n");
    AstCursor c;
    c.setFromTex("a\\,b");
    CHECK(c.glyphCount() == 2);   // a,b — \, глифов не даёт
    c.moveToStart();
    auto trace = walk(c, { Dir::Right, Dir::Right, Dir::Right });
    checkTrace(trace, std::vector<std::string>{
        "g0,after=0",   // перед a
        "g0,after=1",   // после a
        "g1,after=0",   // после \, → перед b (каретка НЕ исчезает)
        "g1,after=1",   // после b
    }, "a\\,b Right", __FILE__, __LINE__);
}

// Сценарий: вертикальная навигация в дроби числитель ↔ знаменатель.
static void test_nav_fraction_vertical() {
    std::printf("test_nav_fraction_vertical\n");
    AstCursor c;
    c.setFromTex("\\frac{a}{b}");
    c.moveToStart();
    c.move(Dir::Right);            // в числитель, перед a
    CHECK_EQ(anchorDesc(c), "g0,after=0");
    CHECK(c.move(Dir::Down));      // вниз → знаменатель
    CHECK_EQ(anchorDesc(c), "g1,after=0");   // перед b
    CHECK(c.move(Dir::Up));        // вверх → числитель
    CHECK_EQ(anchorDesc(c), "g0,after=0");
}

static void test_set_from_glyph() {
    std::printf("test_set_from_glyph\n");
    AstCursor c;
    c.setFromTex("\\frac{a}{b}c");  // глифы: a(0), b(1), c(2)
    c.setFromGlyph(0, false);       // перед 'a' (в числителе)
    CHECK(c.glyphIndexAtCursor().value() == 0);
    CHECK(c.depth() == 1);
    c.setFromGlyph(1, true);        // после 'b' (в знаменателе)
    CHECK(c.depth() == 1);
    c.insertChar('z');              // ввод в знаменатель после b → глифы: a,b,z,c
    CHECK_EQ(c.toTex(), "\\frac{a}{bz}c");
    c.setFromGlyph(3, false);       // перед 'c' (верхний уровень)
    CHECK(c.depth() == 0);
}

// ============================================================================ //
//  БОЛЬШОЙ НАБОР СЦЕНАРИЕВ (~100+): round-trip, инварианты навигации,
//  редактирование, вертикаль, клики. Каждый вызов — один сценарий (g_scenarios).
// ============================================================================ //

static int g_scenarios = 0;

// Проверить, что опора каретки в текущей позиции валидна (не «исчезает» и не
// выходит за пределы массива глифов). Это ловит класс багов с пропаданием каретки.
static bool anchorValid(const AstCursor& c, std::size_t total, std::string& why) {
    if (c.inEmptyRow()) return true;                 // пустое поле — отдельная отрисовка
    auto a = c.caretAnchor();
    // Поле может состоять только из «невидимых» атомов (\,) — глифов нет; тогда
    // виджет рисует каретку по bbox поля (emptyFieldCaretRect). Это допустимо.
    if (!a) return true;
    if (!a->isStruct) {
        if (a->glyphIndex >= total) { why = "glyphIndex вне диапазона"; return false; }
    } else {
        if (a->structCtx.glyphCount > 0 &&
            a->structCtx.glyphStart + a->structCtx.glyphCount > total) {
            why = "диапазон структуры вне границ";
            return false;
        }
    }
    return true;
}

// Универсальная проверка формулы: round-trip стабилен; при обходе Right от начала
// до конца каретка везде валидна и нет зацикливания; обратный обход Left
// возвращает курсор в начало.
static void checkFormula(const std::string& tex, const char* file, int line) {
    ++g_scenarios;
    // 1) round-trip: toTex(parse(x)) стабилен при повторном разборе
    const std::string t1 = toTex(parseTex(tex));
    const std::string t2 = toTex(parseTex(t1));
    if (t1 != t2) {
        ++g_checks; ++g_failures;
        std::printf("  FAIL [%s:%d] round-trip нестабилен: \"%s\" -> \"%s\" -> \"%s\"\n",
                    file, line, tex.c_str(), t1.c_str(), t2.c_str());
        return;
    }

    AstCursor c;
    c.setFromTex(tex);
    const std::size_t total = c.glyphCount();

    // 2) обход вправо: валидность опоры + защита от зацикливания
    c.moveToStart();
    std::string why;
    ++g_checks;
    bool ok = true;
    if (!anchorValid(c, total, why)) { ok = false; }
    std::size_t steps = 0;
    // Число позиций каретки растёт с размером формулы (глифы + границы полей),
    // поэтому потолок масштабируем — иначе большие формулы дают ложное «зацикливание».
    const std::size_t kMax = std::max<std::size_t>(2000, total * 8 + 2000);
    while (ok && c.move(Dir::Right)) {
        if (!anchorValid(c, total, why)) { ok = false; break; }
        if (++steps > kMax) { why = "зацикливание Right"; ok = false; break; }
    }
    // 3) в конце: верхний уровень, конец корня
    if (ok && (c.depth() != 0 || c.offset() != c.root().children.size())) {
        why = "Right не привёл в конец корня"; ok = false;
    }
    // 4) обход влево возвращает в начало
    std::size_t bsteps = 0;
    while (ok && c.move(Dir::Left)) {
        if (!anchorValid(c, total, why)) { ok = false; break; }
        if (++bsteps > kMax) { why = "зацикливание Left"; ok = false; break; }
    }
    if (ok && (c.depth() != 0 || c.offset() != 0)) {
        why = "Left не вернул в начало"; ok = false;
    }
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL [%s:%d] формула \"%s\": %s\n", file, line, tex.c_str(), why.c_str());
    }
}
#define CHECK_FORMULA(tex) checkFormula((tex), __FILE__, __LINE__)

static void checkGlyphCount(const std::string& tex, std::size_t expect, const char* file, int line) {
    ++g_scenarios; ++g_checks;
    AstCursor c; c.setFromTex(tex);
    if (c.glyphCount() != expect) {
        ++g_failures;
        std::printf("  FAIL [%s:%d] glyphCount(\"%s\")=%zu, ожидалось %zu\n",
                    file, line, tex.c_str(), c.glyphCount(), expect);
    }
}
#define CHECK_GLYPHS(tex, n) checkGlyphCount((tex), (n), __FILE__, __LINE__)

static void checkEdit(const char* name, const std::function<void(AstCursor&)>& ops,
                      const std::string& expect, const char* file, int line) {
    ++g_scenarios; ++g_checks;
    AstCursor c;
    ops(c);
    if (c.toTex() != expect) {
        ++g_failures;
        std::printf("  FAIL [%s:%d] правка «%s»: got \"%s\", want \"%s\"\n",
                    file, line, name, c.toTex().c_str(), expect.c_str());
    }
}
#define CHECK_EDIT(name, ops, expect) checkEdit((name), (ops), (expect), __FILE__, __LINE__)

static bool anchorDescEq(const AstCursor& c, const char* s) { return anchorDesc(c) == std::string(s); }

// --- Группа: формулы (round-trip + инварианты навигации) --------------------
static void scenarios_formulas() {
    std::printf("scenarios_formulas\n");
    const char* fs[] = {
        "", "a", "ab", "abc", "a+b", "a-b*c", "1+2=3", "a+b-c*d/e",
        "x^2", "x_i", "x^2_3", "x^{a+b}", "x_{i+1}", "x^{2}_{i}", "a^b^c",
        "\\frac{1}{2}", "\\frac{a+b}{c-d}", "\\frac{1}{2\\pi i}",
        "\\frac{\\frac{a}{b}}{c}", "\\frac{a}{\\frac{b}{c}}", "\\frac{\\frac{a}{b}}{\\frac{c}{d}}",
        "\\sqrt{x}", "\\sqrt{a+b}", "\\sqrt{\\frac{a}{b}}", "\\sqrt{x^2+y^2}",
        "\\sin(x)", "\\cos(2x)", "\\ln(x+1)", "\\lim", "\\exp(x)",
        "\\alpha+\\beta", "\\pi r^2", "2\\pi i", "\\alpha^\\beta_\\gamma",
        "\\int", "\\int_a^b", "\\int_\\gamma", "\\sum_{i=1}^{n}", "\\prod_k",
        "a\\,b", "a\\,\\frac{b}{c}", "\\frac{f(z)}{z-a}\\,dz",
        "{ab}", "{a+b}c", "(a+b)^2", "a_{i,j}",
        "\\frac{-b\\pm\\sqrt{b^2-4ac}}{2a}",
        "\\frac{1}{2\\pi i}\\int_\\gamma\\frac{f(z)}{z-a}\\,dz",
        "x^{\\frac{a}{b}}", "\\frac{\\sin(x)}{\\cos(x)}", "e^{i\\pi}+1",
        "\\sqrt[3]{x}", "\\frac{d}{dx}f(x)", "\\nabla\\cdot F",
        "\\int^{}_{}", "\\sum^{}_{}", "|x|", "(x+y)", "[a,b]",
        "\\int^{1}_{0} x\\,dx", "\\sum^{n}_{i=1} i", "|x+y|^2",
    };
    for (const char* f : fs) CHECK_FORMULA(f);
}

// --- Группа: количество глифов ----------------------------------------------
static void scenarios_glyphcount() {
    std::printf("scenarios_glyphcount\n");
    CHECK_GLYPHS("", 0);
    CHECK_GLYPHS("a", 1);
    CHECK_GLYPHS("abc", 3);
    CHECK_GLYPHS("a+b", 3);
    CHECK_GLYPHS("\\frac{a}{b}", 2);
    CHECK_GLYPHS("\\frac{ab}{cd}", 4);
    CHECK_GLYPHS("x^2", 2);
    CHECK_GLYPHS("x^2_3", 3);
    CHECK_GLYPHS("\\sqrt{ab}", 2);
    CHECK_GLYPHS("\\pi", 1);
    CHECK_GLYPHS("2\\pi i", 3);
    CHECK_GLYPHS("\\int_\\gamma", 2);
    CHECK_GLYPHS("a\\,b", 2);            // \, — 0 глифов
    CHECK_GLYPHS("a\\,\\,b", 2);         // два пробела — тоже 0
    CHECK_GLYPHS("\\sin", 3);            // s,i,n
    CHECK_GLYPHS("\\sin(x)", 6);         // s,i,n,(,x,)
    CHECK_GLYPHS("\\lim", 3);
    CHECK_GLYPHS("\\ln", 2);
    CHECK_GLYPHS("\\frac{\\sin x}{2}", 5); // sin(3)+x + 2
    CHECK_GLYPHS("\\sum", 1);
}

// --- Группа: редактирование (ввод/удаление структур) ------------------------
static void scenarios_editing() {
    std::printf("scenarios_editing\n");
    CHECK_EDIT("type abc", [](AstCursor& c){ c.insertChar('a'); c.insertChar('b'); c.insertChar('c'); }, "abc");
    CHECK_EDIT("type a+b", [](AstCursor& c){ c.insertChar('a'); c.insertChar('+'); c.insertChar('b'); }, "a+b");
    CHECK_EDIT("frac 1/2", [](AstCursor& c){ c.insertFraction(); c.insertChar('1'); c.tabNextField(); c.insertChar('2'); }, "\\frac{1}{2}");
    CHECK_EDIT("x^2", [](AstCursor& c){ c.insertChar('x'); c.insertScript(true); c.insertChar('2'); }, "x^{2}");
    CHECK_EDIT("x_i", [](AstCursor& c){ c.insertChar('x'); c.insertScript(false); c.insertChar('i'); }, "x_{i}");
    CHECK_EDIT("sqrt x", [](AstCursor& c){ c.insertSqrt(); c.insertChar('x'); }, "\\sqrt{x}");
    CHECK_EDIT("group ab", [](AstCursor& c){ c.insertGroup(); c.insertChar('a'); c.insertChar('b'); }, "{ab}");
    CHECK_EDIT("symbol pi", [](AstCursor& c){ c.insertSymbol("pi"); }, "\\pi");
    CHECK_EDIT("2 pi i", [](AstCursor& c){ c.insertChar('2'); c.insertSymbol("pi"); c.insertChar('i'); }, "2\\pi i");
    CHECK_EDIT("sin(x) via parens", [](AstCursor& c){
        c.insertSymbol("sin"); c.insertChar('('); c.insertChar(')'); c.move(Dir::Left); c.insertChar('x'); }, "\\sin(x)");
    CHECK_EDIT("x^2_3", [](AstCursor& c){
        c.insertChar('x'); c.insertScript(true); c.insertChar('2'); c.move(Dir::Right);
        c.insertScript(false); c.insertChar('3'); }, "x^{2}_{3}");
    CHECK_EDIT("nested frac in num", [](AstCursor& c){
        c.insertFraction(); c.insertFraction(); c.insertChar('a'); c.tabNextField(); c.insertChar('b'); }, "\\frac{\\frac{a}{b}}{}");
    CHECK_EDIT("insert middle", [](AstCursor& c){ c.setFromTex("ac"); c.moveToStart(); c.move(Dir::Right); c.insertChar('b'); }, "abc");
    CHECK_EDIT("insert start", [](AstCursor& c){ c.setFromTex("bc"); c.moveToStart(); c.insertChar('a'); }, "abc");
    CHECK_EDIT("backspace leaf", [](AstCursor& c){ c.setFromTex("abc"); c.deleteBack(); }, "ab");
    CHECK_EDIT("backspace to empty", [](AstCursor& c){ c.insertChar('a'); c.deleteBack(); }, "");
    CHECK_EDIT("delete forward", [](AstCursor& c){ c.setFromTex("abc"); c.moveToStart(); c.deleteForward(); }, "bc");
    CHECK_EDIT("backspace empty frac", [](AstCursor& c){ c.insertFraction(); c.deleteBack(); }, "");
    CHECK_EDIT("frac then exit then char", [](AstCursor& c){
        c.insertFraction(); c.insertChar('1'); c.tabNextField(); c.insertChar('2');
        c.move(Dir::Right); c.insertChar('+'); }, "\\frac{1}{2}+");
    CHECK_EDIT("sqrt with frac inside", [](AstCursor& c){
        c.insertSqrt(); c.insertFraction(); c.insertChar('a'); c.tabNextField(); c.insertChar('b'); }, "\\sqrt{\\frac{a}{b}}");
    CHECK_EDIT("script on frac", [](AstCursor& c){
        c.insertFraction(); c.insertChar('a'); c.tabNextField(); c.insertChar('b');
        c.move(Dir::Right); c.insertScript(true); c.insertChar('2'); }, "{\\frac{a}{b}}^{2}");
    CHECK_EDIT("delete all forward", [](AstCursor& c){
        c.setFromTex("ab"); c.moveToStart(); c.deleteForward(); c.deleteForward(); }, "");
    CHECK_EDIT("type into denominator", [](AstCursor& c){
        c.setFromTex("\\frac{a}{b}"); c.moveToStart(); c.move(Dir::Right); c.move(Dir::Down); c.insertChar('c'); }, "\\frac{a}{cb}");
    CHECK_EDIT("insert symbol between", [](AstCursor& c){
        c.setFromTex("ab"); c.moveToStart(); c.move(Dir::Right); c.insertSymbol("cdot"); }, "a\\cdot b");
    CHECK_EDIT("backspace inside frac num", [](AstCursor& c){
        c.setFromTex("\\frac{ab}{c}"); c.moveToStart(); c.move(Dir::Right); c.move(Dir::Right); c.move(Dir::Right); c.deleteBack(); }, "\\frac{a}{c}");
    // Backspace/Delete удаляют соседнюю СТРУКТУРУ целиком:
    CHECK_EDIT("backspace deletes fraction", [](AstCursor& c){
        c.setFromTex("\\frac{a}{b}c"); c.moveToEnd(); c.move(Dir::Left); c.deleteBack(); }, "c");
    CHECK_EDIT("backspace deletes integral", [](AstCursor& c){
        c.setFromTex("a\\int_0^1"); c.moveToEnd(); c.deleteBack(); }, "a");
    CHECK_EDIT("backspace deletes sqrt", [](AstCursor& c){
        c.setFromTex("x\\sqrt{ab}"); c.moveToEnd(); c.deleteBack(); }, "x");
    CHECK_EDIT("delete forward removes fraction", [](AstCursor& c){
        c.setFromTex("c\\frac{a}{b}"); c.moveToStart(); c.move(Dir::Right); c.deleteForward(); }, "c");
    CHECK_EDIT("delete forward removes integral", [](AstCursor& c){
        c.setFromTex("\\int_0^1 x"); c.moveToStart(); c.deleteForward(); }, "x");
}

// --- Группа: вертикальная навигация -----------------------------------------
static void scenarios_vertical() {
    std::printf("scenarios_vertical\n");
    // дробь: числитель <-> знаменатель
    { AstCursor c; c.setFromTex("\\frac{a}{b}"); c.moveToStart(); c.move(Dir::Right);
      ++g_scenarios; ++g_checks; if(!(c.move(Dir::Down) && anchorDescEq(c,"g1,after=0"))){++g_failures; std::printf("  FAIL down->den\n");} }
    { AstCursor c; c.setFromTex("\\frac{a}{b}"); c.moveToStart(); c.move(Dir::Right); c.move(Dir::Down);
      ++g_scenarios; ++g_checks; if(!(c.move(Dir::Up) && anchorDescEq(c,"g0,after=0"))){++g_failures; std::printf("  FAIL up->num\n");} }
    // скрипт: база <-> верхний
    { AstCursor c; c.setFromTex("x^2"); c.moveToStart(); c.move(Dir::Right);   // в базе перед?
      ++g_scenarios; ++g_checks; bool up = c.move(Dir::Up); if(!up){++g_failures; std::printf("  FAIL x^2 up\n");} }
    // скрипт: база <-> нижний
    { AstCursor c; c.setFromTex("x_i"); c.moveToStart(); c.move(Dir::Right);
      ++g_scenarios; ++g_checks; bool dn = c.move(Dir::Down); if(!dn){++g_failures; std::printf("  FAIL x_i down\n");} }
    // нет вертикали в линейном тексте
    { AstCursor c; c.setFromTex("abc"); c.moveToStart();
      ++g_scenarios; ++g_checks; if(c.move(Dir::Up)||c.move(Dir::Down)){++g_failures; std::printf("  FAIL linear vertical\n");} }
    // вложенная дробь: заход и вертикаль
    { AstCursor c; c.setFromTex("\\frac{\\frac{a}{b}}{c}"); c.moveToStart(); c.move(Dir::Right); c.move(Dir::Right);
      ++g_scenarios; ++g_checks; bool dn = c.move(Dir::Down); if(!dn){++g_failures; std::printf("  FAIL nested down\n");} }
    // x^2_3: база -> верх -> база -> низ
    { AstCursor c; c.setFromTex("x^2_3"); c.moveToStart(); c.move(Dir::Right);
      ++g_scenarios; ++g_checks; bool ok = c.move(Dir::Up); ok = ok && c.move(Dir::Down); ok = ok && c.move(Dir::Down);
      if(!ok){++g_failures; std::printf("  FAIL x^2_3 vertical chain\n");} }
    // Tab по полям дроби
    { AstCursor c; c.insertFraction(); ++g_scenarios; ++g_checks;
      bool ok = c.inEmptyRow(); c.insertChar('a'); ok = ok && c.tabNextField() && c.inEmptyRow();
      if(!ok){++g_failures; std::printf("  FAIL tab fields\n");} }
}

// --- Группа: setFromGlyph (клик мышью) --------------------------------------
static void scenarios_setfromglyph() {
    std::printf("scenarios_setfromglyph\n");
    auto chk = [&](const char* tex, std::size_t gi, bool after, std::size_t wantDepth, const char* wantAnchor){
        ++g_scenarios; ++g_checks;
        AstCursor c; c.setFromTex(tex); c.setFromGlyph(gi, after);
        if (c.depth()!=wantDepth || anchorDesc(c)!=std::string(wantAnchor)) {
            ++g_failures;
            std::printf("  FAIL setFromGlyph(%s,%zu,%d): depth=%zu anchor=%s (want depth=%zu %s)\n",
                        tex, gi, (int)after, c.depth(), anchorDesc(c).c_str(), wantDepth, wantAnchor);
        }
    };
    chk("ab", 0, false, 0, "g0,after=0");
    chk("ab", 0, true, 0, "g0,after=1");
    chk("ab", 1, false, 0, "g0,after=1");   // перед b == после a (соседний лист слева)
    chk("\\frac{a}{b}c", 0, false, 1, "g0,after=0");  // в числитель перед a
    chk("\\frac{a}{b}c", 1, true, 1, "g1,after=1");   // в знаменатель после b
    chk("\\frac{a}{b}c", 2, false, 0, "g2,after=0");  // верхний уровень перед c
    chk("\\sin(x)", 0, false, 0, "g0,after=0");        // перед sin
    chk("\\sin(x)", 4, false, 0, "g3,after=1");        // перед x == после '(' (сосед-лист слева)
}

// --- Группа: render-строка (пустые поля → \, для верстки) --------------------
static void scenarios_rendertex() {
    std::printf("scenarios_rendertex\n");
    auto chk = [&](const char* tex, const char* wantRender, const char* wantClean){
        ++g_scenarios; ++g_checks;
        MathNode ast = parseTex(tex);
        if (toRenderTex(ast) != std::string(wantRender) || toTex(ast) != std::string(wantClean)) {
            ++g_failures;
            std::printf("  FAIL rendertex(%s): render=\"%s\"(want \"%s\") clean=\"%s\"(want \"%s\")\n",
                        tex, toRenderTex(ast).c_str(), wantRender, toTex(ast).c_str(), wantClean);
        }
    };
    chk("a+b", "a+b", "a+b");                                   // без структур — не меняется
    chk("\\frac{}{}", "\\frac{\\enspace}{\\enspace}", "\\frac{}{}");        // пустая дробь → распорки
    chk("\\frac{a}{}", "\\frac{a}{\\enspace}", "\\frac{a}{}");        // пустой знаменатель
    chk("\\frac{}{b}", "\\frac{\\enspace}{b}", "\\frac{}{b}");        // пустой числитель
    chk("x^{}", "x^{\\enspace}", "x^{}");                             // пустой индекс
    chk("\\sqrt{}", "\\sqrt{\\enspace}", "\\sqrt{}");                 // пустой корень
    chk("\\frac{a}{b}", "\\frac{a}{b}", "\\frac{a}{b}");        // заполненная — не меняется
}

// --- Группа: Photomath-шаблоны и Shift+Tab ----------------------------------
static void scenarios_templates() {
    std::printf("scenarios_templates\n");
    CHECK_EDIT("integral template", [](AstCursor& c){ c.insertBigOperator("int"); }, "\\int^{}_{}");
    CHECK_EDIT("sum template", [](AstCursor& c){ c.insertBigOperator("sum"); }, "\\sum^{}_{}");
    CHECK_EDIT("prod template", [](AstCursor& c){ c.insertBigOperator("prod"); }, "\\prod^{}_{}");
    CHECK_EDIT("integral lower limit", [](AstCursor& c){ c.insertBigOperator("int"); c.insertChar('0'); }, "\\int^{}_{0}");
    CHECK_EDIT("integral both limits", [](AstCursor& c){
        c.insertBigOperator("int"); c.insertChar('0');   // нижний предел
        c.move(Dir::Up); c.move(Dir::Up);                // база -> верхний
        c.insertChar('1'); }, "\\int^{1}_{0}");
    CHECK_EDIT("abs |x|", [](AstCursor& c){
        c.insertChar('|'); c.insertChar('|'); c.move(Dir::Left); c.insertChar('x'); }, "|x|");
    CHECK_EDIT("parens (x)", [](AstCursor& c){
        c.insertChar('('); c.insertChar(')'); c.move(Dir::Left); c.insertChar('x'); }, "(x)");
    CHECK_EDIT("brackets [n]", [](AstCursor& c){
        c.insertChar('['); c.insertChar(']'); c.move(Dir::Left); c.insertChar('n'); }, "[n]");
    CHECK_EDIT("tabPrev back to num", [](AstCursor& c){
        c.insertFraction(); c.insertChar('a'); c.tabNextField(); c.insertChar('b');
        c.tabPrevField(); c.insertChar('z'); }, "\\frac{za}{b}");
    CHECK_EDIT("tabNext then tabPrev", [](AstCursor& c){
        c.insertFraction(); c.tabNextField(); c.tabPrevField(); c.insertChar('n'); }, "\\frac{n}{}");
    CHECK_EDIT("integrand after operator", [](AstCursor& c){
        c.insertBigOperator("int"); c.moveToEnd(); c.insertChar('x'); }, "\\int^{}_{}x");
    // Два интеграла: навигация в нижний предел ПРАВОГО не уезжает к левому.
    CHECK_EDIT("two integrals: right sub", [](AstCursor& c){
        c.setFromTex("\\int^{d}_{c}+\\int^{}_{}");
        c.moveToEnd();            // после правого интеграла
        c.move(Dir::Left);        // в нижний предел правого (последнее поле)
        c.insertChar('x'); }, "\\int^{d}_{c}+\\int^{}_{x}");
    // Down из верхнего предела правого интеграла остаётся в правом (база), затем в его низ.
    CHECK_EDIT("two integrals: Down stays right", [](AstCursor& c){
        c.setFromTex("a+\\int^{}_{}");
        c.moveToEnd();            // после правого интеграла
        c.move(Dir::Left);        // в нижний предел (последнее поле)
        c.move(Dir::Up);          // -> база
        c.move(Dir::Up);          // -> верхний предел
        c.insertChar('n');        // верхний = n
        c.move(Dir::Down);        // -> база
        c.move(Dir::Down);        // -> нижний предел (того же интеграла)
        c.insertChar('0'); }, "a+\\int^{n}_{0}");
}

// -------------------------------------------------------------------------- //
//  Undo / Redo
// -------------------------------------------------------------------------- //

static void test_undo_redo() {
    std::printf("test_undo_redo\n");

    // Набор символов объединяется в один шаг отмены.
    {
        AstCursor c;
        c.snapshot(true); c.insertChar('a');
        c.snapshot(true); c.insertChar('b');
        c.snapshot(true); c.insertChar('c');
        CHECK_EQ(c.toTex(), "abc");
        CHECK(c.canUndo());
        CHECK(c.undo());
        CHECK_EQ(c.toTex(), "");          // одно Ctrl+Z убирает весь набор
        CHECK(c.canRedo());
        CHECK(c.redo());
        CHECK_EQ(c.toTex(), "abc");
    }

    // Структурная правка — отдельный шаг; новая правка стирает redo.
    {
        AstCursor c;
        c.snapshot(true); c.insertChar('x');
        c.snapshot(false); c.insertFraction();
        CHECK(c.toTex().find("frac") != std::string::npos);
        CHECK(c.undo());
        CHECK_EQ(c.toTex(), "x");
        CHECK(c.redo());
        CHECK(c.toTex().find("frac") != std::string::npos);
        CHECK(c.undo());                  // назад к "x"
        c.snapshot(true); c.insertChar('y');   // новая правка стирает redo
        CHECK(!c.canRedo());
    }

    // Движение курсора завершает «набор» — отмена идёт по отдельным символам.
    {
        AstCursor c;
        c.snapshot(true); c.insertChar('x');
        c.move(Dir::Left);
        c.snapshot(true); c.insertChar('y');
        CHECK_EQ(c.toTex(), "yx");
        CHECK(c.undo()); CHECK_EQ(c.toTex(), "x");
        CHECK(c.undo()); CHECK_EQ(c.toTex(), "");
        CHECK(!c.undo());                 // история исчерпана
    }

    // Холостой снимок отбрасывается (discardLastUndo).
    {
        AstCursor c;
        c.snapshot(false); c.insertChar('a');
        c.snapshot(false);                // ничего не поменяли
        c.discardLastUndo();
        CHECK(c.undo()); CHECK_EQ(c.toTex(), "");  // вернулись сразу к пустому
    }
}

// -------------------------------------------------------------------------- //
//  Выделение и буфер обмена
// -------------------------------------------------------------------------- //

static void test_selection() {
    std::printf("test_selection\n");

    // Shift+Left выделяет символы; selectedTex/GlyphRange корректны.
    {
        AstCursor c; c.setFromTex("a+bc");   // курсор в конце (offset=4)
        CHECK(!c.hasSelection());
        c.extendSelection(Dir::Left);        // 'c'
        c.extendSelection(Dir::Left);        // 'bc'
        CHECK(c.hasSelection());
        auto off = c.selectionOffsets();
        CHECK(off && off->first == 2 && off->second == 4);
        CHECK_EQ(c.selectedTex(), "bc");
        auto gr = c.selectedGlyphRange();
        CHECK(gr && gr->first == 2 && gr->second == 4);
        CHECK(c.deleteSelection());
        CHECK_EQ(c.toTex(), "a+");
        CHECK(!c.hasSelection());
    }

    // Структура выделяется как единый атом.
    {
        AstCursor c; c.setFromTex("\\frac{1}{2}c");
        c.moveToStart();
        c.extendSelection(Dir::Right);       // вся дробь — один атом
        CHECK(c.hasSelection());
        CHECK_EQ(c.selectedTex(), "\\frac{1}{2}");
    }

    // selectAll + удаление.
    {
        AstCursor c; c.setFromTex("xyz");
        c.selectAll();
        CHECK(c.hasSelection());
        CHECK_EQ(c.selectedTex(), "xyz");
        CHECK(c.deleteSelection());
        CHECK_EQ(c.toTex(), "");
    }

    // Вставка (paste) замещает выделение; без выделения — по курсору.
    {
        AstCursor c; c.setFromTex("a+b");
        c.selectAll();
        c.insertTex("\\frac{1}{2}");
        CHECK_EQ(c.toTex(), "\\frac{1}{2}");
    }
    {
        AstCursor c; c.setFromTex("xy");
        c.moveToStart();
        c.insertTex("ab");
        CHECK_EQ(c.toTex(), "abxy");
    }

    // Смена поля сбрасывает выделение (другой Row).
    {
        AstCursor c; c.setFromTex("\\frac{ab}{cd}");
        c.moveToStart();
        c.move(Dir::Right);                  // в числитель
        c.extendSelection(Dir::Right);       // выделили 'a'
        CHECK(c.hasSelection());
        c.move(Dir::Down);                   // ушли в знаменатель — выделения нет
        CHECK(!c.hasSelection());
    }
}

// -------------------------------------------------------------------------- //
//  Fuzzing: устойчивость round-trip toTex -> parse -> toTex на случайных деревьях
// -------------------------------------------------------------------------- //

static MathNode randomRow(std::mt19937& rng, int depth);

static MathNode randomNode(std::mt19937& rng, int depth) {
    int kinds = (depth > 0) ? 9 : 2;      // на глубине 0 только листья
    std::uniform_int_distribution<int> pick(0, kinds - 1);
    switch (pick(rng)) {
        case 0: {
            static const char* chars = "abcxyz019+=-()[]";
            std::uniform_int_distribution<int> ci(0, int(std::strlen(chars)) - 1);
            return MathNode::makeChar(chars[ci(rng)]);
        }
        case 1: {
            static const char* syms[] = {"alpha", "beta", "pi", "gamma",
                                         "int", "sum", "cdot", "infty", ",", "sin"};
            std::uniform_int_distribution<int> si(0, 9);
            return MathNode::makeSymbol(syms[si(rng)]);
        }
        case 2: {
            MathNode f{.kind = MathKind::Frac};
            f.children.push_back(randomRow(rng, depth - 1));
            f.children.push_back(randomRow(rng, depth - 1));
            return f;
        }
        case 3: {
            MathNode s{.kind = MathKind::Sqrt};
            s.children.push_back(randomRow(rng, depth - 1));
            return s;
        }
        case 4: {
            MathNode g{.kind = MathKind::Group};
            g.children.push_back(randomRow(rng, depth - 1));
            return g;
        }
        case 5: {
            MathNode sc{.kind = MathKind::Script};
            sc.children.push_back(randomRow(rng, depth - 1));   // база
            sc.children.push_back(randomRow(rng, depth - 1));   // sup
            sc.has_sup = true;
            return sc;
        }
        case 7: {
            static const char* acc[] = {"hat", "bar", "vec", "tilde",
                                        "widehat", "overline", "underline"};
            std::uniform_int_distribution<int> ai(0, 6);
            MathNode a{.kind = MathKind::Accent};
            a.command = acc[ai(rng)];
            a.children.push_back(randomRow(rng, depth - 1));
            return a;
        }
        case 8: {
            std::uniform_int_distribution<int> rc(1, 2);
            const int rows = rc(rng), cols = rc(rng);
            MathNode m{.kind = MathKind::Matrix};
            m.matrixCols = std::size_t(cols);
            for (int i = 0; i < rows * cols; ++i)
                m.children.push_back(randomRow(rng, depth - 1));
            // Иногда — со скобками (тестируем \left…\right round-trip).
            static const char* pairs[] = {"", "()", "[]", "{."};
            std::uniform_int_distribution<int> bi(0, 3);
            const char* p = pairs[bi(rng)];
            if (p[0]) { m.matrixOpen = p[0]; m.matrixClose = (p[1] == '.') ? 0 : p[1]; }
            return m;
        }
        default: {
            MathNode sc{.kind = MathKind::Script};
            sc.children.push_back(randomRow(rng, depth - 1));   // база
            sc.children.push_back(randomRow(rng, depth - 1));   // sub
            sc.has_sub = true;
            return sc;
        }
    }
}

static MathNode randomRow(std::mt19937& rng, int depth) {
    MathNode row = MathNode::makeRow();
    std::uniform_int_distribution<int> nAtoms(1, 4);
    const int n = nAtoms(rng);
    for (int i = 0; i < n; ++i) row.children.push_back(randomNode(rng, depth));
    return row;
}

static void test_fuzz_roundtrip() {
    std::printf("test_fuzz_roundtrip\n");
    std::mt19937 rng(0xC0FFEEu);
    const int N = 3000;
    int shown = 0;
    for (int i = 0; i < N; ++i) {
        const MathNode tree = randomRow(rng, 3);
        const std::string s1 = toTex(tree);              // сериализация случайного дерева
        const std::string s2 = toTex(parseTex(s1));      // разбор + сериализация
        const std::string s3 = toTex(parseTex(s2));      // должно быть неподвижной точкой
        ++g_scenarios; ++g_checks;
        if (s2 != s3) {
            ++g_failures;
            if (shown++ < 10)
                std::printf("  FAIL round-trip нестабилен: \"%s\" -> \"%s\" -> \"%s\"\n",
                            s1.c_str(), s2.c_str(), s3.c_str());
            continue;
        }
        // Инвариант навигации на случайной формуле (каретка валидна, обход обратим).
        checkFormula(s2, __FILE__, __LINE__);
    }
}

// -------------------------------------------------------------------------- //
//  Новые структуры: акценты, линии, матрицы
// -------------------------------------------------------------------------- //

static void test_structures() {
    std::printf("test_structures\n");

    // Round-trip акцентов и линий.
    CHECK_EQ(toTex(parseTex("\\hat{x}")), "\\hat{x}");
    CHECK_EQ(toTex(parseTex("\\vec{v}")), "\\vec{v}");
    CHECK_EQ(toTex(parseTex("\\overline{a+b}")), "\\overline{a+b}");
    CHECK_EQ(toTex(parseTex("\\underline{x}")), "\\underline{x}");
    CHECK_EQ(toTex(parseTex("\\bar{y}^{2}")), "{\\bar{y}}^{2}");  // база-акцент берётся в {}

    // glyphSpan: метка акцента добавляет +1, линии — нет.
    CHECK_GLYPHS("\\hat{x}", 2);          // x + метка
    CHECK_GLYPHS("\\widehat{xy}", 3);     // xy + метка
    CHECK_GLYPHS("\\overline{ab}", 2);    // без метки
    CHECK_GLYPHS("\\underline{x}", 1);

    // Матрица: round-trip и число ячеек/глифов.
    CHECK_EQ(toTex(parseTex("\\matrix{a & b \\cr c & d}")), "\\matrix{a & b \\cr c & d}");
    CHECK_EQ(toTex(parseTex("\\matrix{a & b}")), "\\matrix{a & b}");
    CHECK_GLYPHS("\\matrix{a & b \\cr c & d}", 4);

    // Матрица в скобках (\left…\right) — round-trip и +2 глифа-ограничителя.
    CHECK_EQ(toTex(parseTex("\\left(\\matrix{a & b \\cr c & d}\\right)")),
             "\\left(\\matrix{a & b \\cr c & d}\\right)");
    CHECK_EQ(toTex(parseTex("\\left[\\matrix{x \\cr y}\\right]")),
             "\\left[\\matrix{x \\cr y}\\right]");
    CHECK_EQ(toTex(parseTex("\\left\\{\\matrix{a \\cr b}\\right.")),
             "\\left\\{\\matrix{a \\cr b}\\right.");   // cases: только левая скобка
    CHECK_GLYPHS("\\left(\\matrix{a & b \\cr c & d}\\right)", 6);   // 4 ячейки + 2 скобки
    {
        MathNode m = parseTex("\\left(\\matrix{a & b}\\right)");
        CHECK(m.children.size() == 1 && m.children[0].kind == MathKind::Matrix);
        CHECK(m.children[0].matrixOpen == '(' && m.children[0].matrixClose == ')');
    }
    // Вставка матрицы в скобках.
    CHECK_EDIT("insert bracketed matrix",
        [](AstCursor& c){
            c.insertMatrix(1, 2, '(', ')');
            c.insertChar('a'); c.tabNextField(); c.insertChar('b');
        }, "\\left(\\matrix{a & b}\\right)");
    {
        MathNode m = parseTex("\\matrix{a & b \\cr c & d}");
        CHECK(m.children.size() == 1);
        CHECK(m.children[0].kind == MathKind::Matrix);
        CHECK(m.children[0].matrixCols == 2);
        CHECK(m.children[0].children.size() == 4);
    }

    // Вставка акцента: курсор попадает внутрь поля, ввод пишет под акцент.
    CHECK_EDIT("insert accent hat",
        [](AstCursor& c){ c.insertAccent("hat"); c.insertChar('x'); }, "\\hat{x}");

    // Вставка матрицы 2x2 и заполнение по Tab.
    CHECK_EDIT("insert matrix 2x2",
        [](AstCursor& c){
            c.insertMatrix(2, 2);
            c.insertChar('a'); c.tabNextField();
            c.insertChar('b'); c.tabNextField();
            c.insertChar('c'); c.tabNextField();
            c.insertChar('d');
        }, "\\matrix{a & b \\cr c & d}");

    // 2D-навигация по матрице: Down/Up переходят между строками.
    {
        AstCursor c; c.setFromTex("\\matrix{a & b \\cr c & d}");
        c.moveToStart();
        c.move(Dir::Right);          // войти в ячейку a (поле 0)
        CHECK(c.depth() == 1);
        c.move(Dir::Down);           // a -> c (та же колонка, строка ниже)
        CHECK(c.path().size() == 2 && c.path()[1] == 2);   // поле 2 = ячейка (1,0)
        c.move(Dir::Up);             // назад в a
        CHECK(c.path()[1] == 0);
    }

    // Инварианты навигации на формулах со структурами.
    CHECK_FORMULA("\\hat{x}+\\vec{y}");
    CHECK_FORMULA("\\overline{a+b}^{2}");
    CHECK_FORMULA("\\matrix{a & b \\cr c & d}");
    CHECK_FORMULA("\\frac{\\hat{x}}{\\matrix{1 & 2 \\cr 3 & 4}}");
    CHECK_FORMULA("A=\\matrix{\\alpha & \\beta \\cr \\gamma & \\delta}");
}

int main() {
    test_roundtrip();
    test_undo_redo();
    test_selection();
    test_structures();
    test_fuzz_roundtrip();
    test_nav_integral();
    test_nav_fraction_boundary();
    test_nav_function_multiglyph();
    test_nav_thinspace();
    test_nav_fraction_vertical();
    test_set_from_glyph();
    test_parse_structure();
    test_linear_typing();
    test_navigation_linear();
    test_enter_fraction();
    test_enter_empty_field();
    test_insert_script();
    test_vertical_nav_fraction();
    test_backspace_leaf();
    test_backspace_empty_structure();
    test_delete_keeps_valid_tex();
    test_glyph_index_mapping();

    // Большой набор сценариев
    scenarios_formulas();
    scenarios_glyphcount();
    scenarios_editing();
    scenarios_rendertex();
    scenarios_templates();
    scenarios_vertical();
    scenarios_setfromglyph();

    std::printf("\n%d сценариев, %d проверок, %d ошибок\n", g_scenarios, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
