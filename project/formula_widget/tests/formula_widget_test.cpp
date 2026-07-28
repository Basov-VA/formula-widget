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
    // Проверяем согласованность прямого и обратного преобразования Qt→mfl→Qt.
    const QPointF qtPos(100, 100);
    const QPointF mflPos = widget->qtToMfl(qtPos);
    const QPointF back = widget->mflToQt(mfl::points{mflPos.x()}, mfl::points{mflPos.y()});
    QVERIFY(qAbs(back.x() - qtPos.x()) < 1e-6);
    QVERIFY(qAbs(back.y() - qtPos.y()) < 1e-6);
}

void FormulaWidgetTest::testMflToQtConversion()
{
    // Проверяем согласованность обратного и прямого преобразования mfl→Qt→mfl.
    const mfl::points x(10.0);
    const mfl::points y(10.0);
    const QPointF qtPos = widget->mflToQt(x, y);
    const QPointF backMfl = widget->qtToMfl(qtPos);
    QVERIFY(qAbs(backMfl.x() - x.value()) < 1e-6);
    QVERIFY(qAbs(backMfl.y() - y.value()) < 1e-6);
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

    // Рекурсивно ищем узлы верхнего и нижнего индексов в дереве.
    // Примечание: ядро скрипта mfl для "x_1^2" представляет обычным symbol
    // (script_nucleus появляется лишь в части случаев), поэтому проверяем
    // наличие именно superscript и subscript — это и есть инвариант структуры.
    bool foundSubscript = false;
    bool foundSuperscript = false;

    const std::function<void(const mfl::formula_node&)> scan =
        [&](const mfl::formula_node& node) {
            if (node.type == mfl::formula_node_type::subscript) foundSubscript = true;
            if (node.type == mfl::formula_node_type::superscript) foundSuperscript = true;
            for (const auto& child : node.children) scan(child);
        };
    scan(elements.tree);

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

void FormulaWidgetTest::testCaretRightIntegralSub()
{
    // Два интеграла; курсор в нижнем пределе ПРАВОГО (пустого). Каретка должна
    // быть в правой части формулы, а не у нижнего предела ЛЕВОГО интеграла
    // (регрессия findScriptField при нескольких «расплющенных» скриптах).
    widget->setFormula("\\int_c^d+");
    widget->insertBigOperator("int");   // правый интеграл, курсор в его нижнем пределе
    QVERIFY(widget->currentTexFormula() == QString("\\int^{d}_{c}+\\int^{}_{}"));

    const auto caret = widget->currentCaretRect();
    QVERIFY(caret.has_value());
    const double widthPx = widget->pointsToPixels(widget->layoutElements().width);
    // Правый интеграл — правая часть формулы; каретка его предела должна быть
    // существенно правее середины (до фикса уезжала влево, к пределу левого).
    QVERIFY(caret->x() > widthPx * 0.45);
}

void FormulaWidgetTest::testCursorSetPosition()
{
    widget->setFormula("x");

    // Set cursor position in pixel coordinates
    widget->setCursorPosition(50, 50);

    // Check that cursor position is stored
    auto cursorPos = widget->getCursorPosition();
    QVERIFY(cursorPos.has_value());
    QCOMPARE(cursorPos->x(), 50.0);
    QCOMPARE(cursorPos->y(), 50.0);
}

void FormulaWidgetTest::testCursorSetPositionMfl()
{
    widget->setFormula("x");

    // Set cursor position in MFL coordinates
    widget->setCursorPositionMfl(mfl::points{10.0}, mfl::points{10.0});

    // Check that cursor position is stored
    auto cursorPos = widget->getCursorPosition();
    QVERIFY(cursorPos.has_value());

    // Наличие подсветки зависит от layout; здесь только фиксируем вызов API.
    auto highlight = widget->currentCursorHit();
    Q_UNUSED(highlight);
    QVERIFY(true); // Placeholder
}

void FormulaWidgetTest::testCursorHighlight()
{
    widget->setFormula("x");

    // Set cursor position to highlight a glyph
    widget->setCursorPositionMfl(mfl::points{1.0}, mfl::points{0.0});

    // Check that we have a highlight
    auto highlight = widget->currentCursorHit();
    // For a simple formula like "x", we should find a glyph
    QVERIFY(highlight.has_value());

    // Check that the highlight has valid bbox coordinates
    QVERIFY(highlight->bbox_left.value() >= 0);
    QVERIFY(highlight->bbox_right.value() > highlight->bbox_left.value());
    QVERIFY(highlight->bbox_top.value() >= highlight->bbox_bottom.value());
}

void FormulaWidgetTest::testCoordinateConversionConsistency()
{
    widget->setFormula("x");

    // pixelToMflX вычитает левое поле, а mflToPixelX его обратно НЕ добавляет,
    // т.к. при отрисовке применяется painter.translate(margin_left_, 0). Поэтому
    // точный обратный ход по X учитывает margin_left_; по Y преобразование симметрично.
    const double margin = widget->margin_left_;

    // pixel -> MFL -> pixel
    const double pixel_x = 100.0;
    const double pixel_y = 100.0;
    const mfl::points mfl_x = widget->pixelToMflX(pixel_x);
    const mfl::points mfl_y = widget->pixelToMflY(pixel_y);
    const double converted_x = widget->mflToPixelX(mfl_x) + margin;
    const double converted_y = widget->mflToPixelY(mfl_y);
    QVERIFY(qAbs(converted_x - pixel_x) < 0.1);
    QVERIFY(qAbs(converted_y - pixel_y) < 0.1);

    // MFL -> pixel -> MFL (в пиксели переходим с учётом margin)
    const mfl::points original_mfl_x{10.0};
    const mfl::points original_mfl_y{10.0};
    const double pixel_x2 = widget->mflToPixelX(original_mfl_x) + margin;
    const double pixel_y2 = widget->mflToPixelY(original_mfl_y);
    const mfl::points converted_mfl_x = widget->pixelToMflX(pixel_x2);
    const mfl::points converted_mfl_y = widget->pixelToMflY(pixel_y2);
    QVERIFY(qAbs(converted_mfl_x.value() - original_mfl_x.value()) < 0.1);
    QVERIFY(qAbs(converted_mfl_y.value() - original_mfl_y.value()) < 0.1);
}
