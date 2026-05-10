# Детальный план реализации пунктов 2 и 3

## Контекст: Как устроена библиотека mfl

### Архитектура mfl (что уже есть)

Библиотека `mfl` — это **чистый layout-движок** для математических формул. Она работает в 4 этапа:

1. **Парсинг** (`parse.cpp`) — TeX-строка → вектор `noad` (абстрактное дерево формулы)
2. **Обработка** — добавление неявных пробелов между операторами
3. **Layout** (`layout.cpp`) — `noad`-дерево → `box`-дерево (с размерами и позициями)
4. **Flatten** (`layout.cpp`) — `box`-дерево → плоский список `shaped_glyph` + `line`

### Ключевые структуры данных

**Публичный API** (единственная функция):
```cpp
// include/mfl/layout.hpp
layout_elements layout(string_view input, points font_size, font_face_creator& create_font_face);
```

**Выход — плоский список** (без иерархии!):
```cpp
struct layout_elements {
    points width, height;
    vector<shaped_glyph> glyphs;  // глифы с абсолютными координатами (x, y)
    vector<line> lines;            // линии (дроби, корни, надчёркивания)
    optional<string> error;
};
```

**Внутреннее дерево noad** (семантическое, НЕ экспортируется):
```
noad = variant<math_char, radical, accent, vcenter, overline, underline,
               fraction, left_right, script, big_op, matrix, math_space, mlist, mlist_with_kind>
```

**Внутреннее дерево node/box** (layout-дерево, НЕ экспортируется):
```
node_variant = variant<glyph, kern, rule, glue_spec, wrapped_box>
box { box_kind, box_dims, shift, vector<node_variant>, glue_param }
```

### Проблема

`mfl::layout()` возвращает **плоский** список глифов и линий. Иерархическая информация (какой глиф — числитель дроби, какой — степень, какой — подкоренное выражение) **теряется** при flatten в `layout.cpp`. Для пунктов 2-4 ТЗ нужно эту иерархию **сохранить**.

---

## ПУНКТ 2: Свой холст/виджет с рисованием от mfl

### Цель
Создать Qt-виджет (QWidget), который:
- Принимает TeX-строку
- Вызывает `mfl::layout()` для получения layout_elements
- Рисует формулу на экране через QPainter
- Является основой для дальнейшей интерактивности (hit-testing, выделение)

### Подзадача 2.1: Реализация `abstract_font_face` через FreeType

**Файлы для создания:**
- `UIR/formula_widget/src/ft_font_face.hpp`
- `UIR/formula_widget/src/ft_font_face.cpp`
- `UIR/formula_widget/src/ft_library.hpp`
- `UIR/formula_widget/src/ft_library.cpp`

**Что делать:**

1. Создать класс `FtLibrary` — RAII-обёртка над `FT_Library`:
   ```cpp
   class FtLibrary {
   public:
       FtLibrary();   // FT_Init_FreeType
       ~FtLibrary();  // FT_Done_FreeType
       FT_Face load_face(font_family family) const;
       // Загружает STIX2 шрифты по family:
       //   roman  → STIXTwoMath-Regular.otf
       //   italic → STIXTwoMath-Regular.otf (с italic flag)
       //   bold   → STIXTwoText-Bold.otf
       //   mono   → DejaVuSansMono.ttf (или другой моноширинный)
       //   sans   → DejaVuSans.ttf
   private:
       FT_Library lib_;
       std::string fonts_dir_;  // путь к директории со шрифтами
   };
   ```

2. Создать класс `FtFontFace : public mfl::abstract_font_face`:
   ```cpp
   class FtFontFace : public mfl::abstract_font_face {
   public:
       FtFontFace(mfl::font_family family, const FtLibrary& lib);

       math_constants constants() const override;
       // Читает MATH table через HarfBuzz: hb_ot_math_get_constant()

       math_glyph_info glyph_info(size_t glyph_index) const override;
       // FT_Load_Glyph → width, height, depth, italic_correction
       // Все размеры в 26.6 fixed point (64ths of pixel)

       size_t glyph_index_from_code_point(code_point cp, bool large) const override;
       // FT_Get_Char_Index или hb_font_get_glyph для large variants

       vector<size_variant> vertical_size_variants(code_point cp) const override;
       vector<size_variant> horizontal_size_variants(code_point cp) const override;
       // hb_ot_math_get_glyph_variants()

       optional<glyph_assembly> vertical_assembly(code_point cp) const override;
       optional<glyph_assembly> horizontal_assembly(code_point cp) const override;
       // hb_ot_math_get_glyph_assembly()

       void set_size(points size) override;
       // FT_Set_Char_Size

   private:
       FT_Face ft_face_;
       hb_font_t* hb_font_;  // HarfBuzz font для MATH table
   };
   ```

**Важно:** Можно взять за основу реализацию из `tests/fonts_for_tests/` — там уже есть рабочий `font_face` через FreeType. Нужно скопировать и адаптировать.

**Зависимости:** FreeType, HarfBuzz, шрифты STIX2 (скачать с https://github.com/stipub/stixfonts).

### Подзадача 2.2: Qt-виджет для рисования формулы

**Файлы для создания:**
- `UIR/formula_widget/src/formula_widget.hpp`
- `UIR/formula_widget/src/formula_widget.cpp`

**Что делать:**

1. Создать класс `FormulaWidget : public QWidget`:
   ```cpp
   class FormulaWidget : public QWidget {
       Q_OBJECT
   public:
       explicit FormulaWidget(QWidget* parent = nullptr);

       void setFormula(const QString& tex);  // задать формулу
       void setFontSize(double pt);          // размер шрифта в points

       // Доступ к результату layout (для пункта 3)
       const mfl::layout_elements& layoutElements() const;

   signals:
       void formulaChanged();

   protected:
       void paintEvent(QPaintEvent* event) override;
       void resizeEvent(QResizeEvent* event) override;

   private:
       void recalculateLayout();  // пересчитать layout при изменении формулы

       // Рисование отдельных элементов
       void renderGlyph(QPainter& painter, const mfl::shaped_glyph& g);
       void renderLine(QPainter& painter, const mfl::line& l);

       QString tex_formula_;
       double font_size_pt_ = 12.0;
       double dpi_ = 96.0;
       mfl::layout_elements layout_;

       // FreeType/HarfBuzz ресурсы
       std::unique_ptr<FtLibrary> ft_lib_;
   };
   ```

2. Реализация `paintEvent`:
   ```cpp
   void FormulaWidget::paintEvent(QPaintEvent*) {
       QPainter painter(this);
       painter.setRenderHint(QPainter::Antialiasing);

       if (layout_.error) {
           painter.drawText(rect(), Qt::AlignCenter,
                           QString::fromStdString(*layout_.error));
           return;
       }

       // Координаты mfl: начало координат внизу-слева, Y вверх
       // Координаты Qt: начало координат вверху-слева, Y вниз
       // Нужна трансформация!

       const double scale = dpi_ / 72.0;  // points → pixels

       for (const auto& g : layout_.glyphs) {
           renderGlyph(painter, g);
       }
       for (const auto& l : layout_.lines) {
           renderLine(painter, l);
       }
   }
   ```

3. Реализация `renderGlyph` — ключевой метод:
   ```cpp
   void FormulaWidget::renderGlyph(QPainter& painter, const mfl::shaped_glyph& g) {
       // Вариант A: Через QRawFont (рекомендуется)
       QRawFont rawFont = getRawFont(g.family, g.size);

       // Конвертация координат mfl (points, Y-up) → Qt (pixels, Y-down)
       double px = pointsToPixels(g.x);
       double py = height() - pointsToPixels(g.y);  // инверсия Y

       // Получить QPainterPath для глифа по индексу
       QVector<quint32> glyphIndexes = { static_cast<quint32>(g.index) };
       QVector<QPointF> positions = { QPointF(px, py) };

       // Рисуем глиф
       QPainterPath path = rawFont.pathForGlyph(g.index);
       painter.save();
       painter.translate(px, py);
       double scaleFactor = pointsToPixels(g.size) / rawFont.unitsPerEm();
       painter.scale(scaleFactor, -scaleFactor);  // -Y для инверсии
       painter.fillPath(path, painter.pen().color());
       painter.restore();

       // Вариант B: Через FreeType рендеринг в QImage
       // FT_Load_Glyph → FT_Render_Glyph → bitmap → QImage → drawImage
       // Менее качественно, но проще
   }
   ```

4. Реализация `renderLine`:
   ```cpp
   void FormulaWidget::renderLine(QPainter& painter, const mfl::line& l) {
       double x = pointsToPixels(l.x);
       double y = height() - pointsToPixels(l.y);
       double w = pointsToPixels(l.length);
       double h = pointsToPixels(l.thickness);

       painter.fillRect(QRectF(x, y - h, w, h), painter.pen().color());
   }
   ```

### Подзадача 2.3: Система координат и масштабирование

**Критически важно понять систему координат mfl:**

- mfl работает в **points** (1 point = 1/72 дюйма)
- Внутренние расстояния в `dist_t` = 26.6 fixed point (×65536)
- Координаты глифов в `layout_elements` уже в **points** (конвертированы из dist_t)
- Начало координат — **левый нижний** угол формулы
- Y растёт **вверх**

**Конвертация в пиксели Qt:**
```cpp
double pointsToPixels(mfl::points pt) const {
    return pt.value() * dpi_ / 72.0;
}

// Конвертация позиции mfl → Qt
QPointF mflToQt(mfl::points x, mfl::points y) const {
    return QPointF(
        pointsToPixels(x) + margin_left_,
        widget_height_ - pointsToPixels(y) - margin_bottom_  // инверсия Y
    );
}
```

### Подзадача 2.4: CMake и структура проекта

**Файлы:**
- `UIR/formula_widget/CMakeLists.txt`
- `UIR/formula_widget/src/main.cpp` (демо-приложение)

```cmake
cmake_minimum_required(VERSION 3.16)
project(formula_widget)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Freetype REQUIRED)
find_package(harfbuzz REQUIRED)

# mfl как подпроект
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../mfl ${CMAKE_CURRENT_BINARY_DIR}/mfl)

add_executable(formula_widget
    src/main.cpp
    src/formula_widget.cpp
    src/ft_font_face.cpp
    src/ft_library.cpp
)

target_link_libraries(formula_widget PRIVATE
    mfl::mfl
    Qt6::Widgets
    Freetype::Freetype
    harfbuzz::harfbuzz
)
```

### Подзадача 2.5: Демо-приложение

```cpp
// main.cpp
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    auto* widget = new FormulaWidget(&window);
    widget->setFormula(R"(\frac{1}{2\pi i} \int_\gamma \frac{f(z)}{z-a} \, dz)");
    widget->setFontSize(20.0);

    window.setCentralWidget(widget);
    window.resize(800, 200);
    window.show();

    return app.exec();
}
```

---

## ПУНКТ 3: Построение дерева элементов формулы

### Цель
Построить иерархическое дерево элементов формулы, где каждый узел знает:
- Свой тип (символ, дробь, корень, степень, индекс, скобки, матрица...)
- Свой bounding box (прямоугольник в координатах виджета)
- Своих детей
- Какие `shaped_glyph` и `line` из `layout_elements` ему принадлежат

### Проблема и два подхода

**Проблема:** `mfl::layout()` возвращает плоский список — иерархия теряется.

**Подход A (рекомендуемый): Модификация mfl::layout() — добавить аннотации**

Модифицировать `layout.cpp` так, чтобы при flatten каждый глиф/линия получал ID своего родительского noad-узла. Это самый чистый подход.

**Подход B (без модификации mfl): Параллельный обход noad-дерева**

Вызвать `parse()` отдельно, получить noad-дерево, и параллельно с layout восстановить соответствие. Сложнее, но не требует изменения mfl.

**Подход C (самый простой, но грубый): Эвристическое восстановление**

По координатам и размерам глифов эвристически восстановить иерархию. Ненадёжно.

### Рекомендуемая реализация: Подход A (модификация mfl)

#### Шаг 3.1: Расширить `layout_elements` — добавить дерево

**Файл:** `include/mfl/layout.hpp` (модификация)

```cpp
namespace mfl {
    // Тип узла формулы
    enum class formula_node_type {
        root,           // корневой узел всей формулы
        symbol,         // одиночный символ (буква, цифра, оператор)
        fraction,       // дробь (\frac)
        numerator,      // числитель дроби
        denominator,    // знаменатель дроби
        radical,        // корень (\sqrt)
        radicand,       // подкоренное выражение
        degree,         // степень корня
        superscript,    // верхний индекс (^)
        subscript,      // нижний индекс (_)
        script_nucleus, // ядро при скрипте
        accent,         // акцент (\hat, \tilde, ...)
        overline,       // надчёркивание
        underline,      // подчёркивание
        left_right,     // \left...\right
        big_op,         // большой оператор (\sum, \int, ...)
        matrix,         // матрица
        matrix_cell,    // ячейка матрицы
        group,          // группа {}, mlist
        space,          // пробел
        function_name,  // имя функции (\sin, \cos, ...)
    };

    // Узел дерева формулы
    struct formula_node {
        formula_node_type type = formula_node_type::root;

        // Bounding box в points (координаты mfl)
        points bbox_x{0};       // левый край
        points bbox_y{0};       // нижний край
        points bbox_width{0};   // ширина
        points bbox_height{0};  // высота

        // Индексы в layout_elements.glyphs и .lines
        std::vector<std::size_t> glyph_indices;
        std::vector<std::size_t> line_indices;

        // Дочерние узлы
        std::vector<formula_node> children;

        // Опциональная семантическая информация
        std::string source_tex;     // исходный TeX-фрагмент (если доступен)
        code_point char_code = 0;   // для символов — Unicode code point
    };

    struct layout_elements {
        points width;
        points height;
        std::vector<shaped_glyph> glyphs;
        std::vector<line> lines;
        std::optional<std::string> error;

        // НОВОЕ: дерево элементов
        formula_node tree;
    };
}
```

#### Шаг 3.2: Модифицировать `layout.cpp` — строить дерево при flatten

**Файл:** `src/layout.cpp` (модификация)

Ключевая идея: при обходе box-дерева в `layout_box`/`layout_hbox`/`layout_vbox` параллельно строить `formula_node`-дерево.

```cpp
namespace mfl {
namespace {
    // Модифицированные функции layout — теперь принимают formula_node&

    void layout_glyph(const glyph& g, points x, points y,
                      layout_elements& elements, formula_node& parent) {
        size_t idx = elements.glyphs.size();
        elements.glyphs.push_back({g.family, g.index, g.size, x, y});
        parent.glyph_indices.push_back(idx);
    }

    void layout_rule(const rule& r, points x, points y,
                     layout_elements& elements, formula_node& parent) {
        size_t idx = elements.lines.size();
        auto thickness = dist_to_points(r.depth + r.height);
        auto min_y = y - dist_to_points(r.depth);
        auto length = dist_to_points(r.width);
        elements.lines.push_back({x, min_y, length, thickness});
        parent.line_indices.push_back(idx);
    }

    void layout_box(const box& b, points x, points y,
                    layout_elements& elements, formula_node& parent);

    void layout_node(const node_variant& n, points x, points y,
                     layout_elements& elements, formula_node& parent) {
        std::visit(overload{
            [&](const box& b) { layout_box(b, x, y, elements, parent); },
            [](const glue_spec&) {},
            [&](const glyph& g) { layout_glyph(g, x, y, elements, parent); },
            [](const kern&) {},
            [&](const rule& r) { layout_rule(r, x, y, elements, parent); }
        }, n);
    }

    void layout_hbox(const box& b, points x, points y,
                     layout_elements& elements, formula_node& parent) {
        if (!b.nodes.empty()) {
            auto cur_x = x;
            for (const auto& n : b.nodes) {
                layout_node(n, cur_x, y - shift(n), elements, parent);
                cur_x += dist_to_points(scaled_width(n, b.glue.scale));
            }
        }
    }

    // Аналогично layout_vbox...
}
}
```

#### Шаг 3.3: Связать noad-типы с formula_node_type

**Проблема:** При переходе noad → box → layout_elements теряется информация о типе noad. Нужно **пробросить** тип noad через box-дерево.

**Решение:** Добавить аннотацию в `box`:

**Файл:** `src/node/box.hpp` (модификация)

```cpp
struct box {
    box_kind kind = box_kind::hbox;
    box_dims dims;
    dist_t shift = 0;
    std::vector<node_variant> nodes;
    glue_param glue;

    // НОВОЕ: аннотация для дерева элементов
    formula_node_type annotation = formula_node_type::root;
};
```

Затем в каждой функции `*_to_hlist` (например, `fraction_to_hlist`, `script_to_hlist`, `radical_to_hlist`) при создании box'ов проставлять аннотацию:

```cpp
// В fraction_to_hlist:
auto num_box = clean_box(s, cramping::on, f.numerator);
num_box.annotation = formula_node_type::numerator;

auto den_box = clean_box(s, cramping::on, f.denominator);
den_box.annotation = formula_node_type::denominator;

auto result_box = make_vbox(...);
result_box.annotation = formula_node_type::fraction;
```

Затем в `layout_box` при обходе:
```cpp
void layout_box(const box& b, points x, points y,
                layout_elements& elements, formula_node& parent) {
    // Создать дочерний узел если есть аннотация
    formula_node* target = &parent;
    if (b.annotation != formula_node_type::root) {
        parent.children.push_back({.type = b.annotation});
        target = &parent.children.back();
    }

    if (b.kind == box_kind::hbox)
        layout_hbox(b, x, y, elements, *target);
    else
        layout_vbox(b, x, y, elements, *target);

    // Вычислить bounding box узла
    target->bbox_x = x;
    target->bbox_y = y - dist_to_points(b.dims.depth);
    target->bbox_width = dist_to_points(b.dims.width);
    target->bbox_height = dist_to_points(b.dims.height + b.dims.depth);
}
```

#### Шаг 3.4: Список файлов mfl для модификации

| Файл | Что менять |
|-------|-----------|
| `include/mfl/layout.hpp` | Добавить `formula_node_type`, `formula_node`, поле `tree` в `layout_elements` |
| `src/node/box.hpp` | Добавить поле `annotation` в `struct box` |
| `src/layout.cpp` | Модифицировать все `layout_*` функции для построения дерева |
| `src/noad/fraction.cpp` | Проставить `annotation` при создании box'ов |
| `src/noad/script.cpp` | Проставить `annotation` для nucleus/sub/sup |
| `src/noad/radical.cpp` | Проставить `annotation` для radicand/degree |
| `src/noad/accent.cpp` | Проставить `annotation` |
| `src/noad/overline.cpp` | Проставить `annotation` |
| `src/noad/underline.cpp` | Проставить `annotation` |
| `src/noad/left_right.cpp` | Проставить `annotation` |
| `src/noad/big_op.cpp` | Проставить `annotation` |
| `src/noad/matrix.cpp` | Проставить `annotation` для matrix/cell |
| `src/noad/math_char.cpp` | Проставить `annotation` для symbol |

#### Шаг 3.5: Вычисление bounding box для каждого узла

После построения дерева нужно рекурсивно вычислить bounding box каждого узла:

```cpp
void compute_bounding_boxes(formula_node& node, const layout_elements& elements) {
    if (node.children.empty() && node.glyph_indices.empty() && node.line_indices.empty())
        return;

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();

    // Учесть глифы
    for (auto idx : node.glyph_indices) {
        const auto& g = elements.glyphs[idx];
        min_x = std::min(min_x, g.x.value());
        min_y = std::min(min_y, g.y.value() - g.size.value()); // приблизительно
        max_x = std::max(max_x, g.x.value() + /* ширина глифа */);
        max_y = std::max(max_y, g.y.value());
    }

    // Учесть линии
    for (auto idx : node.line_indices) {
        const auto& l = elements.lines[idx];
        min_x = std::min(min_x, l.x.value());
        min_y = std::min(min_y, l.y.value());
        max_x = std::max(max_x, l.x.value() + l.length.value());
        max_y = std::max(max_y, l.y.value() + l.thickness.value());
    }

    // Рекурсивно для детей
    for (auto& child : node.children) {
        compute_bounding_boxes(child, elements);
        min_x = std::min(min_x, child.bbox_x.value());
        min_y = std::min(min_y, child.bbox_y.value());
        max_x = std::max(max_x, child.bbox_x.value() + child.bbox_width.value());
        max_y = std::max(max_y, child.bbox_y.value() + child.bbox_height.value());
    }

    node.bbox_x = points{min_x};
    node.bbox_y = points{min_y};
    node.bbox_width = points{max_x - min_x};
    node.bbox_height = points{max_y - min_y};
}
```

**Лучший вариант:** Использовать `box_dims` из box-дерева напрямую (они уже точно вычислены mfl), а не пересчитывать по глифам.

---

## Порядок реализации (пошаговый)

### Фаза 1: Минимальный рабочий виджет (Пункт 2, MVP)

**Шаг 1.1** — Скопировать `tests/fonts_for_tests/` как основу для FtFontFace
- Скопировать `font_face.hpp`, `font_face.cpp`, `freetype.hpp`, `freetype.cpp`
- Адаптировать пути к шрифтам (STIX2)
- Убедиться что компилируется отдельно от тестов

**Шаг 1.2** — Создать минимальный Qt-проект
- `CMakeLists.txt` с Qt6, FreeType, HarfBuzz, mfl
- `main.cpp` с QApplication + QMainWindow

**Шаг 1.3** — Реализовать `FormulaWidget::paintEvent`
- Вызвать `mfl::layout()` с TeX-строкой
- Нарисовать глифы через `QRawFont::pathForGlyph()` или FreeType bitmap
- Нарисовать линии через `QPainter::fillRect()`
- Правильно конвертировать координаты (Y-инверсия!)

**Шаг 1.4** — Тестирование
- Отрисовать несколько формул: `\frac{a}{b}`, `x^2 + y^2`, `\sqrt{x}`, `\sum_{i=0}^n x_i`
- Сравнить визуально с SVG из approval tests

### Фаза 2: Дерево элементов (Пункт 3)

**Шаг 2.1** — Добавить `formula_node_type` и `formula_node` в `layout.hpp`

**Шаг 2.2** — Добавить поле `annotation` в `struct box` (`box.hpp`)

**Шаг 2.3** — Модифицировать `layout.cpp`:
- Добавить параметр `formula_node&` во все `layout_*` функции
- При обходе box-дерева создавать дочерние `formula_node` по аннотации
- Записывать индексы глифов/линий в соответствующие узлы

**Шаг 2.4** — Проставить аннотации в noad-обработчиках:
- `fraction.cpp` → `fraction`, `numerator`, `denominator`
- `script.cpp` → `superscript`, `subscript`, `script_nucleus`
- `radical.cpp` → `radical`, `radicand`, `degree`
- `accent.cpp` → `accent`
- `overline.cpp` → `overline`
- `underline.cpp` → `underline`
- `left_right.cpp` → `left_right`
- `big_op.cpp` → `big_op`
- `matrix.cpp` → `matrix`, `matrix_cell`
- `math_char.cpp` → `symbol`

**Шаг 2.5** — Вычислить bounding box'ы:
- Использовать `box_dims` из box-дерева (точные значения)
- Или рекурсивно по глифам/линиям/детям (запасной вариант)

**Шаг 2.6** — Тестирование дерева:
- Для формулы `\frac{a}{b}` проверить что дерево содержит:
  ```
  root
  └── fraction
      ├── numerator
      │   └── symbol (a)
      └── denominator
          └── symbol (b)
  ```
- Для `x^2` проверить:
  ```
  root
  └── script
      ├── script_nucleus
      │   └── symbol (x)
      └── superscript
          └── symbol (2)
  ```
- Написать функцию `print_tree()` для отладки

### Фаза 3: Интеграция дерева в виджет

**Шаг 3.1** — Добавить в `FormulaWidget` хранение дерева:
```cpp
class FormulaWidget : public QWidget {
    // ...
    const mfl::formula_node& formulaTree() const { return layout_.tree; }

    // Для отладки: нарисовать bounding box'ы поверх формулы
    void setDebugDrawBBoxes(bool enable);

protected:
    void paintEvent(QPaintEvent*) override {
        // ... рисование формулы ...

        if (debug_draw_bboxes_) {
            drawBoundingBoxes(painter, layout_.tree);
        }
    }

private:
    void drawBoundingBoxes(QPainter& painter, const mfl::formula_node& node) {
        // Рекурсивно нарисовать полупрозрачные прямоугольники
        QColor color = colorForNodeType(node.type);
        color.setAlpha(40);

        QRectF rect = mflBBoxToQt(node);
        painter.fillRect(rect, color);
        painter.setPen(QPen(color.darker(), 1));
        painter.drawRect(rect);

        for (const auto& child : node.children) {
            drawBoundingBoxes(painter, child);
        }
    }
};
```

**Шаг 3.2** — Подготовка к пункту 4 (hit-testing):
```cpp
// Найти узел дерева по координатам (для будущего пункта 4)
const mfl::formula_node* hitTest(const mfl::formula_node& node,
                                  mfl::points x, mfl::points y) {
    // Проверить попадание в bounding box
    if (x.value() < node.bbox_x.value() ||
        x.value() > node.bbox_x.value() + node.bbox_width.value() ||
        y.value() < node.bbox_y.value() ||
        y.value() > node.bbox_y.value() + node.bbox_height.value()) {
        return nullptr;
    }

    // Рекурсивно проверить детей (от последнего к первому — верхние слои)
    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
        if (auto* found = hitTest(*it, x, y)) {
            return found;
        }
    }

    return &node;  // попали в этот узел, но не в детей
}
```

---

## Структура файлов проекта

```
UIR/
├── mfl/                          # Библиотека mfl (модифицированная)
│   ├── include/mfl/
│   │   └── layout.hpp            # + formula_node_type, formula_node
│   ├── src/
│   │   ├── layout.cpp            # Модифицирован для построения дерева
│   │   ├── node/
│   │   │   └── box.hpp           # + поле annotation
│   │   └── noad/
│   │       ├── fraction.cpp      # + аннотации
│   │       ├── script.cpp        # + аннотации
│   │       ├── radical.cpp       # + аннотации
│   │       └── ...               # + аннотации
│   └── ...
│
├── formula_widget/               # Новый проект — Qt-виджет
│   ├── CMakeLists.txt
│   ├── fonts/                    # Шрифты STIX2
│   │   ├── STIXTwoMath-Regular.otf
│   │   └── ...
│   └── src/
│       ├── main.cpp              # Демо-приложение
│       ├── formula_widget.hpp    # Qt-виджет
│       ├── formula_widget.cpp
│       ├── ft_library.hpp        # RAII обёртка FreeType
│       ├── ft_library.cpp
│       ├── ft_font_face.hpp      # Реализация abstract_font_face
│       └── ft_font_face.cpp
│
└── PLAN_POINTS_2_3.md            # Этот документ
```

---

## Зависимости для установки

```bash
# macOS (Homebrew)
brew install qt@6 freetype harfbuzz cmake

# Ubuntu/Debian
sudo apt install qt6-base-dev libfreetype-dev libharfbuzz-dev cmake

# Шрифты STIX2
git clone https://github.com/stipub/stixfonts.git
# Скопировать OTF файлы в formula_widget/fonts/
```

---

## Риски и сложности

| Риск | Описание | Митигация |
|------|----------|-----------|
| **Координаты** | Инверсия Y между mfl и Qt | Тщательное тестирование с простыми формулами |
| **Шрифты** | Неправильные метрики → кривой layout | Использовать те же шрифты что в тестах mfl (STIX2) |
| **Аннотации** | Не все box'ы имеют семантику | Аннотировать только значимые box'ы, остальные — `root` |
| **C++23** | mfl требует C++23 (std::format, ranges) | Убедиться что компилятор поддерживает (Clang 16+, GCC 13+) |
| **Глифы в Qt** | QRawFont может не поддерживать MATH table | Использовать FreeType для рендеринга bitmap → QImage |
| **Bounding box** | Неточные bbox для глифов (нет ширины в shaped_glyph) | Запрашивать ширину через font_face или хранить в дереве |

---

## Оценка трудоёмкости

| Задача | Часы |
|--------|------|
| 2.1 FtFontFace (копирование + адаптация из тестов) | 3-4 |
| 2.2 FormulaWidget (paintEvent, координаты) | 4-6 |
| 2.3 Рендеринг глифов через Qt/FreeType | 3-5 |
| 2.4 CMake + сборка | 1-2 |
| 2.5 Демо + отладка | 2-3 |
| 3.1 formula_node в layout.hpp | 1 |
| 3.2 annotation в box.hpp | 0.5 |
| 3.3 Модификация layout.cpp | 3-4 |
| 3.4 Аннотации в noad-обработчиках | 4-6 |
| 3.5 Bounding box вычисление | 2-3 |
| 3.6 Тестирование + отладка дерева | 3-4 |
| 3.7 Debug-визуализация bbox в виджете | 1-2 |
| **Итого** | **~28-40 часов** |

---

## Альтернативный подход B (без модификации mfl)

Если нежелательно модифицировать mfl, можно:

1. Вызвать `mfl::parse()` (внутренняя функция, нужно сделать публичной или скопировать парсер)
2. Получить `vector<noad>` — семантическое дерево
3. Параллельно вызвать `mfl::layout()` — получить плоский список
4. Написать свой обход noad-дерева, который для каждого noad вычисляет bbox
5. Сопоставить глифы из layout_elements с noad-узлами по позициям

**Минусы:** Дублирование логики layout, сложность сопоставления, хрупкость.
**Плюсы:** Не нужно трогать mfl.

---

## Тестирование и валидация

Подробный план тестирования вынесен в отдельный документ: **[PLAN_TESTING.md](PLAN_TESTING.md)**

Краткое содержание:
- **Unit-тесты FtFontFace** — проверка math_constants, glyph_info, size_variants
- **Сравнение layout с эталоном** — наш FtFontFace vs тестовый font_face из mfl
- **Конвертация координат** — points→pixels, Y-инверсия
- **Рендеринг в QImage** — формула не пустая, разные формулы дают разные изображения
- **Визуальное сравнение с SVG** — сравнение с approved files из mfl
- **Структура дерева** — для каждого типа формулы проверяется правильная иерархия узлов
- **Полнота дерева** — все глифы/линии учтены, каждый принадлежит ровно одному узлу
- **Bounding box** — положительные размеры, дети внутри родителей, глифы внутри узлов
- **Hit-testing** — клик в числитель/знаменатель возвращает правильный узел
- **Чек-лист** — 25+ пунктов для финальной проверки перед сдачей

---

## Резюме

**Пункт 2** — это в основном инженерная задача: реализовать `abstract_font_face` через FreeType/HarfBuzz и написать Qt-виджет, который рисует `shaped_glyph` и `line` из `layout_elements`. Основная сложность — правильная конвертация координат и рендеринг глифов.

**Пункт 3** — это архитектурная задача: нужно модифицировать внутренности mfl, чтобы при flatten box-дерева в плоский список параллельно строилось иерархическое дерево `formula_node` с bounding box'ами. Основная сложность — правильно проставить аннотации во всех noad-обработчиках.

Оба пункта вместе дают основу для пункта 4 (hit-testing по координатам) — это будет простой рекурсивный поиск по дереву `formula_node`.
