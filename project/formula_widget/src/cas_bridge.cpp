#include "cas_bridge.hpp"

#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace formula {

    namespace {

        const char* kPyScript = R"PY(
import sys
op   = sys.argv[1]
expr = sys.argv[2]
var  = sys.argv[3] if len(sys.argv) > 3 else 'x'
out  = sys.argv[4] if len(sys.argv) > 4 else ''
try:
    import sympy as sp
    x = sp.Symbol(var)
    e = sp.sympify(expr)
    if op == 'plot':
        import numpy as np
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fn = sp.lambdify(x, e.doit(), 'numpy')
        xs = np.linspace(-10.0, 10.0, 800)
        ys = np.empty_like(xs)
        with np.errstate(all='ignore'):
            ys[:] = fn(xs)   # присваивание широковещательно: скаляр (константа) заполняет массив
        ys = np.asarray(ys, dtype=float)
        ys[~np.isfinite(ys)] = np.nan
        fig = plt.figure(figsize=(5.0, 3.3), dpi=120)
        ax = fig.add_subplot(111)
        ax.axhline(0, color='#888', lw=0.8)
        ax.axvline(0, color='#888', lw=0.8)
        ax.plot(xs, ys, color='#2C568C', lw=2)
        ax.grid(True, alpha=0.3)
        ax.set_title('y = ' + str(e.doit()))
        fig.tight_layout()
        fig.savefig(out)
        sys.stdout.write('OK\n')
    else:
        d = e.doit()
        if op == 'simplify':
            r = sp.simplify(d)
        elif op == 'num':
            r = sp.N(d)
        elif op == 'diff':
            r = sp.simplify(sp.diff(d, x))
        elif op == 'integrate':
            r = sp.integrate(d, x)
        else:  # eval
            r = sp.simplify(d)
        sys.stdout.write('OK\n')
        sys.stdout.write(sp.latex(r) + '\n')
        sys.stdout.write(str(r) + '\n')
except Exception as ex:
    sys.stdout.write('ERR\n')
    sys.stdout.write(str(ex) + '\n')
)PY";

        QString pythonExe() {
            for (const QString& cand : {QStringLiteral("python3"),
                                        QStringLiteral("/opt/homebrew/bin/python3"),
                                        QStringLiteral("/usr/local/bin/python3"),
                                        QStringLiteral("/usr/bin/python3")}) {
                if (cand.startsWith('/')) {
                    if (QProcess::execute(cand, {QStringLiteral("--version")}) == 0) return cand;
                } else {
                    const QString full = QStandardPaths::findExecutable(cand);
                    if (!full.isEmpty()) return full;
                }
            }
            return QStringLiteral("python3");
        }

        QString runPy(const QStringList& args, int timeoutMs = 15000) {
            QProcess p;
            QStringList full;
            full << QStringLiteral("-c") << QString::fromUtf8(kPyScript) << args;
            p.start(pythonExe(), full);
            if (!p.waitForStarted(5000)) return {};
            if (!p.waitForFinished(timeoutMs)) { p.kill(); return {}; }
            return QString::fromUtf8(p.readAllStandardOutput());
        }

    }

    bool casAvailable() {
        static int cached = -1;
        if (cached < 0) {
            const int rc = QProcess::execute(
                pythonExe(), {QStringLiteral("-c"), QStringLiteral("import sympy")});
            cached = (rc == 0) ? 1 : 0;
        }
        return cached == 1;
    }

    CasResult casCompute(const QString& sympyExpr, const QString& op, const QString& var) {
        CasResult r;
        if (!casAvailable()) { r.error = QStringLiteral("SymPy недоступен (нужен python3 + sympy)"); return r; }
        const QString out = runPy({op, sympyExpr, var});
        const QStringList lines = out.split('\n');
        if (lines.isEmpty() || lines[0].trimmed().isEmpty()) {
            r.error = QStringLiteral("нет ответа от CAS");
            return r;
        }
        if (lines[0].trimmed() == QLatin1String("OK")) {
            r.ok = true;
            r.latex = lines.value(1);
            r.text = lines.value(2);
        } else {
            r.error = lines.value(1, QStringLiteral("ошибка CAS"));
        }
        return r;
    }

    CasResult casPlot(const QString& sympyExpr, const QString& var, const QString& outPng) {
        CasResult r;
        if (!casAvailable()) { r.error = QStringLiteral("SymPy/matplotlib недоступны"); return r; }
        const QString out = runPy({QStringLiteral("plot"), sympyExpr, var, outPng}, 20000);
        const QStringList lines = out.split('\n');
        if (!lines.isEmpty() && lines[0].trimmed() == QLatin1String("OK")) {
            r.ok = true;
        } else {
            r.error = lines.value(1, QStringLiteral("не удалось построить график"));
        }
        return r;
    }

}
