#include <QtTest/QtTest>
#include <QApplication>

// Include test classes
#include "formula_widget_test.hpp"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Run tests
    int status = 0;

    {
        FormulaWidgetTest test;
        status |= QTest::qExec(&test, argc, argv);
    }

    return status;
}
