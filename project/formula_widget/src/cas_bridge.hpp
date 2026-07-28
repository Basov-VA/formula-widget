#pragma once

#include <QString>

namespace formula {

    struct CasResult {
        bool ok = false;
        QString latex;
        QString text;
        QString error;
    };

    CasResult casCompute(const QString& sympyExpr, const QString& op, const QString& var);

    CasResult casPlot(const QString& sympyExpr, const QString& var, const QString& outPng);

    bool casAvailable();

}
