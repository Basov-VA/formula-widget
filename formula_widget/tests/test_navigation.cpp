#include "formula_cursor.hpp"
#include <QtTest/QtTest>
#include <memory>

class TestNavigation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        // Any setup needed before all tests
    }

    void cleanupTestCase() {
        // Any cleanup needed after all tests
    }

    // Part 1: Linear formula tests
    void test_three_glyphs_in_row_Right_moves_forward() {
        // Формула: a + b  (три глифа на одной baseline)
        // Глиф 0: x=0,  y=0, advance=6
        // Глиф 1: x=10, y=0, advance=6
        // Глиф 2: x=20, y=0, advance=6

        mfl::layout_elements layout;
        for (int i = 0; i < 3; ++i) {
            layout.glyphs.push_back({
                .family = mfl::font_family::roman,
                .index = static_cast<std::size_t>(i),
                .size = mfl::points{12.0},
                .x = mfl::points{static_cast<double>(i * 10)},
                .y = mfl::points{0.0},
                .advance = mfl::points{6.0},
                .height = mfl::points{8.0},
                .depth = mfl::points{2.0},
            });
        }

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);
        cursor.setGlyphIndex(0);

        // Right из глифа 0 → глиф 1
        auto next = cursor.findGlyphInDirection(0, formula::NavigationDirection::Right);
        QVERIFY(next.has_value());
        QCOMPARE(*next, 1u);

        // Right из глифа 1 → глиф 2
        next = cursor.findGlyphInDirection(1, formula::NavigationDirection::Right);
        QVERIFY(next.has_value());
        QCOMPARE(*next, 2u);

        // Right из глифа 2 → nullopt (нет глифов правее)
        next = cursor.findGlyphInDirection(2, formula::NavigationDirection::Right);
        QVERIFY(!next.has_value());
    }

    void test_three_glyphs_in_row_Left_moves_backward() {
        mfl::layout_elements layout;
        for (int i = 0; i < 3; ++i) {
            layout.glyphs.push_back({
                .family = mfl::font_family::roman,
                .index = static_cast<std::size_t>(i),
                .size = mfl::points{12.0},
                .x = mfl::points{static_cast<double>(i * 10)},
                .y = mfl::points{0.0},
                .advance = mfl::points{6.0},
                .height = mfl::points{8.0},
                .depth = mfl::points{2.0},
            });
        }

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Left из глифа 2 → глиф 1
        auto prev = cursor.findGlyphInDirection(2, formula::NavigationDirection::Left);
        QVERIFY(prev.has_value());
        QCOMPARE(*prev, 1u);

        // Left из глифа 1 → глиф 0
        prev = cursor.findGlyphInDirection(1, formula::NavigationDirection::Left);
        QVERIFY(prev.has_value());
        QCOMPARE(*prev, 0u);

        // Left из глифа 0 → nullopt (нет глифов левее)
        prev = cursor.findGlyphInDirection(0, formula::NavigationDirection::Left);
        QVERIFY(!prev.has_value());
    }

    void test_horizontal_formula_Up_Down_return_nullopt() {
        mfl::layout_elements layout;
        for (int i = 0; i < 3; ++i) {
            layout.glyphs.push_back({
                .family = mfl::font_family::roman,
                .index = static_cast<std::size_t>(i),
                .size = mfl::points{12.0},
                .x = mfl::points{static_cast<double>(i * 10)},
                .y = mfl::points{0.0},  // все на одной baseline
                .advance = mfl::points{6.0},
                .height = mfl::points{8.0},
                .depth = mfl::points{2.0},
            });
        }

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Up из любого глифа → nullopt (все на одной высоте)
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Up).has_value());
        QVERIFY(!cursor.findGlyphInDirection(1, formula::NavigationDirection::Up).has_value());

        // Down из любого глифа → nullopt
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Down).has_value());
        QVERIFY(!cursor.findGlyphInDirection(1, formula::NavigationDirection::Down).has_value());
    }

    // Part 2: Fraction tests (vertical navigation)
    void test_fraction_Down_from_numerator_to_denominator() {
        // Имитация \frac{a}{b}:
        // Глиф 0 (числитель 'a'): x=5, y=10  (выше baseline)
        // Глиф 1 (знаменатель 'b'): x=5, y=-5 (ниже baseline)

        mfl::layout_elements layout;

        // Числитель
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

        // Знаменатель
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

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Down из числителя (0) → знаменатель (1)
        auto target = cursor.findGlyphInDirection(0, formula::NavigationDirection::Down);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 1u);

        // Up из знаменателя (1) → числитель (0)
        target = cursor.findGlyphInDirection(1, formula::NavigationDirection::Up);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 0u);
    }

    void test_fraction_Left_Right_dont_jump_vertically() {
        // Числитель: x=5, y=10
        // Знаменатель: x=5, y=-5
        // Оба на одной X-позиции, но на разных Y

        mfl::layout_elements layout;

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

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Right из числителя — нет глифов правее → nullopt
        auto target = cursor.findGlyphInDirection(0, formula::NavigationDirection::Right);
        QVERIFY(!target.has_value());

        // Left из числителя — нет глифов левее → nullopt
        target = cursor.findGlyphInDirection(0, formula::NavigationDirection::Left);
        QVERIFY(!target.has_value());
    }

    // Part 3: Exponent tests (x^2)
    void test_exponent_Right_from_base_to_exponent() {
        // Глиф 0 ('x'): x=0, y=0, advance=7
        // Глиф 1 ('2'): x=8, y=5, advance=4 (правее и выше)

        mfl::layout_elements layout;

        // Основание 'x'
        layout.glyphs.push_back({
            .family = mfl::font_family::italic,
            .index = 1,
            .size = mfl::points{12.0},
            .x = mfl::points{0.0},
            .y = mfl::points{0.0},
            .advance = mfl::points{7.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{0.0},
        });

        // Степень '2'
        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 2,
            .size = mfl::points{8.0},
            .x = mfl::points{8.0},
            .y = mfl::points{5.0},
            .advance = mfl::points{4.0},
            .height = mfl::points{6.0},
            .depth = mfl::points{0.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Right из 'x' (0) → '2' (1) — степень правее
        auto target = cursor.findGlyphInDirection(0, formula::NavigationDirection::Right);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 1u);

        // Left из '2' (1) → 'x' (0) — основание левее
        target = cursor.findGlyphInDirection(1, formula::NavigationDirection::Left);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 0u);
    }

    void test_exponent_Up_from_base_to_exponent() {
        mfl::layout_elements layout;

        // Основание 'x': x=0, y=0
        layout.glyphs.push_back({
            .family = mfl::font_family::italic,
            .index = 1,
            .size = mfl::points{12.0},
            .x = mfl::points{0.0},
            .y = mfl::points{0.0},
            .advance = mfl::points{7.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{0.0},
        });

        // Степень '2': x=8, y=5 (выше)
        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 2,
            .size = mfl::points{8.0},
            .x = mfl::points{8.0},
            .y = mfl::points{5.0},
            .advance = mfl::points{4.0},
            .height = mfl::points{6.0},
            .depth = mfl::points{0.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Up из 'x' (0) → '2' (1) — степень выше
        auto target = cursor.findGlyphInDirection(0, formula::NavigationDirection::Up);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 1u);

        // Down из '2' (1) → 'x' (0) — основание ниже
        target = cursor.findGlyphInDirection(1, formula::NavigationDirection::Down);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 0u);
    }

    // Part 5: moveToDirection tests (full cycle)
    void test_moveToDirection_updates_current_highlight() {
        mfl::layout_elements layout;
        for (int i = 0; i < 3; ++i) {
            layout.glyphs.push_back({
                .family = mfl::font_family::roman,
                .index = static_cast<std::size_t>(i),
                .size = mfl::points{12.0},
                .x = mfl::points{static_cast<double>(i * 10)},
                .y = mfl::points{0.0},
                .advance = mfl::points{6.0},
                .height = mfl::points{8.0},
                .depth = mfl::points{2.0},
            });
        }

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);
        cursor.setGlyphIndex(0);

        QCOMPARE(cursor.currentGlyphIndex().value(), 0u);

        // Move right
        bool moved = cursor.moveToDirection(formula::NavigationDirection::Right);
        QVERIFY(moved);
        QCOMPARE(cursor.currentGlyphIndex().value(), 1u);

        // Move right again
        moved = cursor.moveToDirection(formula::NavigationDirection::Right);
        QVERIFY(moved);
        QCOMPARE(cursor.currentGlyphIndex().value(), 2u);

        // Move right — no more glyphs
        moved = cursor.moveToDirection(formula::NavigationDirection::Right);
        QVERIFY(!moved);
        QCOMPARE(cursor.currentGlyphIndex().value(), 2u); // stays at 2
    }

    void test_moveToDirection_returns_false_when_no_highlight_set() {
        mfl::layout_elements layout;
        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 0,
            .size = mfl::points{12.0},
            .x = mfl::points{0.0},
            .y = mfl::points{0.0},
            .advance = mfl::points{6.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{2.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);
        // Не вызываем setGlyphIndex / setPosition

        bool moved = cursor.moveToDirection(formula::NavigationDirection::Right);
        QVERIFY(!moved);
        QVERIFY(!cursor.hasHighlight());
    }

    void test_moveToDirection_full_traversal_Left_and_Right() {
        mfl::layout_elements layout;
        for (int i = 0; i < 4; ++i) {
            layout.glyphs.push_back({
                .family = mfl::font_family::roman,
                .index = static_cast<std::size_t>(i),
                .size = mfl::points{12.0},
                .x = mfl::points{static_cast<double>(i * 10)},
                .y = mfl::points{0.0},
                .advance = mfl::points{6.0},
                .height = mfl::points{8.0},
                .depth = mfl::points{2.0},
            });
        }

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);
        cursor.setGlyphIndex(2); // начинаем с глифа 2

        // Left → 1
        cursor.moveToDirection(formula::NavigationDirection::Left);
        QCOMPARE(cursor.currentGlyphIndex().value(), 1u);

        // Left → 0
        cursor.moveToDirection(formula::NavigationDirection::Left);
        QCOMPARE(cursor.currentGlyphIndex().value(), 0u);

        // Left → нет (остаёмся на 0)
        cursor.moveToDirection(formula::NavigationDirection::Left);
        QCOMPARE(cursor.currentGlyphIndex().value(), 0u);

        // Right → 1
        cursor.moveToDirection(formula::NavigationDirection::Right);
        QCOMPARE(cursor.currentGlyphIndex().value(), 1u);

        // Right → 2
        cursor.moveToDirection(formula::NavigationDirection::Right);
        QCOMPARE(cursor.currentGlyphIndex().value(), 2u);
    }

    // Part 6: setGlyphIndex tests
    void test_setGlyphIndex_sets_highlight_correctly() {
        mfl::layout_elements layout;
        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 42,
            .size = mfl::points{12.0},
            .x = mfl::points{10.0},
            .y = mfl::points{5.0},
            .advance = mfl::points{7.0},
            .height = mfl::points{9.0},
            .depth = mfl::points{3.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        cursor.setGlyphIndex(0);

        QVERIFY(cursor.hasHighlight());
        auto hit = cursor.currentHighlight();
        QVERIFY(hit.has_value());
        QCOMPARE(hit->glyph_index, 0u);
        QCOMPARE(hit->bbox_left.value(), 10.0);
        QCOMPARE(hit->bbox_right.value(), 17.0);
        QCOMPARE(hit->bbox_top.value(), 14.0);
        QCOMPARE(hit->bbox_bottom.value(), 2.0);
    }

    void test_setGlyphIndex_invalid_index_does_nothing() {
        mfl::layout_elements layout;
        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 0,
            .size = mfl::points{12.0},
            .x = mfl::points{0.0},
            .y = mfl::points{0.0},
            .advance = mfl::points{6.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{2.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Индекс за пределами массива
        cursor.setGlyphIndex(999);
        QVERIFY(!cursor.hasHighlight());
    }

    void test_setGlyphIndex_no_layout_does_nothing() {
        formula::FormulaCursor cursor;
        // setLayout не вызван

        cursor.setGlyphIndex(0);
        QVERIFY(!cursor.hasHighlight());
    }

    // Part 7: Edge cases
    void test_single_glyph_all_directions_return_nullopt() {
        mfl::layout_elements layout;
        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 0,
            .size = mfl::points{12.0},
            .x = mfl::points{0.0},
            .y = mfl::points{0.0},
            .advance = mfl::points{6.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{2.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Left).has_value());
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Right).has_value());
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Up).has_value());
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Down).has_value());
    }

    void test_empty_layout_returns_nullopt() {
        mfl::layout_elements layout;

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Right).has_value());
    }

    void test_no_layout_returns_nullopt() {
        formula::FormulaCursor cursor;

        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Right).has_value());
    }

    void test_invalid_current_index_returns_nullopt() {
        mfl::layout_elements layout;
        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 0,
            .size = mfl::points{12.0},
            .x = mfl::points{0.0},
            .y = mfl::points{0.0},
            .advance = mfl::points{6.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{2.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Индекс 999 — за пределами массива
        QVERIFY(!cursor.findGlyphInDirection(999, formula::NavigationDirection::Right).has_value());
    }

    void test_overlapping_glyphs_at_same_position() {
        // Два глифа с одинаковыми координатами (перекрывающиеся)
        mfl::layout_elements layout;

        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 0,
            .size = mfl::points{12.0},
            .x = mfl::points{10.0},
            .y = mfl::points{5.0},
            .advance = mfl::points{6.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{2.0},
        });

        layout.glyphs.push_back({
            .family = mfl::font_family::roman,
            .index = 1,
            .size = mfl::points{12.0},
            .x = mfl::points{10.0},  // тот же x
            .y = mfl::points{5.0},   // тот же y
            .advance = mfl::points{6.0},
            .height = mfl::points{8.0},
            .depth = mfl::points{2.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Центры совпадают → dx=0, dy=0 → ни одно направление не подходит
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Right).has_value());
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Left).has_value());
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Up).has_value());
        QVERIFY(!cursor.findGlyphInDirection(0, formula::NavigationDirection::Down).has_value());
    }

    // Part 8: Matrix tests (2x2)
    void test_matrix_navigation_all_directions() {
        // Матрица:
        //   a  b     (y=10)
        //   c  d     (y=-5)
        //  x=5 x=20

        mfl::layout_elements layout;

        // a: top-left
        layout.glyphs.push_back({
            .family = mfl::font_family::italic, .index = 1,
            .size = mfl::points{10.0},
            .x = mfl::points{5.0}, .y = mfl::points{10.0},
            .advance = mfl::points{5.0}, .height = mfl::points{7.0}, .depth = mfl::points{0.0},
        });
        // b: top-right
        layout.glyphs.push_back({
            .family = mfl::font_family::italic, .index = 2,
            .size = mfl::points{10.0},
            .x = mfl::points{20.0}, .y = mfl::points{10.0},
            .advance = mfl::points{5.0}, .height = mfl::points{7.0}, .depth = mfl::points{0.0},
        });
        // c: bottom-left
        layout.glyphs.push_back({
            .family = mfl::font_family::italic, .index = 3,
            .size = mfl::points{10.0},
            .x = mfl::points{5.0}, .y = mfl::points{-5.0},
            .advance = mfl::points{5.0}, .height = mfl::points{7.0}, .depth = mfl::points{0.0},
        });
        // d: bottom-right
        layout.glyphs.push_back({
            .family = mfl::font_family::italic, .index = 4,
            .size = mfl::points{10.0},
            .x = mfl::points{20.0}, .y = mfl::points{-5.0},
            .advance = mfl::points{5.0}, .height = mfl::points{7.0}, .depth = mfl::points{0.0},
        });

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Из a (0): Right → b (1)
        auto target = cursor.findGlyphInDirection(0, formula::NavigationDirection::Right);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 1u);

        // Из a (0): Down → c (2)
        target = cursor.findGlyphInDirection(0, formula::NavigationDirection::Down);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 2u);

        // Из d (3): Left → c (2)
        target = cursor.findGlyphInDirection(3, formula::NavigationDirection::Left);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 2u);

        // Из d (3): Up → b (1)
        target = cursor.findGlyphInDirection(3, formula::NavigationDirection::Up);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 1u);

        // Из b (1): Down → d (3)
        target = cursor.findGlyphInDirection(1, formula::NavigationDirection::Down);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 3u);

        // Из c (2): Right → d (3)
        target = cursor.findGlyphInDirection(2, formula::NavigationDirection::Right);
        QVERIFY(target.has_value());
        QCOMPARE(*target, 3u);
    }
};

QTEST_MAIN(TestNavigation)
#include "test_navigation.moc"
