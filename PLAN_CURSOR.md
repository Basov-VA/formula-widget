# План реализации: Курсор в редакторе формул

## Контекст

### Что уже есть

Проект `mfl` — layout-движок для математических формул (LaTeX → визуальное представление). Текущее состояние:

1. **Парсинг** — TeX-строка → дерево `noad` (семантическое дерево формулы)
2. **Layout** — `noad`-дерево → `box`-дерево → плоский список `shaped_glyph` + `line`
3. **Дерево элементов** — `formula_node` с bounding box'ами для каждого узла формулы
4. **Qt-виджет** — рисует формулу на холсте, поддерживает масштабирование

### Ключевые структуры данных (уже реализованы)

```cpp
// Глиф с абсолютными координатами (include/mfl/layout.hpp)
struct shaped_glyph {
    font_family family;
    std::size_t index;
    points size;
    points x, y;           // позиция на холсте (в points, Y-up)
    points advance;         // ширина глифа
    points height;          // высота над baseline
    points depth;           // глубина под baseline
};

// Узел дерева формулы (include/mfl/layout.hpp)
struct formula_node {
    formula_node_type type;
    points bbox_x, bbox_y, bbox_width, bbox_height;  // bounding box
    std::vector<std::size_t> glyph_indices;           // индексы глифов
    std::vector<std::size_t> line_indices;             // индексы линий
    std::vector<formula_node> children;
    code_point char_code;
};

// Результат layout
struct layout_elements {
    points width, height;
    std::vector<shaped_glyph> glyphs;
    std::vector<line> lines;
    std::optional<std::string> error;
    formula_node tree;  // иерархическое дерево
};
```

### Система координат

- **mfl**: начало координат — левый нижний угол, Y растёт **вверх**, единицы — `points` (1 pt = 1/72 дюйма)
- **Qt**: начало координат — левый верхний угол, Y растёт **вниз**, единицы — пиксели
- **Bounding box глифа**: `(x, y)` — позиция baseline, `advance` — ширина, `height` — вверх от baseline, `depth` — вниз от baseline
- Полный bbox глифа: `[x, x + advance] × [y - depth, y + height]`

---

## Цель: Сущность «Курсор»

### Задача первого этапа

Реализовать механизм курсора, который:
1. Принимает координаты точки на холсте (в пикселях Qt или в points mfl)
2. Находит ближайший глиф (символ) к этой точке
3. Подсвечивает bounding box найденного глифа на холсте

---

## Этап 1: Структура данных курсора

### 1.1 Определение `FormulaCursor`

**Файл:** `formula_widget/src/formula_cursor.hpp` (новый)

```cpp
#pragma once

#include "mfl/layout.hpp"
#include <optional>
#include <cstddef>

namespace formula {

    // Результат поиска ближайшего глифа
    struct glyph_hit_result {
        std::size_t glyph_index;    // индекс в layout_elements.glyphs
        double distance;             // расстояние от точки до bbox глифа

        // Bounding box глифа в координатах mfl (points, Y-up)
        mfl::points bbox_left;
        mfl::points bbox_top;       // y + height (наивысшая точка в mfl Y-up)
        mfl::points bbox_right;     // x + advance
        mfl::points bbox_bottom;    // y - depth (наинизшая точка в mfl Y-up)
    };

    class FormulaCursor {
    public:
        FormulaCursor() = default;

        // Обновить данные layout (вызывается при смене формулы)
        void setLayout(const mfl::layout_elements* layout);

        // Найти ближайший глиф к точке (координаты в mfl points, Y-up)
        std::optional<glyph_hit_result> findNearestGlyph(mfl::points x, mfl::points y) const;

        // Установить текущую позицию курсора (в mfl points)
        void setPosition(mfl::points x, mfl::points y);

        // Получить текущий выделенный глиф (если есть)
        std::optional<glyph_hit_result> currentHighlight() const;

        // Сбросить выделение
        void clearHighlight();

        // Есть ли активное выделение
        bool hasHighlight() const;

    private:
        const mfl::layout_elements* layout_ = nullptr;
        std::optional<glyph_hit_result> current_highlight_;

        // Вычислить центр bounding box глифа
        static std::pair<double, double> glyphBBoxCenter(const mfl::shaped_glyph& g);

        // Вычислить расстояние от точки до bbox глифа
        // (0 если точка внутри bbox, иначе — расстояние до ближайшей стороны)
        static double distanceToBBox(double px, double py, const mfl::shaped_glyph& g);
    };

} // namespace formula
```

### 1.2 Реализация `FormulaCursor`

**Файл:** `formula_widget/src/formula_cursor.cpp` (новый)

Ключевые алгоритмы:

#### Вычисление расстояния до bounding box глифа

```cpp
double FormulaCursor::distanceToBBox(double px, double py, const mfl::shaped_glyph& g) {
    // Bounding box глифа в mfl координатах (Y-up):
    double left   = g.x.value();
    double right  = g.x.value() + g.advance.value();
    double bottom = g.y.value() - g.depth.value();   // наинизшая точка
    double top    = g.y.value() + g.height.value();   // наивысшая точка

    // Расстояние от точки до прямоугольника:
    // dx = max(left - px, 0, px - right)
    // dy = max(bottom - py, 0, py - top)
    // distance = sqrt(dx² + dy²)
    double dx = std::max({left - px, 0.0, px - right});
    double dy = std::max({bottom - py, 0.0, py - top});
    return std::sqrt(dx * dx + dy * dy);
}
```

#### Поиск ближайшего глифа

```cpp
std::optional<glyph_hit_result> FormulaCursor::findNearestGlyph(mfl::points x, mfl::points y) const {
    if (!layout_ || layout_->glyphs.empty()) return std::nullopt;

    double px = x.value();
    double py = y.value();

    std::size_t best_index = 0;
    double best_distance = std::numeric_limits<double>::max();

    for (std::size_t i = 0; i < layout_->glyphs.size(); ++i) {
        double dist = distanceToBBox(px, py, layout_->glyphs[i]);
        if (dist < best_distance) {
            best_distance = dist;
            best_index = i;
        }
    }

    const auto& g = layout_->glyphs[best_index];
    return glyph_hit_result{
        .glyph_index = best_index,
        .distance = best_distance,
        .bbox_left   = g.x,
        .bbox_top    = g.y + g.height,
        .bbox_right  = g.x + g.advance,
        .bbox_bottom = g.y - g.depth,
    };
}
```

---

## Этап 2: Интеграция курсора в виджет

### 2.1 Расширение `FormulaWidget`

**Файл:** `formula_widget/src/formula_widget.hpp` (модификация)

Добавить:
```cpp
class FormulaWidget : public QWidget {
    // ... существующие поля ...

public:
    // Установить координаты курсора (в пикселях виджета)
    void setCursorPosition(double pixel_x, double pixel_y);

    // Установить координаты курсора (в mfl points)
    void setCursorPositionMfl(mfl::points x, mfl::points y);

    // Включить/выключить подсветку курсора
    void setCursorHighlightEnabled(bool enabled);

    // Получить информацию о текущем выделенном глифе
    std::optional<formula::glyph_hit_result> currentCursorHit() const;

signals:
    void cursorGlyphChanged(std::size_t glyph_index);

private:
    formula::FormulaCursor cursor_;
    bool cursor_highlight_enabled_ = true;

    // Конвертация координат
    mfl::points pixelToMflX(double pixel_x) const;
    mfl::points pixelToMflY(double pixel_y) const;
    double mflToPixelX(mfl::points x) const;
    double mflToPixelY(mfl::points y) const;
};
```

### 2.2 Рисование подсветки bounding box

**Файл:** `formula_widget/src/formula_widget.cpp` (модификация `paintEvent`)

```cpp
void FormulaWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    // ... существующий рендеринг формулы ...

    // Рисование подсветки курсора
    if (cursor_highlight_enabled_ && cursor_.hasHighlight()) {
        auto hit = cursor_.currentHighlight();
        if (hit) {
            // Конвертировать bbox из mfl points в пиксели Qt
            double left   = mflToPixelX(hit->bbox_left);
            double right  = mflToPixelX(hit->bbox_right);
            double top    = mflToPixelY(hit->bbox_top);     // mfl top → Qt top (меньше y)
            double bottom = mflToPixelY(hit->bbox_bottom);  // mfl bottom → Qt bottom (больше y)

            QRectF highlight_rect(left, top, right - left, bottom - top);

            // Полупрозрачная заливка
            painter.fillRect(highlight_rect, QColor(66, 133, 244, 60));  // голубой, alpha=60

            // Рамка
            painter.setPen(QPen(QColor(66, 133, 244, 200), 1.5));
            painter.drawRect(highlight_rect);
        }
    }
}
```

### 2.3 Ввод координат через UI

**Вариант A: Поля ввода координат**

Добавить в UI два поля `QDoubleSpinBox` для ввода X и Y координат, и кнопку «Найти глиф»:

```cpp
// В main.cpp или в отдельном виджете управления
QDoubleSpinBox* spinX = new QDoubleSpinBox;
QDoubleSpinBox* spinY = new QDoubleSpinBox;
QPushButton* btnFind = new QPushButton("Найти ближайший глиф");

connect(btnFind, &QPushButton::clicked, [=]() {
    double x = spinX->value();
    double y = spinY->value();
    formulaWidget->setCursorPositionMfl(mfl::points{x}, mfl::points{y});
});
```

**Вариант B: Клик мышью по холсту (для будущего расширения)**

```cpp
void FormulaWidget::mousePressEvent(QMouseEvent* event) {
    double px = event->position().x();
    double py = event->position().y();
    setCursorPosition(px, py);
}
```

---

## Этап 3: Конвертация координат

### 3.1 Пиксели Qt → mfl points

```cpp
mfl::points FormulaWidget::pixelToMflX(double pixel_x) const {
    // pixel_x → mfl_x: убрать отступ, конвертировать px → pt
    double dpi = logicalDpiX();  // или фиксированное значение
    double pt_x = (pixel_x - margin_left_) * 72.0 / dpi;
    return mfl::points{pt_x};
}

mfl::points FormulaWidget::pixelToMflY(double pixel_y) const {
    // Qt Y-down → mfl Y-up: инвертировать
    double dpi = logicalDpiY();
    // В Qt: pixel_y=0 — верх виджета
    // В mfl: y=0 — baseline (примерно низ формулы)
    // formula_height_px = pointsToPixels(layout_.height)
    double formula_height_px = layout_.height.value() * dpi / 72.0;
    double pt_y = (formula_height_px - (pixel_y - margin_top_)) * 72.0 / dpi;
    return mfl::points{pt_y};
}
```

### 3.2 mfl points → пиксели Qt

```cpp
double FormulaWidget::mflToPixelX(mfl::points x) const {
    double dpi = logicalDpiX();
    return x.value() * dpi / 72.0 + margin_left_;
}

double FormulaWidget::mflToPixelY(mfl::points y) const {
    // mfl Y-up → Qt Y-down: инвертировать
    double dpi = logicalDpiY();
    double formula_height_px = layout_.height.value() * dpi / 72.0;
    return formula_height_px - y.value() * dpi / 72.0 + margin_top_;
}
```

---

## Этап 4: Файлы для создания/модификации

| Файл | Действие | Описание |
|------|----------|----------|
| `formula_widget/src/formula_cursor.hpp` | **Создать** | Класс `FormulaCursor` — поиск ближайшего глифа |
| `formula_widget/src/formula_cursor.cpp` | **Создать** | Реализация алгоритмов поиска |
| `formula_widget/src/formula_widget.hpp` | **Модифицировать** | Добавить поле `cursor_`, методы `setCursorPosition*` |
| `formula_widget/src/formula_widget.cpp` | **Модифицировать** | Рисование подсветки в `paintEvent`, конвертация координат |
| `formula_widget/src/main.cpp` | **Модифицировать** | Добавить UI для ввода координат |
| `formula_widget/CMakeLists.txt` | **Модифицировать** | Добавить `formula_cursor.cpp` в сборку |
| `formula_widget/tests/test_cursor.cpp` | **Создать** | Unit-тесты курсора |

---

## Этап 5: Порядок реализации

### Шаг 1: Создать `FormulaCursor` (чистая логика, без Qt)
- Реализовать `distanceToBBox()` — расстояние от точки до bbox глифа
- Реализовать `findNearestGlyph()` — перебор всех глифов, поиск минимального расстояния
- Реализовать `setPosition()` / `currentHighlight()` / `clearHighlight()`

### Шаг 2: Написать unit-тесты для `FormulaCursor`
- Тесты на `distanceToBBox` с известными значениями
- Тесты на `findNearestGlyph` с синтетическими layout_elements
- Тесты на `findNearestGlyph` с реальными формулами через `mfl::layout()`

### Шаг 3: Интегрировать курсор в `FormulaWidget`
- Добавить поле `cursor_` и обновлять его при смене формулы
- Реализовать `setCursorPosition()` и `setCursorPositionMfl()`
- Реализовать рисование подсветки в `paintEvent`

### Шаг 4: Добавить UI для ввода координат
- Два `QDoubleSpinBox` для X и Y
- Кнопка «Найти» или автоматическое обновление при изменении значений
- Отображение информации о найденном глифе (индекс, тип, координаты)

### Шаг 5: Тестирование и отладка
- Проверить на простых формулах (`x`, `a+b`)
- Проверить на сложных формулах (`\frac{x^2}{\sqrt{y}}`)
- Проверить граничные случаи (точка за пределами формулы, пустая формула)

---

## Оценка трудоёмкости

| Задача | Часы |
|--------|------|
| 1. `FormulaCursor` — структура и алгоритмы | 2-3 |
| 2. Unit-тесты курсора | 2-3 |
| 3. Интеграция в `FormulaWidget` | 2-3 |
| 4. Конвертация координат Qt ↔ mfl | 1-2 |
| 5. UI для ввода координат | 1-2 |
| 6. Рисование подсветки bbox | 1-2 |
| 7. Тестирование и отладка | 2-3 |
| **Итого** | **~11-18 часов** |

---

## Риски и сложности

| Риск | Описание | Митигация |
|------|----------|-----------|
| **Координаты** | Ошибка в конвертации Qt ↔ mfl приведёт к неправильному выделению | Тщательные unit-тесты конвертации, визуальная проверка |
| **Перекрытие глифов** | В формулах типа `x_i^2` глифы могут перекрываться | Алгоритм `distanceToBBox` корректно обрабатывает перекрытия — выбирает ближайший |
| **Пустые глифы** | Некоторые «глифы» могут быть невидимыми (пробелы, kern) | Фильтровать глифы с нулевыми размерами |
| **Масштабирование** | При изменении масштаба координаты курсора должны пересчитываться | Хранить координаты в mfl points, конвертировать при рисовании |
| **Производительность** | Линейный поиск O(n) по всем глифам | Для формул с <1000 глифов это не проблема; при необходимости — spatial index |

---

## Будущие расширения (за рамками текущего этапа)

1. **Клик мышью** — `mousePressEvent` вместо ввода координат вручную
2. **Навигация стрелками** — перемещение курсора между соседними глифами
3. **Выделение диапазона** — подсветка нескольких глифов (Shift+клик)
4. **Курсор-каретка** — вертикальная линия между глифами (для вставки текста)
5. **Подсветка узла дерева** — при наведении на глиф подсвечивать весь узел `formula_node` (числитель, знаменатель и т.д.)

---

# Тесты

## Часть 1: Unit-тесты `FormulaCursor` — расстояние до bbox

### Тест 1.1: Точка внутри bounding box глифа

```cpp
TEST_CASE("distanceToBBox: point inside glyph bbox returns 0") {
    mfl::shaped_glyph g;
    g.x = mfl::points{10.0};
    g.y = mfl::points{5.0};       // baseline
    g.advance = mfl::points{8.0}; // ширина
    g.height = mfl::points{7.0};  // вверх от baseline
    g.depth = mfl::points{2.0};   // вниз от baseline
    // bbox: [10, 18] × [3, 12]  (x: left..right, y: bottom..top в mfl Y-up)

    // Точка в центре bbox
    CHECK(FormulaCursor::distanceToBBox(14.0, 7.5, g) == Approx(0.0));

    // Точка на baseline внутри bbox
    CHECK(FormulaCursor::distanceToBBox(12.0, 5.0, g) == Approx(0.0));

    // Точка у левого края внутри bbox
    CHECK(FormulaCursor::distanceToBBox(10.0, 5.0, g) == Approx(0.0));

    // Точка у правого края внутри bbox
    CHECK(FormulaCursor::distanceToBBox(18.0, 5.0, g) == Approx(0.0));

    // Точка у верхнего края внутри bbox
    CHECK(FormulaCursor::distanceToBBox(14.0, 12.0, g) == Approx(0.0));

    // Точка у нижнего края внутри bbox
    CHECK(FormulaCursor::distanceToBBox(14.0, 3.0, g) == Approx(0.0));
}
```

### Тест 1.2: Точка снаружи bounding box — по горизонтали

```cpp
TEST_CASE("distanceToBBox: point outside horizontally") {
    mfl::shaped_glyph g;
    g.x = mfl::points{10.0};
    g.y = mfl::points{5.0};
    g.advance = mfl::points{8.0};
    g.height = mfl::points{7.0};
    g.depth = mfl::points{2.0};
    // bbox: [10, 18] × [3, 12]

    // Точка слева от bbox, на уровне центра по Y
    // px=5, py=7.5 → dx = 10-5 = 5, dy = 0 → dist = 5
    CHECK(FormulaCursor::distanceToBBox(5.0, 7.5, g) == Approx(5.0));

    // Точка справа от bbox
    // px=23, py=7.5 → dx = 23-18 = 5, dy = 0 → dist = 5
    CHECK(FormulaCursor::distanceToBBox(23.0, 7.5, g) == Approx(5.0));
}
```

### Тест 1.3: Точка снаружи bounding box — по вертикали

```cpp
TEST_CASE("distanceToBBox: point outside vertically") {
    mfl::shaped_glyph g;
    g.x = mfl::points{10.0};
    g.y = mfl::points{5.0};
    g.advance = mfl::points{8.0};
    g.height = mfl::points{7.0};
    g.depth = mfl::points{2.0};
    // bbox: [10, 18] × [3, 12]

    // Точка выше bbox
    // px=14, py=15 → dx = 0, dy = 15-12 = 3 → dist = 3
    CHECK(FormulaCursor::distanceToBBox(14.0, 15.0, g) == Approx(3.0));

    // Точка ниже bbox
    // px=14, py=0 → dx = 0, dy = 3-0 = 3 → dist = 3
    CHECK(FormulaCursor::distanceToBBox(14.0, 0.0, g) == Approx(3.0));
}
```

### Тест 1.4: Точка снаружи bounding box — по диагонали

```cpp
TEST_CASE("distanceToBBox: point outside diagonally") {
    mfl::shaped_glyph g;
    g.x = mfl::points{10.0};
    g.y = mfl::points{5.0};
    g.advance = mfl::points{8.0};
    g.height = mfl::points{7.0};
    g.depth = mfl::points{2.0};
    // bbox: [10, 18] × [3, 12]

    // Точка в левом нижнем углу от bbox
    // px=7, py=0 → dx = 10-7 = 3, dy = 3-0 = 3 → dist = sqrt(9+9) = sqrt(18) ≈ 4.243
    CHECK(FormulaCursor::distanceToBBox(7.0, 0.0, g) == Approx(std::sqrt(18.0)));

    // Точка в правом верхнем углу от bbox
    // px=22, py=16 → dx = 22-18 = 4, dy = 16-12 = 4 → dist = sqrt(32) ≈ 5.657
    CHECK(FormulaCursor::distanceToBBox(22.0, 16.0, g) == Approx(std::sqrt(32.0)));
}
```

### Тест 1.5: Глиф с нулевой глубиной (depth = 0)

```cpp
TEST_CASE("distanceToBBox: glyph with zero depth") {
    mfl::shaped_glyph g;
    g.x = mfl::points{0.0};
    g.y = mfl::points{0.0};
    g.advance = mfl::points{5.0};
    g.height = mfl::points{8.0};
    g.depth = mfl::points{0.0};
    // bbox: [0, 5] × [0, 8]

    // Точка на baseline (нижний край bbox)
    CHECK(FormulaCursor::distanceToBBox(2.5, 0.0, g) == Approx(0.0));

    // Точка чуть ниже baseline
    CHECK(FormulaCursor::distanceToBBox(2.5, -1.0, g) == Approx(1.0));
}
```

### Тест 1.6: Глиф с нулевыми размерами (вырожденный случай)

```cpp
TEST_CASE("distanceToBBox: degenerate glyph with zero advance") {
    mfl::shaped_glyph g;
    g.x = mfl::points{5.0};
    g.y = mfl::points{5.0};
    g.advance = mfl::points{0.0};
    g.height = mfl::points{0.0};
    g.depth = mfl::points{0.0};
    // bbox: [5, 5] × [5, 5] — точка

    // Расстояние от (5,5) до точки (5,5) = 0
    CHECK(FormulaCursor::distanceToBBox(5.0, 5.0, g) == Approx(0.0));

    // Расстояние от (8,9) до точки (5,5) = sqrt(9+16) = 5
    CHECK(FormulaCursor::distanceToBBox(8.0, 9.0, g) == Approx(5.0));
}
```

---

## Часть 2: Unit-тесты `FormulaCursor` — поиск ближайшего глифа

### Тест 2.1: Один глиф — всегда находится

```cpp
TEST_CASE("findNearestGlyph: single glyph always found") {
    mfl::layout_elements layout;
    layout.glyphs.push_back({
        .family = mfl::font_family::roman,
        .index = 42,
        .size = mfl::points{12.0},
        .x = mfl::points{10.0},
        .y = mfl::points{0.0},
        .advance = mfl::points{6.0},
        .height = mfl::points{8.0},
        .depth = mfl::points{0.0},
    });

    FormulaCursor cursor;
    cursor.setLayout(&layout);

    // Точка внутри глифа
    auto result = cursor.findNearestGlyph(mfl::points{13.0}, mfl::points{4.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 0);
    CHECK(result->distance == Approx(0.0));

    // Точка далеко от глифа — всё равно находит единственный
    result = cursor.findNearestGlyph(mfl::points{100.0}, mfl::points{100.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 0);
    CHECK(result->distance > 0.0);
}
```

### Тест 2.2: Два глифа — выбирается ближайший

```cpp
TEST_CASE("findNearestGlyph: two glyphs, selects nearest") {
    mfl::layout_elements layout;

    // Глиф 'a' на позиции x=0
    layout.glyphs.push_back({
        .family = mfl::font_family::italic,
        .index = 1,
        .size = mfl::points{12.0},
        .x = mfl::points{0.0},
        .y = mfl::points{0.0},
        .advance = mfl::points{6.0},
        .height = mfl::points{8.0},
        .depth = mfl::points{0.0},
    });

    // Глиф 'b' на позиции x=20
    layout.glyphs.push_back({
        .family = mfl::font_family::italic,
        .index = 2,
        .size = mfl::points{12.0},
        .x = mfl::points{20.0},
        .y = mfl::points{0.0},
        .advance = mfl::points{6.0},
        .height = mfl::points{8.0},
        .depth = mfl::points{0.0},
    });

    FormulaCursor cursor;
    cursor.setLayout(&layout);

    // Точка ближе к первому глифу
    auto result = cursor.findNearestGlyph(mfl::points{3.0}, mfl::points{4.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 0);

    // Точка ближе ко второму глифу
    result = cursor.findNearestGlyph(mfl::points{22.0}, mfl::points{4.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 1);

    // Точка ровно посередине между глифами (x=13, между [0,6] и [20,26])
    // Расстояние до первого: dx = 13-6 = 7, до второго: dx = 20-13 = 7 → равны
    // Алгоритм выберет первый найденный (индекс 0)
    result = cursor.findNearestGlyph(mfl::points{13.0}, mfl::points{4.0});
    REQUIRE(result.has_value());
    CHECK((result->glyph_index == 0 || result->glyph_index == 1));
}
```

### Тест 2.3: Пустой layout — возвращает nullopt

```cpp
TEST_CASE("findNearestGlyph: empty layout returns nullopt") {
    mfl::layout_elements layout;
    // Нет глифов

    FormulaCursor cursor;
    cursor.setLayout(&layout);

    auto result = cursor.findNearestGlyph(mfl::points{5.0}, mfl::points{5.0});
    CHECK(!result.has_value());
}
```

### Тест 2.4: Layout не установлен — возвращает nullopt

```cpp
TEST_CASE("findNearestGlyph: no layout set returns nullopt") {
    FormulaCursor cursor;
    // setLayout не вызван

    auto result = cursor.findNearestGlyph(mfl::points{5.0}, mfl::points{5.0});
    CHECK(!result.has_value());
}
```

### Тест 2.5: Несколько глифов в строку — точка попадает точно в один

```cpp
TEST_CASE("findNearestGlyph: point exactly inside one of many glyphs") {
    mfl::layout_elements layout;

    // Три глифа подряд: [0,6], [8,14], [16,22] по X, все на baseline y=0
    for (int i = 0; i < 3; ++i) {
        layout.glyphs.push_back({
            .family = mfl::font_family::italic,
            .index = static_cast<std::size_t>(i + 10),
            .size = mfl::points{12.0},
            .x = mfl::points{static_cast<double>(i * 8)},
            .y = mfl::points{0.0},
            .advance = mfl::points{6.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{2.0},
        });
    }

    FormulaCursor cursor;
    cursor.setLayout(&layout);

    // Точка внутри второго глифа (x=10, y=3) → bbox [8,14] × [-2, 8]
    auto result = cursor.findNearestGlyph(mfl::points{10.0}, mfl::points{3.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 1);
    CHECK(result->distance == Approx(0.0));

    // Точка внутри третьего глифа
    result = cursor.findNearestGlyph(mfl::points{19.0}, mfl::points{3.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 2);
    CHECK(result->distance == Approx(0.0));
}
```

### Тест 2.6: Глифы в дроби — числитель и знаменатель на разных Y

```cpp
TEST_CASE("findNearestGlyph: fraction-like layout with different Y positions") {
    mfl::layout_elements layout;

    // Числитель: глиф на y=10 (выше)
    layout.glyphs.push_back({
        .family = mfl::font_family::italic,
        .index = 1,
        .size = mfl::points{12.0},
        .x = mfl::points{5.0},
        .y = mfl::points{10.0},
        .advance = mfl::points{6.0},
        .height = mfl::points{8.0},
        .depth = mfl::points{0.0},
    });
    // bbox числителя: [5, 11] × [10, 18]

    // Знаменатель: глиф на y=-5 (ниже)
    layout.glyphs.push_back({
        .family = mfl::font_family::italic,
        .index = 2,
        .size = mfl::points{12.0},
        .x = mfl::points{5.0},
        .y = mfl::points{-5.0},
        .advance = mfl::points{6.0},
        .height = mfl::points{8.0},
        .depth = mfl::points{0.0},
    });
    // bbox знаменателя: [5, 11] × [-5, 3]

    FormulaCursor cursor;
    cursor.setLayout(&layout);

    // Точка в области числителя
    auto result = cursor.findNearestGlyph(mfl::points{8.0}, mfl::points{14.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 0);  // числитель

    // Точка в области знаменателя
    result = cursor.findNearestGlyph(mfl::points{8.0}, mfl::points{-2.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 1);  // знаменатель

    // Точка между числителем и знаменателем (y=6, между [3] и [10])
    // Расстояние до числителя: dy = 10-6 = 4
    // Расстояние до знаменателя: dy = 6-3 = 3
    // Ближе к знаменателю
    result = cursor.findNearestGlyph(mfl::points{8.0}, mfl::points{6.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 1);  // знаменатель ближе
}
```

---

## Часть 3: Unit-тесты `FormulaCursor` — состояние курсора

### Тест 3.1: setPosition обновляет highlight

```cpp
TEST_CASE("setPosition updates current highlight") {
    mfl::layout_elements layout;
    layout.glyphs.push_back({
        .family = mfl::font_family::roman,
        .index = 1,
        .size = mfl::points{12.0},
        .x = mfl::points{0.0},
        .y = mfl::points{0.0},
        .advance = mfl::points{6.0},
        .height = mfl::points{8.0},
        .depth = mfl::points{0.0},
    });

    FormulaCursor cursor;
    cursor.setLayout(&layout);

    CHECK(!cursor.hasHighlight());

    cursor.setPosition(mfl::points{3.0}, mfl::points{4.0});

    CHECK(cursor.hasHighlight());
    auto hit = cursor.currentHighlight();
    REQUIRE(hit.has_value());
    CHECK(hit->glyph_index == 0);
}
```

### Тест 3.2: clearHighlight сбрасывает выделение

```cpp
TEST_CASE("clearHighlight removes current highlight") {
    mfl::layout_elements layout;
    layout.glyphs.push_back({
        .family = mfl::font_family::roman,
        .index = 1,
        .size = mfl::points{12.0},
        .x = mfl::points{0.0},
        .y = mfl::points{0.0},
        .advance = mfl::points{6.0},
        .height = mfl::points{8.0},
        .depth = mfl::points{0.0},
    });

    FormulaCursor cursor;
    cursor.setLayout(&layout);
    cursor.setPosition(mfl::points{3.0}, mfl::points{4.0});

    CHECK(cursor.hasHighlight());

    cursor.clearHighlight();

    CHECK(!cursor.hasHighlight());
    CHECK(!cursor.currentHighlight().has_value());
}
```

### Тест 3.3: setLayout сбрасывает highlight

```cpp
TEST_CASE("setLayout resets highlight") {
    mfl::layout_elements layout1;
    layout1.glyphs.push_back({
        .family = mfl::font_family::roman,
        .index = 1,
        .size = mfl::points{12.0},
        .x = mfl::points{0.0},
        .y = mfl::points{0.0},
        .advance = mfl::points{6.0},
        .height = mfl::points{8.0},
        .depth = mfl::points{0.0},
    });

    FormulaCursor cursor;
    cursor.setLayout(&layout1);
    cursor.setPosition(mfl::points{3.0}, mfl::points{4.0});
    CHECK(cursor.hasHighlight());

    // Установить новый layout — highlight должен сброситься
    mfl::layout_elements layout2;
    cursor.setLayout(&layout2);
    CHECK(!cursor.hasHighlight());
}
```

---

## Часть 4: Unit-тесты `FormulaCursor` — корректность bbox в результате

### Тест 4.1: Bbox в результате соответствует глифу

```cpp
TEST_CASE("glyph_hit_result bbox matches glyph metrics") {
    mfl::layout_elements layout;
    layout.glyphs.push_back({
        .family = mfl::font_family::italic,
        .index = 42,
        .size = mfl::points{12.0},
        .x = mfl::points{10.0},
        .y = mfl::points{5.0},
        .advance = mfl::points{7.0},
        .height = mfl::points{9.0},
        .depth = mfl::points{3.0},
    });

    FormulaCursor cursor;
    cursor.setLayout(&layout);

    auto result = cursor.findNearestGlyph(mfl::points{13.0}, mfl::points{5.0});
    REQUIRE(result.has_value());

    // bbox_left = x = 10
    CHECK(result->bbox_left.value() == Approx(10.0));
    // bbox_right = x + advance = 10 + 7 = 17
    CHECK(result->bbox_right.value() == Approx(17.0));
    // bbox_top = y + height = 5 + 9 = 14
    CHECK(result->bbox_top.value() == Approx(14.0));
    // bbox_bottom = y - depth = 5 - 3 = 2
    CHECK(result->bbox_bottom.value() == Approx(2.0));
}
```

---

## Часть 5: Интеграционные тесты с реальными формулами

### Тест 5.1: Простая формула `x` — единственный глиф

```cpp
TEST_CASE("Integration: cursor on simple formula 'x'") {
    auto elements = mfl::layout("x", mfl::points{12.0}, create_font_face);
    REQUIRE(!elements.error.has_value());
    REQUIRE(elements.glyphs.size() >= 1);

    FormulaCursor cursor;
    cursor.setLayout(&elements);

    // Точка в центре формулы
    double cx = elements.width.value() / 2.0;
    double cy = elements.height.value() / 2.0;
    auto result = cursor.findNearestGlyph(mfl::points{cx}, mfl::points{cy});

    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 0);
    // Bbox должен иметь положительные размеры
    CHECK(result->bbox_right.value() > result->bbox_left.value());
    CHECK(result->bbox_top.value() > result->bbox_bottom.value());
}
```

### Тест 5.2: Формула `a+b` — три глифа, выбор по позиции

```cpp
TEST_CASE("Integration: cursor on formula 'a+b' selects correct glyph") {
    auto elements = mfl::layout("a+b", mfl::points{12.0}, create_font_face);
    REQUIRE(!elements.error.has_value());
    REQUIRE(elements.glyphs.size() >= 3);  // a, +, b

    FormulaCursor cursor;
    cursor.setLayout(&elements);

    // Точка у первого глифа 'a'
    const auto& g0 = elements.glyphs[0];
    auto result = cursor.findNearestGlyph(
        g0.x + g0.advance * 0.5,
        g0.y
    );
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 0);

    // Точка у последнего глифа 'b'
    const auto& g_last = elements.glyphs.back();
    result = cursor.findNearestGlyph(
        g_last.x + g_last.advance * 0.5,
        g_last.y
    );
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == elements.glyphs.size() - 1);
}
```

### Тест 5.3: Формула `\frac{a}{b}` — числитель и знаменатель

```cpp
TEST_CASE("Integration: cursor on fraction distinguishes numerator and denominator") {
    auto elements = mfl::layout(R"(\frac{a}{b})", mfl::points{12.0}, create_font_face);
    REQUIRE(!elements.error.has_value());
    REQUIRE(elements.glyphs.size() >= 2);

    FormulaCursor cursor;
    cursor.setLayout(&elements);

    // Найти глиф с наибольшим Y (числитель) и наименьшим Y (знаменатель)
    std::size_t top_idx = 0, bottom_idx = 0;
    double max_y = -1e9, min_y = 1e9;
    for (std::size_t i = 0; i < elements.glyphs.size(); ++i) {
        double y = elements.glyphs[i].y.value();
        if (y > max_y) { max_y = y; top_idx = i; }
        if (y < min_y) { min_y = y; bottom_idx = i; }
    }

    // Точка рядом с числителем (высокий Y)
    const auto& g_top = elements.glyphs[top_idx];
    auto result = cursor.findNearestGlyph(
        g_top.x + g_top.advance * 0.5,
        g_top.y + g_top.height * 0.5
    );
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == top_idx);

    // Точка рядом со знаменателем (низкий Y)
    const auto& g_bot = elements.glyphs[bottom_idx];
    result = cursor.findNearestGlyph(
        g_bot.x + g_bot.advance * 0.5,
        g_bot.y - g_bot.depth * 0.5
    );
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == bottom_idx);
}
```

### Тест 5.4: Формула `x^2` — основание и степень

```cpp
TEST_CASE("Integration: cursor on x^2 distinguishes base and exponent") {
    auto elements = mfl::layout("x^2", mfl::points{12.0}, create_font_face);
    REQUIRE(!elements.error.has_value());
    REQUIRE(elements.glyphs.size() >= 2);

    FormulaCursor cursor;
    cursor.setLayout(&elements);

    // Глиф 'x' (основание) — обычно первый, больший размер
    // Глиф '2' (степень) — обычно второй, меньший размер, выше по Y
    const auto& g_base = elements.glyphs[0];
    const auto& g_exp = elements.glyphs[1];

    // Точка рядом с основанием
    auto result = cursor.findNearestGlyph(
        g_base.x + g_base.advance * 0.5,
        g_base.y
    );
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 0);

    // Точка рядом со степенью
    result = cursor.findNearestGlyph(
        g_exp.x + g_exp.advance * 0.5,
        g_exp.y + g_exp.height * 0.5
    );
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 1);
}
```

### Тест 5.5: Точка далеко за пределами формулы

```cpp
TEST_CASE("Integration: cursor far outside formula still finds nearest glyph") {
    auto elements = mfl::layout("x", mfl::points{12.0}, create_font_face);
    REQUIRE(!elements.error.has_value());

    FormulaCursor cursor;
    cursor.setLayout(&elements);

    // Точка очень далеко справа
    auto result = cursor.findNearestGlyph(mfl::points{1000.0}, mfl::points{0.0});
    REQUIRE(result.has_value());
    CHECK(result->glyph_index == 0);
    CHECK(result->distance > 0.0);

    // Точка очень далеко вверху
    result = cursor.findNearestGlyph(mfl::points{0.0}, mfl::points{1000.0});
    REQUIRE(result.has_value());
    CHECK(result->distance > 0.0);

    // Точка в отрицательных координатах
    result = cursor.findNearestGlyph(mfl::points{-100.0}, mfl::points{-100.0});
    REQUIRE(result.has_value());
    CHECK(result->distance > 0.0);
}
```

---

## Часть 6: Тесты конвертации координат

### Тест 6.1: Points → Pixels при разных DPI

```cpp
TEST_CASE("Coordinate conversion: mfl points to Qt pixels") {
    // При 72 DPI: 1 point = 1 pixel
    CHECK(pointsToPixels(mfl::points{10.0}, 72.0) == Approx(10.0));
    CHECK(pointsToPixels(mfl::points{0.0}, 72.0) == Approx(0.0));

    // При 96 DPI: 72 points = 96 pixels
    CHECK(pointsToPixels(mfl::points{72.0}, 96.0) == Approx(96.0));
    CHECK(pointsToPixels(mfl::points{36.0}, 96.0) == Approx(48.0));

    // При 144 DPI: 1 point = 2 pixels
    CHECK(pointsToPixels(mfl::points{10.0}, 144.0) == Approx(20.0));
}
```

### Тест 6.2: Y-инверсия mfl → Qt

```cpp
TEST_CASE("Y-axis inversion: mfl Y-up to Qt Y-down") {
    double formula_height_pt = 20.0;  // высота формулы в points
    double dpi = 72.0;                // 1pt = 1px для простоты

    // В mfl: y=0 — baseline (низ), y=20 — верх
    // В Qt: y=0 — верх виджета, y=20 — низ

    // mfl y=20 (верх формулы) → Qt y=0 (верх виджета)
    double qt_y_top = mflYToQtY(mfl::points{20.0}, formula_height_pt, dpi);
    CHECK(qt_y_top == Approx(0.0));

    // mfl y=0 (низ формулы) → Qt y=20 (низ виджета)
    double qt_y_bottom = mflYToQtY(mfl::points{0.0}, formula_height_pt, dpi);
    CHECK(qt_y_bottom == Approx(20.0));

    // mfl y=10 (середина) → Qt y=10 (середина)
    double qt_y_mid = mflYToQtY(mfl::points{10.0}, formula_height_pt, dpi);
    CHECK(qt_y_mid == Approx(10.0));
}
```

### Тест 6.3: Roundtrip: mfl → Qt → mfl

```cpp
TEST_CASE("Roundtrip: mfl points -> Qt pixels -> mfl points") {
    double dpi = 96.0;
    double formula_height_pt = 30.0;

    mfl::points original_x{15.0};
    mfl::points original_y{10.0};

    // mfl → Qt
    double qt_x = mflXToQtX(original_x, dpi);
    double qt_y = mflYToQtY(original_y, formula_height_pt, dpi);

    // Qt → mfl
    mfl::points roundtrip_x = qtXToMflX(qt_x, dpi);
    mfl::points roundtrip_y = qtYToMflY(qt_y, formula_height_pt, dpi);

    CHECK(roundtrip_x.value() == Approx(original_x.value()));
    CHECK(roundtrip_y.value() == Approx(original_y.value()));
}
```

---

## Часть 7: Чек-лист валидации курсора

### Функциональные проверки

- [ ] `FormulaCursor` компилируется без ошибок
- [ ] `distanceToBBox` возвращает 0 для точки внутри bbox
- [ ] `distanceToBBox` возвращает корректное расстояние для точки снаружи
- [ ] `findNearestGlyph` возвращает `nullopt` для пустого layout
- [ ] `findNearestGlyph` возвращает `nullopt` если layout не установлен
- [ ] `findNearestGlyph` находит единственный глиф при любой точке
- [ ] `findNearestGlyph` выбирает ближайший из нескольких глифов
- [ ] `setPosition` обновляет `currentHighlight`
- [ ] `clearHighlight` сбрасывает выделение
- [ ] `setLayout` сбрасывает выделение
- [ ] Bbox в `glyph_hit_result` соответствует метрикам глифа

### Интеграционные проверки

- [ ] Курсор работает с реальной формулой `x`
- [ ] Курсор различает глифы в `a+b`
- [ ] Курсор различает числитель и знаменатель в `\frac{a}{b}`
- [ ] Курсор различает основание и степень в `x^2`
- [ ] Курсор работает для точки далеко за пределами формулы
- [ ] Конвертация координат mfl ↔ Qt корректна при разных DPI
- [ ] Y-инверсия работает правильно
- [ ] Roundtrip конвертация не теряет точность

### Визуальные проверки (ручные)

- [ ] Подсветка bbox отображается на правильном глифе
- [ ] Подсветка имеет полупрозрачную заливку и рамку
- [ ] При вводе координат рядом с числителем — подсвечивается числитель
- [ ] При вводе координат рядом со знаменателем — подсвечивается знаменатель
- [ ] При вводе координат рядом со степенью — подсвечивается степень
- [ ] Подсветка корректно масштабируется при изменении размера шрифта
- [ ] Подсветка не выходит за пределы виджета
