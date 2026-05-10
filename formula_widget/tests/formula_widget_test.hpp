#pragma once

#include <QtTest/QtTest>
#include <QWidget>

#include "../src/formula_widget.hpp"

class FormulaWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testFormulaWidgetCreation();
    void testSetFormula();
    void testSetFontSize();
    void testSetDpi();
    void testLayoutElements();
    void testDebugDrawBBoxes();
    void testNodeAtPosition();
    void testQtToMflConversion();
    void testMflToQtConversion();
    void testPointsToPixelsConversion();

    // Additional comprehensive tests
    void testFractionFormula();
    void testScriptFormula();
    void testRadicalFormula();
    void testComplexFormula();
    void testBoundingBoxComputation();
    void testEmptyFormula();
    void testInvalidFormula();

    // Cursor functionality tests
    void testCursorSetPosition();
    void testCursorSetPositionMfl();
    void testCursorHighlight();
    void testCoordinateConversionConsistency();

private:
    FormulaWidget* widget;
};
