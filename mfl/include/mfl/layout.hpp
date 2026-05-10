#pragma once

#include "mfl/abstract_font_face.hpp"
#include "mfl/font_family.hpp"
#include "mfl/units.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mfl
{
    struct shaped_glyph
    {
        font_family family = font_family::roman;
        std::size_t index = 0;
        points size;
        points x;
        points y;
        // Actual glyph metrics (advance width, height above baseline, depth below baseline)
        points advance{0};
        points height{0};
        points depth{0};
    };

    struct line
    {
        points x;
        points y;
        points length;
        points thickness;
    };

    // Тип узла формулы
    enum class formula_node_type
    {
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
    struct formula_node
    {
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

    struct layout_elements
    {
        points width;
        points height;
        std::vector<shaped_glyph> glyphs;
        std::vector<line> lines;
        std::optional<std::string> error;

        // НОВОЕ: дерево элементов
        formula_node tree;
    };

    layout_elements layout(const std::string_view input, const points font_size,
                           const font_face_creator& create_font_face);
}
