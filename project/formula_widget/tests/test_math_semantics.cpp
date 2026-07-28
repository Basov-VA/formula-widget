// Тесты семантического слоя: MathNode -> выражение SymPy.
// Чистый C++ (без Qt/mfl): парсим TeX -> дерево -> toSympy, сверяем строку.

#include "math_ast.hpp"
#include "math_semantics.hpp"

#include <cstdio>
#include <string>

using namespace formula;

static int g_fail = 0, g_checks = 0;

static void checkEq(const std::string& tex, const std::string& got, const std::string& want,
                    const char* file, int line) {
    ++g_checks;
    if (got != want) {
        ++g_fail;
        std::printf("  FAIL [%s:%d] tex=\"%s\": got \"%s\", want \"%s\"\n",
                    file, line, tex.c_str(), got.c_str(), want.c_str());
    }
}
#define CHECK_SYMPY(tex, want) checkEq((tex), toSympy(parseTex(tex)).sympy, (want), __FILE__, __LINE__)

static void checkVar(const std::string& tex, const std::string& want, const char* file, int line) {
    ++g_checks;
    const std::string got = toSympy(parseTex(tex)).mainVar;
    if (got != want) {
        ++g_fail;
        std::printf("  FAIL [%s:%d] mainVar(\"%s\"): got \"%s\", want \"%s\"\n",
                    file, line, tex.c_str(), got.c_str(), want.c_str());
    }
}
#define CHECK_VAR(tex, want) checkVar((tex), (want), __FILE__, __LINE__)

int main() {
    std::printf("math_semantics tests\n");

    // Арифметика и неявное умножение
    CHECK_SYMPY("x+1", "x+1");
    CHECK_SYMPY("2x", "2*x");
    CHECK_SYMPY("2\\pi", "2*pi");
    CHECK_SYMPY("a-b", "a-b");
    CHECK_SYMPY("x(x+1)", "x*(x+1)");
    CHECK_SYMPY("2\\cdot 3", "2*3");

    // Дроби, корни, степени, индексы
    CHECK_SYMPY("\\frac{1}{2}", "((1)/(2))");
    CHECK_SYMPY("\\sqrt{x}", "sqrt(x)");
    CHECK_SYMPY("x^{2}", "(x)**(2)");
    CHECK_SYMPY("x^{2}+y^{2}", "(x)**(2)+(y)**(2)");
    CHECK_SYMPY("x_{i}", "Symbol('x_i')");

    // Функции
    CHECK_SYMPY("\\sin(x)", "sin(x)");
    CHECK_SYMPY("\\cos(2x)", "cos(2*x)");
    CHECK_SYMPY("\\ln(x)", "log(x)");

    // Определённый интеграл — флагманский кейс
    CHECK_SYMPY("\\int_{0}^{1}x^{2}\\,dx", "Integral((x)**(2), (x, 0, 1))");
    CHECK_SYMPY("\\int_{0}^{1}x^{2}\\,dx=\\frac{1}{3}", "Integral((x)**(2), (x, 0, 1))");
    CHECK_SYMPY("\\int x\\,dx", "Integral(x, x)");

    // Сумма
    CHECK_SYMPY("\\sum_{i=1}^{n}i", "Sum(i, (i, 1, n))");
    CHECK_SYMPY("\\sum_{k=1}^{n}k^{2}", "Sum((k)**(2), (k, 1, n))");

    // Матрица
    CHECK_SYMPY("\\matrix{1 & 2 \\cr 3 & 4}", "Matrix([[1, 2], [3, 4]])");

    // Акцент — берётся содержимое
    CHECK_SYMPY("\\vec{v}", "v");
    CHECK_SYMPY("\\hat{x}+1", "x+1");

    // Выбор стороны уравнения: y=<expr> → берём правую (функцию)
    CHECK_SYMPY("y=1", "1");
    CHECK_SYMPY("y=x^{2}", "(x)**(2)");
    CHECK_SYMPY("y=\\sin(x)", "sin(x)");
    CHECK_VAR("y=x^{2}", "x");
    // ∫…=значение → берём левую (интеграл)
    CHECK_SYMPY("\\int_{0}^{1}x^{2}\\,dx=\\frac{1}{3}", "Integral((x)**(2), (x, 0, 1))");

    // Главная переменная
    CHECK_VAR("x^{2}+1", "x");
    CHECK_VAR("t+1", "t");
    CHECK_VAR("a+b", "a");

    std::printf("\n%s: %d проверок, %d ошибок\n",
                g_fail == 0 ? "ALL OK" : "FAILURES", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
