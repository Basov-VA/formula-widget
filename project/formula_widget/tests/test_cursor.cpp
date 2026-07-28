#include "formula_cursor.hpp"
#include <QtTest/QtTest>
#include <memory>

// Helper function to create a simple layout_elements for testing
mfl::layout_elements createTestLayout() {
    mfl::layout_elements elements;

    // Create some test glyphs
    mfl::shaped_glyph g1;
    g1.family = mfl::font_family::roman;
    g1.index = 0;
    g1.size = mfl::points{12.0};
    g1.x = mfl::points{10.0};
    g1.y = mfl::points{20.0};
    g1.advance = mfl::points{8.0};
    g1.height = mfl::points{10.0};
    g1.depth = mfl::points{2.0};

    mfl::shaped_glyph g2;
    g2.family = mfl::font_family::roman;
    g2.index = 1;
    g2.size = mfl::points{12.0};
    g2.x = mfl::points{20.0};
    g2.y = mfl::points{20.0};
    g2.advance = mfl::points{6.0};
    g2.height = mfl::points{8.0};
    g2.depth = mfl::points{1.0};

    elements.glyphs.push_back(g1);
    elements.glyphs.push_back(g2);
    elements.width = mfl::points{30.0};
    elements.height = mfl::points{25.0};

    return elements;
}

class TestFormulaCursor : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        // Any setup needed before all tests
    }

    void cleanupTestCase() {
        // Any cleanup needed after all tests
    }

    void testDistanceToBBox_data() {
        QTest::addColumn<double>("px");
        QTest::addColumn<double>("py");
        QTest::addColumn<double>("expected_distance");

        // Test point inside bbox
        QTest::newRow("inside") << 12.0 << 22.0 << 0.0;

        // Test point outside bbox on the right
        QTest::newRow("right") << 20.0 << 22.0 << 2.0;

        // Test point outside bbox on the left
        QTest::newRow("left") << 5.0 << 22.0 << 5.0;

        // Test point outside bbox above
        QTest::newRow("above") << 12.0 << 35.0 << 5.0;

        // Test point outside bbox below
        QTest::newRow("below") << 12.0 << 10.0 << 8.0;
    }

    void testDistanceToBBox() {
        QFETCH(double, px);
        QFETCH(double, py);
        QFETCH(double, expected_distance);

        mfl::shaped_glyph g;
        g.x = mfl::points{10.0};
        g.y = mfl::points{20.0};
        g.advance = mfl::points{8.0};
        g.height = mfl::points{10.0};
        g.depth = mfl::points{2.0};

        double distance = formula::FormulaCursor::distanceToBBox(px, py, g);
        QCOMPARE(distance, expected_distance);
    }

    void testFindNearestGlyph() {
        auto layout = createTestLayout();
        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Test point near first glyph
        auto result = cursor.findNearestGlyph(mfl::points{12.0}, mfl::points{22.0});
        QVERIFY(result.has_value());
        QCOMPARE(result->glyph_index, static_cast<std::size_t>(0));
        QCOMPARE(result->distance, 0.0);

        // Test point near second glyph
        result = cursor.findNearestGlyph(mfl::points{22.0}, mfl::points{22.0});
        QVERIFY(result.has_value());
        QCOMPARE(result->glyph_index, static_cast<std::size_t>(1));

        // Test point far from both glyphs
        result = cursor.findNearestGlyph(mfl::points{50.0}, mfl::points{50.0});
        QVERIFY(result.has_value());
        // Should find the closest glyph (probably the second one)
    }

    void testSetPosition() {
        auto layout = createTestLayout();
        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Set position near first glyph
        cursor.setPosition(mfl::points{12.0}, mfl::points{22.0});

        // Check that we have a highlight
        QVERIFY(cursor.hasHighlight());

        auto highlight = cursor.currentHighlight();
        QVERIFY(highlight.has_value());
        QCOMPARE(highlight->glyph_index, static_cast<std::size_t>(0));
    }

    void testClearHighlight() {
        auto layout = createTestLayout();
        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Set position to create highlight
        cursor.setPosition(mfl::points{12.0}, mfl::points{22.0});
        QVERIFY(cursor.hasHighlight());

        // Clear highlight
        cursor.clearHighlight();
        QVERIFY(!cursor.hasHighlight());
    }

    void testEmptyLayout() {
        mfl::layout_elements empty_layout;
        formula::FormulaCursor cursor;
        cursor.setLayout(&empty_layout);

        // Try to find nearest glyph in empty layout
        auto result = cursor.findNearestGlyph(mfl::points{10.0}, mfl::points{10.0});
        QVERIFY(!result.has_value());
    }

    void testGlyphHitResult() {
        auto layout = createTestLayout();
        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        auto result = cursor.findNearestGlyph(mfl::points{12.0}, mfl::points{22.0});
        QVERIFY(result.has_value());

        // Check that bbox coordinates are correctly calculated
        QCOMPARE(result->bbox_left.value(), 10.0);
        QCOMPARE(result->bbox_right.value(), 18.0);  // 10 + 8
        QCOMPARE(result->bbox_top.value(), 30.0);    // 20 + 10
        QCOMPARE(result->bbox_bottom.value(), 18.0); // 20 - 2
    }

    // Test to verify bounding box positioning is correct
    void testBoundingBoxPositioning() {
        // Create a test layout with a single glyph
        mfl::layout_elements layout;
        mfl::shaped_glyph g;
        g.family = mfl::font_family::roman;
        g.index = 0;
        g.size = mfl::points{12.0};
        g.x = mfl::points{10.0};      // Left edge at x=10
        g.y = mfl::points{20.0};      // Baseline at y=20
        g.advance = mfl::points{8.0}; // Width = 8 points
        g.height = mfl::points{10.0}; // Height above baseline = 10 points
        g.depth = mfl::points{2.0};   // Depth below baseline = 2 points
        layout.glyphs.push_back(g);

        // Set layout dimensions
        layout.width = mfl::points{20.0};
        layout.height = mfl::points{25.0}; // Total height = 10 + 2 = 12 points above baseline

        formula::FormulaCursor cursor;
        cursor.setLayout(&layout);

        // Test point exactly at the center of the glyph
        auto result = cursor.findNearestGlyph(mfl::points{14.0}, mfl::points{20.0}); // x=14 (center), y=20 (baseline)
        QVERIFY(result.has_value());
        QCOMPARE(result->glyph_index, static_cast<std::size_t>(0));
        QCOMPARE(result->distance, 0.0); // Should be inside the bbox

        // Check that bbox coordinates are correctly calculated
        QCOMPARE(result->bbox_left.value(), 10.0);   // Left edge
        QCOMPARE(result->bbox_right.value(), 18.0);  // Left + advance = 10 + 8
        QCOMPARE(result->bbox_top.value(), 30.0);    // Baseline + height = 20 + 10
        QCOMPARE(result->bbox_bottom.value(), 18.0); // Baseline - depth = 20 - 2
    }

    // Test coordinate conversion consistency
    void testCoordinateConversion() {
        // This test would require a FormulaWidget instance to test the conversion functions
        // For now, we'll just verify the mathematical correctness of the conversion

        // Test the distance calculation with various points
        mfl::shaped_glyph g;
        g.x = mfl::points{10.0};
        g.y = mfl::points{20.0};
        g.advance = mfl::points{8.0};
        g.height = mfl::points{10.0};
        g.depth = mfl::points{2.0};

        // Point inside bbox
        double distance = formula::FormulaCursor::distanceToBBox(12.0, 22.0, g);
        QCOMPARE(distance, 0.0);

        // Point outside bbox - to the right
        distance = formula::FormulaCursor::distanceToBBox(20.0, 22.0, g);
        QCOMPARE(distance, 2.0); // 20 - 18 (right edge)

        // Point outside bbox - to the left
        distance = formula::FormulaCursor::distanceToBBox(5.0, 22.0, g);
        QCOMPARE(distance, 5.0); // 10 (left edge) - 5

        // Point outside bbox - above
        distance = formula::FormulaCursor::distanceToBBox(12.0, 35.0, g);
        QCOMPARE(distance, 5.0); // 35 - 30 (top edge)

        // Point outside bbox - below
        distance = formula::FormulaCursor::distanceToBBox(12.0, 10.0, g);
        QCOMPARE(distance, 8.0); // 18 (bottom edge) - 10
    }
};

QTEST_MAIN(TestFormulaCursor)
#include "test_cursor.moc"
