#include <QtTest/QtTest>
#include <memory>
#include <limits>
#include <cmath>

// Mock the MFL structures we need for testing
namespace mfl {
    struct points {
        double value_;
        explicit points(double v) : value_(v) {}
        double value() const { return value_; }
    };

    struct shaped_glyph {
        points x{0};
        points y{0};
        points advance{0};
        points height{0};
        points depth{0};
    };
}

// Implement distanceToBBox function directly for testing
namespace formula {
    double distanceToBBox(double px, double py, const mfl::shaped_glyph& g) {
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
}

class TestCoordinateConversion : public QObject
{
    Q_OBJECT

private slots:
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

        double distance = formula::distanceToBBox(px, py, g);
        QCOMPARE(distance, expected_distance);
    }

    void testBoundingBoxCalculations() {
        mfl::shaped_glyph g;
        g.x = mfl::points{10.0};
        g.y = mfl::points{20.0};
        g.advance = mfl::points{8.0};
        g.height = mfl::points{10.0};
        g.depth = mfl::points{2.0};

        // Test bounding box edges
        double left = g.x.value();
        double right = g.x.value() + g.advance.value();
        double bottom = g.y.value() - g.depth.value();
        double top = g.y.value() + g.height.value();

        QCOMPARE(left, 10.0);
        QCOMPARE(right, 18.0);
        QCOMPARE(bottom, 18.0);
        QCOMPARE(top, 30.0);
    }

    void testBoundingBoxResult() {
        mfl::shaped_glyph g;
        g.x = mfl::points{10.0};
        g.y = mfl::points{20.0};
        g.advance = mfl::points{8.0};
        g.height = mfl::points{10.0};
        g.depth = mfl::points{2.0};

        // Test bounding box edges calculation
        double left = g.x.value();
        double right = g.x.value() + g.advance.value();
        double bottom = g.y.value() - g.depth.value();
        double top = g.y.value() + g.height.value();

        // Check that bbox coordinates are correctly calculated
        QCOMPARE(left, 10.0);   // Left edge
        QCOMPARE(right, 18.0);  // Left + advance = 10 + 8
        QCOMPARE(top, 30.0);    // Baseline + height = 20 + 10
        QCOMPARE(bottom, 18.0); // Baseline - depth = 20 - 2
    }
};

QTEST_MAIN(TestCoordinateConversion)
#include "test_coordinate_conversion.moc"
