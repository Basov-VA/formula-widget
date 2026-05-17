#pragma once

#include "mfl/layout.hpp"
#include <optional>
#include <cstddef>

namespace formula {

    // Направления навигации стрелочками
    enum class NavigationDirection {
        Left,
        Right,
        Up,
        Down
    };

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

        // Вычислить расстояние от точки до bbox глифа
        // (0 если точка внутри bbox, иначе — расстояние до ближайшей стороны)
        static double distanceToBBox(double px, double py, const mfl::shaped_glyph& g);

        // Навигация стрелочками: переместить курсор в указанном направлении
        // Возвращает true если перемещение произошло, false если некуда двигаться
        bool moveToDirection(NavigationDirection direction);

        // Получить индекс текущего глифа (если есть)
        std::optional<std::size_t> currentGlyphIndex() const;

        // Установить курсор на конкретный глиф по индексу
        void setGlyphIndex(std::size_t index);

        // Найти ближайший глиф в заданном направлении от текущего
        std::optional<std::size_t> findGlyphInDirection(
            std::size_t current_index, NavigationDirection direction) const;

    private:
        const mfl::layout_elements* layout_ = nullptr;
        std::optional<glyph_hit_result> current_highlight_;

        // Вычислить центр bounding box глифа
        static std::pair<double, double> glyphBBoxCenter(const mfl::shaped_glyph& g);
    };

} // namespace formula
