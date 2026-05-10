# Тестирование и валидация пунктов 2 и 3

> Этот документ дополняет [PLAN_POINTS_2_3.md](PLAN_POINTS_2_3.md) — описывает как проверить корректность реализации.

---

## Стратегия тестирования: обзор

Тестирование разбито на 4 уровня:

| Уровень | Что проверяем | Метод |
|---------|---------------|-------|
| **Unit-тесты** | Отдельные компоненты (конвертация координат, font_face, дерево) | doctest / Google Test |
| **Визуальные тесты** | Корректность рендеринга формул | Сравнение с эталонными SVG из mfl |
| **Структурные тесты** | Корректность дерева элементов | Проверка структуры дерева для известных формул |
| **Интеграционные тесты** | Виджет + дерево + hit-testing | Ручное + автоматическое тестирование |

---

## Часть 1: Тестирование пункта 2 (Qt-виджет с рисованием)

### Тест 2.1: Проверка FtFontFace — unit-тесты

Цель: убедиться что наша реализация `abstract_font_face` через FreeType/HarfBuzz возвращает корректные данные.

```cpp
// test_ft_font_face.cpp
TEST_CASE("FtFontFace returns valid math constants") {
    FtLibrary lib;
    FtFontFace face(mfl::font_family::italic, lib);

    auto constants = face.constants();

    // Проверить что значения ненулевые и разумные
    CHECK(constants.axis_height > 0);
    CHECK(constants.fraction_rule_thickness > 0);
    CHECK(constants.subscript_shift_down > 0);
    CHECK(constants.superscript_shift_up > 0);
    CHECK(constants.radical_vertical_gap > 0);
    CHECK(constants.radical_rule_thickness > 0);
}

TEST_CASE("FtFontFace resolves glyph indices for basic characters") {
    FtLibrary lib;
    FtFontFace face(mfl::font_family::italic, lib);
    face.set_size(mfl::points{12.0});

    // 'x' = U+0078
    auto idx = face.glyph_index_from_code_point(0x0078, false);
    CHECK(idx != 0);  // 0 = .notdef = символ не найден

    // Проверить что glyph_info возвращает разумные размеры
    auto info = face.glyph_info(idx);
    CHECK(info.width > 0);
    CHECK(info.height > 0);
    CHECK(info.depth >= 0);  // depth может быть 0 для символов без нижней части
}

TEST_CASE("FtFontFace resolves Greek letters") {
    FtLibrary lib;
    FtFontFace face(mfl::font_family::italic, lib);
    face.set_size(mfl::points{12.0});

    // α = U+03B1
    auto idx = face.glyph_index_from_code_point(0x03B1, false);
    CHECK(idx != 0);

    // Σ = U+03A3
    idx = face.glyph_index_from_code_point(0x03A3, false);
    CHECK(idx != 0);
}

TEST_CASE("FtFontFace returns size variants for parentheses") {
    FtLibrary lib;
    FtFontFace face(mfl::font_family::roman, lib);
    face.set_size(mfl::points{12.0});

    // '(' = U+0028
    auto variants = face.vertical_size_variants(0x0028);
    CHECK(variants.size() > 1);  // должны быть варианты разных размеров

    // Размеры должны расти
    for (size_t i = 1; i < variants.size(); ++i) {
        CHECK(variants[i].size >= variants[i-1].size);
    }
}

TEST_CASE("FtFontFace set_size changes glyph metrics") {
    FtLibrary lib;
    FtFontFace face(mfl::font_family::italic, lib);

    face.set_size(mfl::points{10.0});
    auto idx = face.glyph_index_from_code_point(0x0078, false);
    auto info_small = face.glyph_info(idx);

    face.set_size(mfl::points{20.0});
    auto info_large = face.glyph_info(idx);

    // Больший размер → большие метрики
    CHECK(info_large.width > info_small.width);
    CHECK(info_large.height > info_small.height);
}
```

**Критерий прохождения:** Все CHECK проходят. Если какой-то не проходит — проблема в реализации FtFontFace или в загрузке шрифтов.

### Тест 2.2: Сравнение layout с эталоном mfl

Цель: убедиться что наш FtFontFace даёт тот же layout что и тестовый font_face из mfl.

```cpp
TEST_CASE("Our FtFontFace produces same layout as test font_face") {
    // Наша реализация
    FtLibrary our_lib;
    auto our_creator = [&](mfl::font_family f) -> std::unique_ptr<mfl::abstract_font_face> {
        return std::make_unique<FtFontFace>(f, our_lib);
    };

    // Эталонная реализация из тестов mfl (если доступна)
    mfl::fft::freetype test_ft;
    auto test_creator = [&](mfl::font_family f) -> std::unique_ptr<mfl::abstract_font_face> {
        return std::make_unique<mfl::fft::font_face>(f, test_ft);
    };

    std::vector<std::string> formulas = {
        R"(x)", R"(a+b)", R"(\frac{a}{b})", R"(x^2)", R"(\sqrt{x})",
    };

    for (const auto& formula : formulas) {
        CAPTURE(formula);
        auto our_result = mfl::layout(formula, mfl::points{12.0}, our_creator);
        auto test_result = mfl::layout(formula, mfl::points{12.0}, test_creator);

        CHECK(!our_result.error.has_value());
        CHECK(!test_result.error.has_value());

        // Количество глифов и линий должно совпадать
        CHECK(our_result.glyphs.size() == test_result.glyphs.size());
        CHECK(our_result.lines.size() == test_result.lines.size());

        // Позиции глифов должны быть близки
        constexpr double tolerance = 0.5;  // 0.5 pt
        for (size_t i = 0; i < our_result.glyphs.size(); ++i) {
            CHECK(std::abs(our_result.glyphs[i].x.value() -
                          test_result.glyphs[i].x.value()) < tolerance);
            CHECK(std::abs(our_result.glyphs[i].y.value() -
                          test_result.glyphs[i].y.value()) < tolerance);
        }
    }
}
```

**Примечание:** Если используются те же шрифты STIX2, результаты должны совпадать точно (tolerance ≈ 0). Если шрифты разные — допуск нужно увеличить.

**Критерий прохождения:** Количество глифов/линий совпадает, позиции в пределах допуска.

### Тест 2.3: Проверка конвертации координат

```cpp
TEST_CASE("Coordinate conversion: mfl points to Qt pixels") {
    FormulaWidget widget;
    widget.resize(800, 200);
    widget.setDpi(96.0);

    // 72 points = 1 inch = 96 pixels при 96 DPI
    CHECK(widget.pointsToPixels(mfl::points{72.0}) == Approx(96.0));
    CHECK(widget.pointsToPixels(mfl::points{36.0}) == Approx(48.0));
    CHECK(widget.pointsToPixels(mfl::points{0.0}) == Approx(0.0));

    // При 72 DPI: 1 point = 1 pixel
    widget.setDpi(72.0);
    CHECK(widget.pointsToPixels(mfl::points{10.0}) == Approx(10.0));

    // При 144 DPI: 1 point = 2 pixels
    widget.setDpi(144.0);
    CHECK(widget.pointsToPixels(mfl::points{10.0}) == Approx(20.0));
}

TEST_CASE("Y-axis inversion: mfl (Y-up) to Qt (Y-down)") {
    FormulaWidget widget;
    widget.resize(800, 200);
    widget.setDpi(72.0);  // 1pt = 1px для простоты

    // В mfl: y=0 — базовая линия (низ формулы), y>0 — вверх
    // В Qt: y=0 — верх виджета, y>0 — вниз

    auto qt_pos_base = widget.mflToQt(mfl::points{0.0}, mfl::points{0.0});
    auto qt_pos_up = widget.mflToQt(mfl::points{0.0}, mfl::points{10.0});

    // Точка выше в mfl (y=10) должна быть выше в Qt (меньше y)
    CHECK(qt_pos_up.y() < qt_pos_base.y());

    // X не инвертируется
    auto qt_pos_right = widget.mflToQt(mfl::points{10.0}, mfl::points{0.0});
    CHECK(qt_pos_right.x() > qt_pos_base.x());
}
```

**Критерий прохождения:** Все проверки координат проходят. Это критически важно — ошибка здесь приведёт к зеркальному/перевёрнутому рендерингу.

### Тест 2.4: Рендеринг в QImage — формула не пустая

```cpp
TEST_CASE("Render formula to QImage and verify non-empty") {
    std::vector<std::string> formulas = {
        R"(x)", R"(\frac{a}{b})", R"(x^2 + y^2)", R"(\sqrt{x})",
        R"(\sum_{i=0}^n x_i)", R"(\int_0^1 f(x) \, dx)",
    };

    for (const auto& formula : formulas) {
        CAPTURE(formula);

        FormulaWidget widget;
        widget.resize(400, 100);
        widget.setFormula(QString::fromStdString(formula));

        QImage image(400, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);
        widget.render(&image);

        // Проверить что изображение не полностью белое
        bool has_non_white = false;
        for (int y = 0; y < image.height() && !has_non_white; ++y) {
            for (int x = 0; x < image.width() && !has_non_white; ++x) {
                if (image.pixelColor(x, y) != QColor(Qt::white)) {
                    has_non_white = true;
                }
            }
        }
        CHECK_MESSAGE(has_non_white, "Formula rendered as empty: " << formula);
    }
}

TEST_CASE("Different formulas produce different images") {
    auto render = [](const std::string& tex) {
        FormulaWidget widget;
        widget.resize(400, 100);
        widget.setFormula(QString::fromStdString(tex));
        QImage image(400, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);
        widget.render(&image);
        return image;
    };

    auto img1 = render(R"(x^2)");
    auto img2 = render(R"(\frac{a}{b})");
    auto img3 = render(R"(\sqrt{x})");

    CHECK(img1 != img2);
    CHECK(img2 != img3);
    CHECK(img1 != img3);
}
```

**Критерий прохождения:** Каждая формула рисует что-то (не пустое), разные формулы дают разные изображения.

### Тест 2.5: Визуальное сравнение с эталонными SVG

Это самый важный тест — сравнение с тем, что mfl уже генерирует в своих approval tests.

```cpp
TEST_CASE("Visual comparison: render to SVG and diff with approved") {
    // Используем Cairo для рендеринга SVG (как в тестах mfl)
    // Или рендерим в PNG через наш виджет и сравниваем визуально

    struct TestCase {
        std::string formula;
        std::string approved_svg_path;  // путь к эталонному SVG из mfl
    };

    std::vector<TestCase> cases = {
        {R"(f(a) = \frac{1}{2\pi i} \int_\gamma \frac{f(z)}{z-a} \, dz)",
         "mfl/tests/approval_tests/approved_files/docs.cauchy_integral.approved.svg"},
    };

    for (const auto& tc : cases) {
        CAPTURE(tc.formula);

        auto elements = mfl::layout(tc.formula, mfl::points{10.0}, create_font_face);
        CHECK(!elements.error.has_value());

        // Рендерить нашим рендерером в SVG
        std::string our_svg = render_to_svg(elements);

        // Сохранить для ручного сравнения
        save_file("test_output/our_" + sanitize(tc.formula) + ".svg", our_svg);

        // Базовые проверки SVG
        CHECK(our_svg.find("<svg") != std::string::npos);
        CHECK(our_svg.size() > 100);  // не пустой SVG
    }
}
```

**Критерий прохождения:** Визуально наш рендеринг выглядит так же как эталонные SVG. Автоматическое попиксельное сравнение возможно, но сложно из-за различий в рендеринге.

### Тест 2.6: Набор эталонных формул для ручной визуальной проверки

Создать демо-приложение, которое рисует сетку формул:

```cpp
// test_visual_grid.cpp — запускается вручную, результат проверяется глазами
const std::vector<std::pair<std::string, std::string>> test_formulas = {
    {"Simple variable",       R"(x)"},
    {"Addition",              R"(a + b)"},
    {"Fraction",              R"(\frac{a}{b})"},
    {"Nested fraction",       R"(\frac{1}{\frac{2}{3}})"},
    {"Square root",           R"(\sqrt{x})"},
    {"Nth root",              R"(\sqrt[3]{x})"},
    {"Superscript",           R"(x^2)"},
    {"Subscript",             R"(x_i)"},
    {"Both scripts",          R"(x_i^2)"},
    {"Sum with limits",       R"(\sum_{i=0}^{n} x_i)"},
    {"Integral",              R"(\int_0^1 f(x) \, dx)"},
    {"Greek letters",         R"(\alpha + \beta = \gamma)"},
    {"Parentheses",           R"(\left( \frac{a}{b} \right))"},
    {"Overline",              R"(\overline{abc})"},
    {"Accent",                R"(\hat{x} + \tilde{y})"},
    {"Euler's identity",      R"(e^{i\pi} + 1 = 0)"},
    {"Cauchy integral",       R"(f(a) = \frac{1}{2\pi i} \int_\gamma \frac{f(z)}{z-a} \, dz)"},
    {"Nested radicals",       R"(\sqrt{1+\sqrt{1+\sqrt{1+x}}})"},
};

// Для каждой формулы рисуем:
// 1. Название теста
// 2. TeX-строку
// 3. Отрендеренную формулу
// Всё в одном окне, скроллируемом
```

**Критерий прохождения:** Все формулы визуально корректны — дроби выровнены, корни правильной формы, индексы на своих местах, скобки масштабируются.

### Тест 2.7: Ошибочные формулы

```cpp
TEST_CASE("Error handling for invalid formulas") {
    auto elements = mfl::layout(R"(\frac{a})", mfl::points{12.0}, create_font_face);
    CHECK(elements.error.has_value());  // незакрытая скобка

    elements = mfl::layout(R"(\unknowncommand)", mfl::points{12.0}, create_font_face);
    CHECK(elements.error.has_value());  // неизвестная команда
}

TEST_CASE("Widget handles errors gracefully") {
    FormulaWidget widget;
    widget.resize(400, 100);
    widget.setFormula(R"(\frac{a})");  // ошибка

    // Не должно крашиться при рендеринге
    QImage image(400, 100, QImage::Format_ARGB32);
    image.fill(Qt::white);
    widget.render(&image);  // не должно упасть

    // Виджет должен показать сообщение об ошибке
    CHECK(widget.hasError());
}
```

---

## Часть 2: Тестирование пункта 3 (дерево элементов)

### Тест 3.1: Структура дерева для простых формул

Самый важный тест — проверить что дерево имеет правильную структуру для известных формул.

```cpp
// Вспомогательная функция поиска
const mfl::formula_node* find_child(const mfl::formula_node& parent,
                                     mfl::formula_node_type type) {
    for (const auto& child : parent.children) {
        if (child.type == type) return &child;
    }
    return nullptr;
}

const mfl::formula_node* find_descendant(const mfl::formula_node& node,
                                          mfl::formula_node_type type) {
    if (node.type == type) return &node;
    for (const auto& child : node.children) {
        if (auto* found = find_descendant(child, type)) return found;
    }
    return nullptr;
}

size_t count_descendants_of_type(const mfl::formula_node& node,
                                  mfl::formula_node_type type) {
    size_t count = (node.type == type) ? 1 : 0;
    for (const auto& child : node.children) {
        count += count_descendants_of_type(child, type);
    }
    return count;
}

TEST_CASE("Tree for simple fraction: \\frac{a}{b}") {
    auto elements = mfl::layout(R"(\frac{a}{b})", mfl::points{12.0}, create_font_face);
    CHECK(!elements.error.has_value());
    const auto& tree = elements.tree;

    // Корень
    CHECK(tree.type == mfl::formula_node_type::root);

    // Должен содержать дробь
    auto* fraction = find_descendant(tree, mfl::formula_node_type::fraction);
    REQUIRE(fraction != nullptr);

    // Дробь содержит числитель и знаменатель
    auto* numerator = find_child(*fraction, mfl::formula_node_type::numerator);
    auto* denominator = find_child(*fraction, mfl::formula_node_type::denominator);
    REQUIRE(numerator != nullptr);
    REQUIRE(denominator != nullptr);

    // Числитель содержит символ 'a'
    CHECK(count_descendants_of_type(*numerator, mfl::formula_node_type::symbol) >= 1);

    // Знаменатель содержит символ 'b'
    CHECK(count_descendants_of_type(*denominator, mfl::formula_node_type::symbol) >= 1);

    // Дробь содержит линию (черта дроби)
    // Линия может быть в самом fraction-узле или в его предке
    std::set<size_t> fraction_lines;
    collect_line_indices(*fraction, fraction_lines);
    CHECK(fraction_lines.size() >= 1);
}

TEST_CASE("Tree for superscript: x^2") {
    auto elements = mfl::layout(R"(x^2)", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    // Должен быть script_nucleus с 'x'
    auto* nucleus = find_descendant(tree, mfl::formula_node_type::script_nucleus);
    REQUIRE(nucleus != nullptr);

    // Должен быть superscript с '2'
    auto* sup = find_descendant(tree, mfl::formula_node_type::superscript);
    REQUIRE(sup != nullptr);

    // Superscript не должен содержать subscript
    auto* sub = find_descendant(tree, mfl::formula_node_type::subscript);
    CHECK(sub == nullptr);
}

TEST_CASE("Tree for subscript: x_i") {
    auto elements = mfl::layout(R"(x_i)", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    auto* sub = find_descendant(tree, mfl::formula_node_type::subscript);
    REQUIRE(sub != nullptr);

    auto* sup = find_descendant(tree, mfl::formula_node_type::superscript);
    CHECK(sup == nullptr);
}

TEST_CASE("Tree for both scripts: x_i^2") {
    auto elements = mfl::layout(R"(x_i^2)", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    auto* sub = find_descendant(tree, mfl::formula_node_type::subscript);
    auto* sup = find_descendant(tree, mfl::formula_node_type::superscript);
    auto* nucleus = find_descendant(tree, mfl::formula_node_type::script_nucleus);

    REQUIRE(sub != nullptr);
    REQUIRE(sup != nullptr);
    REQUIRE(nucleus != nullptr);
}

TEST_CASE("Tree for square root: \\sqrt{x+1}") {
    auto elements = mfl::layout(R"(\sqrt{x+1})", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    auto* radical = find_descendant(tree, mfl::formula_node_type::radical);
    REQUIRE(radical != nullptr);

    auto* radicand = find_child(*radical, mfl::formula_node_type::radicand);
    REQUIRE(radicand != nullptr);

    // Подкоренное выражение содержит символы x, +, 1
    CHECK(count_descendants_of_type(*radicand, mfl::formula_node_type::symbol) >= 3);

    // Нет степени корня
    auto* degree = find_child(*radical, mfl::formula_node_type::degree);
    CHECK(degree == nullptr);
}

TEST_CASE("Tree for nth root: \\sqrt[3]{x}") {
    auto elements = mfl::layout(R"(\sqrt[3]{x})", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    auto* radical = find_descendant(tree, mfl::formula_node_type::radical);
    REQUIRE(radical != nullptr);

    // Есть степень корня
    auto* degree = find_child(*radical, mfl::formula_node_type::degree);
    REQUIRE(degree != nullptr);

    auto* radicand = find_child(*radical, mfl::formula_node_type::radicand);
    REQUIRE(radicand != nullptr);
}

TEST_CASE("Tree for overline: \\overline{abc}") {
    auto elements = mfl::layout(R"(\overline{abc})", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    auto* ol = find_descendant(tree, mfl::formula_node_type::overline);
    REQUIRE(ol != nullptr);

    // Содержит символы a, b, c
    CHECK(count_descendants_of_type(*ol, mfl::formula_node_type::symbol) >= 3);
}

TEST_CASE("Tree for sum with limits: \\sum_{i=0}^n x_i") {
    auto elements = mfl::layout(R"(\sum_{i=0}^n x_i)", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    auto* big_op = find_descendant(tree, mfl::formula_node_type::big_op);
    REQUIRE(big_op != nullptr);
}

TEST_CASE("Tree for left-right delimiters") {
    auto elements = mfl::layout(R"(\left( \frac{a}{b} \right))", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    auto* lr = find_descendant(tree, mfl::formula_node_type::left_right);
    REQUIRE(lr != nullptr);

    // Внутри должна быть дробь
    auto* fraction = find_descendant(*lr, mfl::formula_node_type::fraction);
    REQUIRE(fraction != nullptr);
}
```

**Критерий прохождения:** Для каждой формулы дерево содержит ожидаемые типы узлов в правильной иерархии.

### Тест 3.2: Полнота дерева — все глифы и линии учтены

```cpp
void collect_glyph_indices(const mfl::formula_node& node, std::set<size_t>& result) {
    for (auto idx : node.glyph_indices) result.insert(idx);
    for (const auto& child : node.children) collect_glyph_indices(child, result);
}

void collect_line_indices(const mfl::formula_node& node, std::set<size_t>& result) {
    for (auto idx : node.line_indices) result.insert(idx);
    for (const auto& child : node.children) collect_line_indices(child, result);
}

TEST_CASE("All glyphs are accounted for in the tree") {
    std::vector<std::string> formulas = {
        R"(x)", R"(\frac{a}{b})", R"(x^2)", R"(\sqrt{x})",
        R"(\sum_{i=0}^n x_i)", R"(\int_0^1 f(x) dx)",
        R"(\left( \frac{a}{b} \right))",
        R"(\sqrt[3]{\frac{x}{y}})",
        R"(A^{x_i^2}_{j^{2n}_{n,m}})",
    };

    for (const auto& formula : formulas) {
        CAPTURE(formula);
        auto elements = mfl::layout(formula, mfl::points{12.0}, create_font_face);
        REQUIRE(!elements.error.has_value());

        // Собрать все glyph_indices из дерева
        std::set<size_t> tree_glyph_indices;
        collect_glyph_indices(elements.tree, tree_glyph_indices);

        // Каждый глиф из layout_elements должен быть в дереве
        for (size_t i = 0; i < elements.glyphs.size(); ++i) {
            CHECK_MESSAGE(tree_glyph_indices.count(i) == 1,
                "Glyph " << i << " not in tree for: " << formula);
        }

        // Общее количество должно совпадать
        CHECK(tree_glyph_indices.size() == elements.glyphs.size());

        // Аналогично для линий
        std::set<size_t> tree_line_indices;
        collect_line_indices(elements.tree, tree_line_indices);

        for (size_t i = 0; i < elements.lines.size(); ++i) {
            CHECK_MESSAGE(tree_line_indices.count(i) == 1,
                "Line " << i << " not in tree for: " << formula);
        }
    }
}
```

**Критерий прохождения:** Для каждой формулы множество индексов глифов/линий в дереве точно совпадает с множеством индексов в `layout_elements`. Ни один элемент не потерян и не продублирован.

### Тест 3.3: Уникальность — каждый глиф принадлежит ровно одному узлу

```cpp
void count_glyph_ownership(const mfl::formula_node& node, std::map<size_t, int>& counts) {
    for (auto idx : node.glyph_indices) counts[idx]++;
    for (const auto& child : node.children) count_glyph_ownership(child, counts);
}

TEST_CASE("Each glyph belongs to exactly one leaf node") {
    std::vector<std::string> formulas = {
        R"(\frac{x^2}{y_i})", R"(\sqrt[3]{\frac{a}{b}})",
        R"(\sum_{i=0}^n x_i)", R"(\left( x + y \right))",
    };

    for (const auto& formula : formulas) {
        CAPTURE(formula);
        auto elements = mfl::layout(formula, mfl::points{12.0}, create_font_face);

        std::map<size_t, int> glyph_count;
        count_glyph_ownership(elements.tree, glyph_count);

        for (const auto& [idx, count] : glyph_count) {
            CHECK_MESSAGE(count == 1,
                "Glyph " << idx << " belongs to " << count << " nodes for: " << formula);
        }
    }
}
```

**Критерий прохождения:** Каждый глиф принадлежит ровно одному узлу дерева. Если глиф принадлежит нескольким — ошибка в логике построения дерева.

### Тест 3.4: Bounding box корректность

```cpp
TEST_CASE("Bounding boxes have non-negative dimensions") {
    std::vector<std::string> formulas = {
        R"(x)", R"(\frac{a+b}{c-d})", R"(\sqrt{x^2+y^2})",
    };

    for (const auto& formula : formulas) {
        CAPTURE(formula);
        auto elements = mfl::layout(formula, mfl::points{12.0}, create_font_face);

        std::function<void(const mfl::formula_node&)> check_bbox;
        check_bbox = [&](const mfl::formula_node& node) {
            CHECK(node.bbox_width.value() >= 0.0);
            CHECK(node.bbox_height.value() >= 0.0);

            if (!node.children.empty() || !node.glyph_indices.empty()) {
                CHECK(node.bbox_width.value() > 0.0);
                CHECK(node.bbox_height.value() > 0.0);
            }

            for (const auto& child : node.children) check_bbox(child);
        };

        check_bbox(elements.tree);
    }
}

TEST_CASE("Child bounding boxes are contained within parent") {
    auto elements = mfl::layout(R"(\frac{a}{b})", mfl::points{12.0}, create_font_face);

    std::function<void(const mfl::formula_node&)> check_containment;
    check_containment = [&](const mfl::formula_node& parent) {
        for (const auto& child : parent.children) {
            constexpr double eps = 0.5;  // 0.5 pt tolerance

            CHECK(child.bbox_x.value() >= parent.bbox_x.value() - eps);
            CHECK(child.bbox_y.value() >= parent.bbox_y.value() - eps);
            CHECK(child.bbox_x.value() + child.bbox_width.value()
                  <= parent.bbox_x.value() + parent.bbox_width.value() + eps);
            CHECK(child.bbox_y.value() + child.bbox_height.value()
                  <= parent.bbox_y.value() + parent.bbox_height.value() + eps);

            check_containment(child);
        }
    };

    check_containment(elements.tree);
}

TEST_CASE("Glyphs are within their node bounding box") {
    auto elements = mfl::layout(R"(\sum_{i=0}^n x_i)", mfl::points{12.0}, create_font_face);

    std::function<void(const mfl::formula_node&)> check;
    check = [&](const mfl::formula_node& node) {
        for (auto idx : node.glyph_indices) {
            const auto& g = elements.glyphs[idx];
            constexpr double eps = 2.0;  // 2 pt tolerance
            CHECK(g.x.value() >= node.bbox_x.value() - eps);
            CHECK(g.x.value() <= node.bbox_x.value() + node.bbox_width.value() + eps);
        }
        for (const auto& child : node.children) check(child);
    };

    check(elements.tree);
}
```

**Критерий прохождения:** Все bbox имеют положительные размеры, дети внутри родителей, глифы внутри своих узлов.

### Тест 3.5: Отладочный вывод дерева (print_tree)

```cpp
std::string node_type_name(mfl::formula_node_type type) {
    switch (type) {
        case mfl::formula_node_type::root: return "root";
        case mfl::formula_node_type::symbol: return "symbol";
        case mfl::formula_node_type::fraction: return "fraction";
        case mfl::formula_node_type::numerator: return "numerator";
        case mfl::formula_node_type::denominator: return "denominator";
        case mfl::formula_node_type::radical: return "radical";
        case mfl::formula_node_type::radicand: return "radicand";
        case mfl::formula_node_type::degree: return "degree";
        case mfl::formula_node_type::superscript: return "superscript";
        case mfl::formula_node_type::subscript: return "subscript";
        case mfl::formula_node_type::script_nucleus: return "script_nucleus";
        case mfl::formula_node_type::accent: return "accent";
        case mfl::formula_node_type::overline: return "overline";
        case mfl::formula_node_type::underline: return "underline";
        case mfl::formula_node_type::left_right: return "left_right";
        case mfl::formula_node_type::big_op: return "big_op";
        case mfl::formula_node_type::matrix: return "matrix";
        case mfl::formula_node_type::matrix_cell: return "matrix_cell";
        case mfl::formula_node_type::group: return "group";
        case mfl::formula_node_type::space: return "space";
        case mfl::formula_node_type::function_name: return "function_name";
        default: return "unknown";
    }
}

std::string print_tree(const mfl::formula_node& node, int indent = 0) {
    std::ostringstream os;
    std::string pad(indent * 2, ' ');

    os << pad << node_type_name(node.type);
    if (node.char_code != 0 && node.char_code < 128) {
        os << " '" << static_cast<char>(node.char_code) << "'";
    } else if (node.char_code != 0) {
        os << " U+" << std::hex << node.char_code << std::dec;
    }
    os << " [" << node.bbox_x.value() << "," << node.bbox_y.value()
       << " " << node.bbox_width.value() << "x" << node.bbox_height.value() << "]";
    os << " g:" << node.glyph_indices.size() << " l:" << node.line_indices.size();
    os << "\n";

    for (const auto& child : node.children) {
        os << print_tree(child, indent + 1);
    }
    return os.str();
}

TEST_CASE("print_tree produces readable output") {
    auto elements = mfl::layout(R"(\frac{x^2}{y})", mfl::points{12.0}, create_font_face);
    std::string tree_str = print_tree(elements.tree);

    // Вывести для визуальной проверки
    std::cout << "\nTree for \\frac{x^2}{y}:\n" << tree_str << std::endl;

    // Ожидаемая структура (примерно):
    // root [...] g:0 l:0
    //   fraction [...] g:0 l:1
    //     numerator [...] g:0 l:0
    //       script_nucleus [...] g:0 l:0
    //         symbol 'x' [...] g:1 l:0
    //       superscript [...] g:0 l:0
    //         symbol '2' [...] g:1 l:0
    //     denominator [...] g:0 l:0
    //       symbol 'y' [...] g:1 l:0

    CHECK(tree_str.find("fraction") != std::string::npos);
    CHECK(tree_str.find("numerator") != std::string::npos);
    CHECK(tree_str.find("denominator") != std::string::npos);
    CHECK(tree_str.find("superscript") != std::string::npos);
}
```

### Тест 3.6: Регрессионные тесты на большом наборе формул

```cpp
TEST_CASE("Tree is valid for all approval test formulas") {
    std::vector<std::string> formulas = {
        R"(M(s)<M(t)<|M| = m)",
        R"(a = \sqrt[2]{b^2 + c^2})",
        R"(A^{x_i^2}_{j^{2n}_{n,m}})",
        R"(\lim_{x \rightarrow 0} \frac{\ln \sin \pi x}{\ln \sin x})",
        R"(\left[ \int + \int \right]_{x=0}^{x=1})",
        R"(\sqrt{1+\sqrt{1+\sqrt{1+\sqrt{1+x}}}})",
        R"({a}_{0}+\frac{1}{{a}_{1}+\frac{1}{{a}_{2}+\frac{1}{{a}_{3}}}}}})",
        R"(\widehat{abc}\widetilde{def})",
        R"(\alpha \beta \gamma \delta \epsilon)",
        R"(e^{i\pi} + 1 = 0)",
    };

    for (const auto& formula : formulas) {
        CAPTURE(formula);
        auto elements = mfl::layout(formula, mfl::points{12.0}, create_font_face);

        // Не должно быть ошибок
        CHECK(!elements.error.has_value());

        // Дерево не пустое
        CHECK(elements.tree.type == mfl::formula_node_type::root);

        // Все глифы учтены
        std::set<size_t> indices;
        collect_glyph_indices(elements.tree, indices);
        CHECK(indices.size() == elements.glyphs.size());

        // Все линии учтены
        std::set<size_t> line_idx;
        collect_line_indices(elements.tree, line_idx);
        CHECK(line_idx.size() == elements.lines.size());

        // Нет дублирования
        std::map<size_t, int> glyph_count;
        count_glyph_ownership(elements.tree, glyph_count);
        for (const auto& [idx, count] : glyph_count) {
            CHECK(count == 1);
        }
    }
}
```

---

## Часть 3: Интеграционные тесты (пункты 2+3 вместе)

### Тест I.1: Debug-визуализация bounding box'ов

```cpp
TEST_CASE("Debug bounding boxes render without crash") {
    FormulaWidget widget;
    widget.resize(600, 200);
    widget.setFormula(R"(\frac{x^2 + 1}{\sqrt{y}})");
    widget.setDebugDrawBBoxes(true);

    QImage image(600, 200, QImage::Format_ARGB32);
    image.fill(Qt::white);
    widget.render(&image);

    // Должны быть цветные прямоугольники (не только чёрный текст)
    bool has_colored_pixel = false;
    for (int y = 0; y < image.height() && !has_colored_pixel; ++y) {
        for (int x = 0; x < image.width() && !has_colored_pixel; ++x) {
            QColor c = image.pixelColor(x, y);
            if (c != QColor(Qt::white) && c != QColor(Qt::black)) {
                has_colored_pixel = true;
            }
        }
    }
    CHECK(has_colored_pixel);  // bbox'ы нарисованы цветом

    // Сохранить для визуальной проверки
    image.save("test_output/debug_bboxes.png");
}
```

### Тест I.2: Подготовка к hit-testing (пункт 4)

```cpp
const mfl::formula_node* hit_test(const mfl::formula_node& node,
                                   mfl::points x, mfl::points y) {
    if (x.value() < node.bbox_x.value() ||
        x.value() > node.bbox_x.value() + node.bbox_width.value() ||
        y.value() < node.bbox_y.value() ||
        y.value() > node.bbox_y.value() + node.bbox_height.value()) {
        return nullptr;
    }

    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
        if (auto* found = hit_test(*it, x, y)) return found;
    }

    return &node;
}

TEST_CASE("Hit-test finds correct element in fraction") {
    auto elements = mfl::layout(R"(\frac{a}{b})", mfl::points{12.0}, create_font_face);
    const auto& tree = elements.tree;

    // Найти числитель и знаменатель
    auto* fraction = find_descendant(tree, mfl::formula_node_type::fraction);
    REQUIRE(fraction != nullptr);
    auto* numerator = find_child(*fraction, mfl::formula_node_type::numerator);
    auto* denominator = find_child(*fraction, mfl::formula_node_type::denominator);
    REQUIRE(numerator != nullptr);
    REQUIRE(denominator != nullptr);

    // Кликнуть в центр числителя
    mfl::points num_cx{numerator->bbox_x.value() + numerator->bbox_width.value() / 2};
    mfl::points num_cy{numerator->bbox_y.value() + numerator->bbox_height.value() / 2};

    auto* hit = hit_test(tree, num_cx, num_cy);
    REQUIRE(hit != nullptr);

    // Должны попасть в числитель или его потомка
    CHECK((hit->type == mfl::formula_node_type::numerator ||
           hit->type == mfl::formula_node_type::symbol));

    // Кликнуть в центр знаменателя
    mfl::points den_cx{denominator->bbox_x.value() + denominator->bbox_width.value() / 2};
    mfl::points den_cy{denominator->bbox_y.value() + denominator->bbox_height.value() / 2};

    hit = hit_test(tree, den_cx, den_cy);
    REQUIRE(hit != nullptr);
    CHECK((hit->type == mfl::formula_node_type::denominator ||
           hit->type == mfl::formula_node_type::symbol));
}

TEST_CASE("Hit-test returns nullptr outside formula") {
    auto elements = mfl::layout(R"(x)", mfl::points{12.0}, create_font_face);

    // Кликнуть далеко за пределами формулы
    auto* hit = hit_test(elements.tree, mfl::points{1000.0}, mfl::points{1000.0});
    CHECK(hit == nullptr);
}
```

---

## Часть 4: Чек-лист валидации

### Для пункта 2 (виджет) — что проверить перед сдачей:

- [ ] **Сборка:** Проект компилируется без ошибок и предупреждений
- [ ] **Шрифты:** STIX2 шрифты загружаются, FtFontFace возвращает ненулевые метрики
- [ ] **Простые формулы:** `x`, `a+b`, `x^2` рисуются корректно
- [ ] **Дроби:** `\frac{a}{b}` — числитель сверху, знаменатель снизу, черта между ними
- [ ] **Корни:** `\sqrt{x}` — знак корня правильной формы, черта сверху
- [ ] **Индексы:** `x_i^2` — нижний индекс ниже, верхний выше, оба меньшего размера
- [ ] **Большие операторы:** `\sum_{i=0}^n` — символ суммы большой, пределы сверху/снизу
- [ ] **Скобки:** `\left( \frac{a}{b} \right)` — скобки масштабируются по высоте дроби
- [ ] **Греческие буквы:** `\alpha, \beta, \gamma` — отображаются правильно
- [ ] **Вложенность:** `\frac{1}{\frac{2}{3}}` — вложенные дроби корректны
- [ ] **Координаты:** Y-ось не перевёрнута, формула не зеркальная
- [ ] **Масштаб:** Формула не слишком маленькая/большая при разных DPI
- [ ] **Ошибки:** Некорректная формула не крашит приложение

### Для пункта 3 (дерево) — что проверить перед сдачей:

- [ ] **Полнота:** Все глифы и линии из layout_elements присутствуют в дереве
- [ ] **Уникальность:** Каждый глиф/линия принадлежит ровно одному узлу
- [ ] **Типы:** Для `\frac{a}{b}` есть fraction → numerator + denominator
- [ ] **Типы:** Для `x^2` есть script_nucleus + superscript
- [ ] **Типы:** Для `\sqrt{x}` есть radical → radicand
- [ ] **Типы:** Для `\sqrt[3]{x}` есть radical → radicand + degree
- [ ] **Типы:** Для `\overline{x}` есть overline
- [ ] **Типы:** Для `\sum_{i=0}^n` есть big_op
- [ ] **Типы:** Для `\left(...\right)` есть left_right
- [ ] **Bbox:** Все bounding box'ы имеют положительные размеры
- [ ] **Bbox:** Дети содержатся внутри родителей
- [ ] **Bbox:** Глифы находятся внутри bbox своего узла
- [ ] **Debug:** print_tree выводит читаемое дерево
- [ ] **Debug:** Визуализация bbox в виджете показывает корректные прямоугольники
- [ ] **Регрессия:** Все формулы из approval tests mfl проходят без ошибок
- [ ] **Существующие тесты mfl:** Модификация mfl не ломает существующие unit/approval тесты

---

## Резюме

**Пункт 2** — это в основном инженерная задача: реализовать `abstract_font_face` через FreeType/HarfBuzz и написать Qt-виджет, который рисует `shaped_glyph` и `line` из `layout_elements`. Основная сложность — правильная конвертация координат и рендеринг глифов.

**Пункт 3** — это архитектурная задача: нужно модифицировать внутренности mfl, чтобы при flatten box-дерева в плоский список параллельно строилось иерархическое дерево `formula_node` с bounding box'ами. Основная сложность — правильно проставить аннотации во всех noad-обработчиках.

Оба пункта вместе дают основу для пункта 4 (hit-testing по координатам) — это будет простой рекурсивный поиск по дереву `formula_node`.
