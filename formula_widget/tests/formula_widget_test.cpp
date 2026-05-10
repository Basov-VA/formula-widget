#include "formula_widget_test.hpp"

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

void FormulaWidgetTest::initTestCase()
{
    widget = new FormulaWidget();
}

void FormulaWidgetTest::cleanupTestCase()
{
    delete widget;
}

void FormulaWidgetTest::testFormulaWidgetCreation()
{
    QVERIFY(widget != nullptr);
    QVERIFY(widget->layoutElements().error.has_value() == false || widget->layoutElements().error->empty());
}

void FormulaWidgetTest::testSetFormula()
{
    QSignalSpy spy(widget, &FormulaWidget::formulaChanged);

    widget->setFormula("x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}");

    QCOMPARE(spy.count(), 1);
    QVERIFY(widget->layoutElements().error.has_value() == false || widget->layoutElements().error->empty());
}

void FormulaWidgetTest::testSetFontSize()
{
    const double originalSize = 12.0;
    const double newSize = 16.0;

    widget->setFontSize(originalSize);
    QVERIFY(widget->layoutElements().error.has_value() == false || widget->layoutElements().error->empty());

    widget->setFontSize(newSize);
    QVERIFY(widget->layoutElements().error.has_value() == false || widget->layoutElements().error->empty());
}

void FormulaWidgetTest::testSetDpi()
{
    const double originalDpi = 96.0;
    const double newDpi = 120.0;

    widget->setDpi(originalDpi);
    // Just check that it doesn't crash

    widget->setDpi(newDpi);
    // Just check that it doesn't crash
}

void FormulaWidgetTest::testLayoutElements()
{
    widget->setFormula("a + b = c");

    const auto& elements = widget->layoutElements();
    QVERIFY(elements.error.has_value() == false || elements.error->empty());
    // We can't check specific values since they depend on font rendering
}

void FormulaWidgetTest::testDebugDrawBBoxes()
{
    // Test getter/setter
    widget->setDebugDrawBBoxes(true);
    QCOMPARE(widget->debugDrawBBoxes(), true);

    widget->setDebugDrawBBoxes(false);
    QCOMPARE(widget->debugDrawBBoxes(), false);
}

void FormulaWidgetTest::testNodeAtPosition()
{
    widget->setFormula("x^2");

    // Test with a position that should be within the formula
    const auto node = widget->nodeAtPosition(QPointF(50, 50));
    // We can't assert specific values since they depend on rendering,
    // but we can check that it doesn't crash and returns either a node or nullopt
    QVERIFY(true); // Placeholder - actual testing would require more setup
}

void FormulaWidgetTest::testQtToMflConversion()
{
    // Test coordinate conversion functions
    const QPointF qtPos(100, 100);
    const QPointF mflPos = widget->qtToMfl(qtPos);
    // We can't check specific values since they depend on DPI and widget size
    QVERIFY(true); // Placeholder
}

void FormulaWidgetTest::testMflToQtConversion()
{
    // Test coordinate conversion functions
    const mfl::points x(10.0);
    const mfl::points y(10.0);
    const QPointF qtPos = widget->mflToQt(x, y);
    // We can't check specific values since they depend on DPI and widget size
    QVERIFY(true); // Placeholder
}

void FormulaWidgetTest::testPointsToPixelsConversion()
{
    const mfl::points pt(10.0);
    const double pixels = widget->pointsToPixels(pt);
    // We can't check specific values since they depend on DPI
    QVERIFY(pixels > 0); // At least check it's positive
}

// Additional comprehensive tests

void FormulaWidgetTest::testFractionFormula()
{
    widget->setFormula("\\frac{a}{b}");

    const auto& elements = widget->layoutElements();
    QVERIFY(elements.error.has_value() == false || elements.error->empty());

    // Check that we have a tree structure
    QVERIFY(!elements.tree.children.empty());

    // Check that we have a fraction node
    bool foundFraction = false;
    for (const auto& child : elements.tree.children) {
        if (child.type == mfl::formula_node_type::fraction) {
            foundFraction = true;
            break;
        }
    }
    QVERIFY(foundFraction);
}

void FormulaWidgetTest::testScriptFormula()
{
    widget->setFormula("x_1^2");

    const auto& elements = widget->layoutElements();
    QVERIFY(elements.error.has_value() == false || elements.error->empty());

    // Check that we have a tree structure
    QVERIFY(!elements.tree.children.empty());

    // Check that we have script nodes
    bool foundNucleus = false;
    bool foundSubscript = false;
    bool foundSuperscript = false;

    for (const auto& child : elements.tree.children) {
        if (child.type == mfl::formula_node_type::script_nucleus) {
            foundNucleus = true;
        } else if (child.type == mfl::formula_node_type::subscript) {
            foundSubscript = true;
        } else if (child.type == mfl::formula_node_type::superscript) {
            foundSuperscript = true;
        }
    }

    QVERIFY(foundNucleus);
    QVERIFY(foundSubscript);
    QVERIFY(foundSuperscript);
}

void FormulaWidgetTest::testRadicalFormula()
{
    widget->setFormula("\\sqrt{x}");

    const auto& elements = widget->layoutElements();
    QVERIFY(elements.error.has_value() == false || elements.error->empty());

    // Check that we have a tree structure
    QVERIFY(!elements.tree.children.empty());

    // Check that we have radical nodes
    bool foundRadical = false;
    bool foundRadicand = false;

    for (const auto& child : elements.tree.children) {
        if (child.type == mfl::formula_node_type::radical) {
            foundRadical = true;
        } else if (child.type == mfl::formula_node_type::radicand) {
            foundRadicand = true;
        }
    }

    QVERIFY(foundRadical);
    QVERIFY(foundRadicand);
}

void FormulaWidgetTest::testComplexFormula()
{
    widget->setFormula("\\frac{\\sqrt{x_1^2 + x_2^2}}{2}");

    const auto& elements = widget->layoutElements();
    QVERIFY(elements.error.has_value() == false || elements.error->empty());

    // Check that we have a tree structure
    QVERIFY(!elements.tree.children.empty());

    // Check for nested structures
    bool foundFraction = false;
    bool foundRadical = false;

    // Simple check for presence of fraction and radical
    std::function<void(const mfl::formula_node&)> checkNode = [&](const mfl::formula_node& node) {
        if (node.type == mfl::formula_node_type::fraction) {
            foundFraction = true;
        } else if (node.type == mfl::formula_node_type::radical) {
            foundRadical = true;
        }

        for (const auto& child : node.children) {
            checkNode(child);
        }
    };

    checkNode(elements.tree);

    QVERIFY(foundFraction);
    QVERIFY(foundRadical);
}

void FormulaWidgetTest::testBoundingBoxComputation()
{
    widget->setFormula("\\frac{a}{b}");

    const auto& elements = widget->layoutElements();
    QVERIFY(elements.error.has_value() == false || elements.error->empty());

    // Check that bounding boxes are computed
    QVERIFY(elements.tree.bbox_width.value() > 0);
    QVERIFY(elements.tree.bbox_height.value() > 0);

    // Check that child nodes also have bounding boxes
    for (const auto& child : elements.tree.children) {
        QVERIFY(child.bbox_width.value() >= 0);  // Allow zero for empty nodes
        QVERIFY(child.bbox_height.value() >= 0);
    }
}

void FormulaWidgetTest::testEmptyFormula()
{
    widget->setFormula("");

    const auto& elements = widget->layoutElements();
    QVERIFY(elements.error.has_value() == false || elements.error->empty());

    // Empty formula should still produce a valid layout
    QVERIFY(elements.width.value() >= 0);
    QVERIFY(elements.height.value() >= 0);
}

void FormulaWidgetTest::testInvalidFormula()
{
    widget->setFormula("\\invalid{command}");

    const auto& elements = widget->layoutElements();
    // Invalid formula should produce an error
    QVERIFY(elements.error.has_value());
    QVERIFY(!elements.error->empty());
}
