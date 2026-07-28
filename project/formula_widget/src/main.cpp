#include "formula_widget.hpp"
#include "math_ast.hpp"
#include "math_semantics.hpp"
#include "cas_bridge.hpp"

#include <QApplication>
#include <QDir>
#include <QPixmap>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QTextEdit>
#include <QSplitter>
#include <QGridLayout>
#include <QToolTip>
#include <QTimer>
#include <QTabWidget>
#include <QFrame>

#include <memory>
#include <functional>
#include <vector>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow()
    {
        setupUI();
        setupConnections();

        QString initial = R"(\frac{1}{2\pi i} \int_\gamma \frac{f(z)}{z-a} \, dz)";
        if (qEnvironmentVariableIsSet("FW_FORMULA"))
            initial = qEnvironmentVariable("FW_FORMULA");
        formulaInput_->setText(initial);
        fontSizeSpinBox_->setValue(28);

        formulaWidget_->setFormula(formulaInput_->text());

        if (qEnvironmentVariableIsSet("FW_CAS")) {
            const QString op = qEnvironmentVariable("FW_CAS");
            QTimer::singleShot(300, this, [this, op] { runCas(op); });
        }
    }

private:
    void setupUI()
    {

        auto* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        auto* mainLayout = new QVBoxLayout(centralWidget);

        auto* controlsWidget = new QWidget;
        auto* controlsLayout = new QVBoxLayout(controlsWidget);

        auto* formulaLayout = new QHBoxLayout;
        formulaLayout->addWidget(new QLabel("Formula:"));
        formulaInput_ = new QLineEdit;
        formulaLayout->addWidget(formulaInput_);

        updateButton_ = new QPushButton("Update");
        formulaLayout->addWidget(updateButton_);

        auto* showTreeButton = new QPushButton("Show Tree Structure");
        formulaLayout->addWidget(showTreeButton);

        controlsLayout->addLayout(formulaLayout);

        auto* settingsLayout = new QHBoxLayout;

        auto* fontSizeLayout = new QHBoxLayout;
        fontSizeLayout->addWidget(new QLabel("Font Size:"));
        fontSizeSpinBox_ = new QSpinBox;
        fontSizeSpinBox_->setRange(8, 72);
        fontSizeSpinBox_->setValue(12);
        fontSizeLayout->addWidget(fontSizeSpinBox_);
        settingsLayout->addLayout(fontSizeLayout);

        debugCheckBox_ = new QCheckBox("Debug Bounding Boxes");
        debugCheckBox_->setChecked(true);
        settingsLayout->addWidget(debugCheckBox_);

        settingsLayout->addStretch();

        controlsLayout->addLayout(settingsLayout);

        auto* cursorLayout = new QHBoxLayout;
        cursorLayout->addWidget(new QLabel("Cursor X:"));
        cursorXSpinBox_ = new QDoubleSpinBox;
        cursorXSpinBox_->setRange(-1000, 1000);
        cursorXSpinBox_->setValue(0);
        cursorLayout->addWidget(cursorXSpinBox_);

        cursorLayout->addWidget(new QLabel("Cursor Y:"));
        cursorYSpinBox_ = new QDoubleSpinBox;
        cursorYSpinBox_->setRange(-1000, 1000);
        cursorYSpinBox_->setValue(0);
        cursorLayout->addWidget(cursorYSpinBox_);

        findGlyphButton_ = new QPushButton("Find Nearest Glyph");
        cursorLayout->addWidget(findGlyphButton_);

        cursorInfoLabel_ = new QLabel("No glyph selected");
        cursorLayout->addWidget(cursorInfoLabel_);

        cursorLayout->addStretch();

        controlsLayout->addLayout(cursorLayout);

        mainLayout->addWidget(controlsWidget);

        auto* splitter = new QSplitter(Qt::Horizontal);

        auto* scrollArea = new QScrollArea;
        formulaWidget_ = new FormulaWidget;
        formulaWidget_->setMinimumSize(200, 150);
        scrollArea->setWidget(formulaWidget_);
        scrollArea->setWidgetResizable(true);
        scrollArea->setMinimumWidth(150);

        treeView_ = new QTextEdit;
        treeView_->setReadOnly(true);
        treeView_->setMinimumWidth(0);
        treeView_->setFont(QFont("Courier New", 10));

        auto* paletteScroll = new QScrollArea;
        paletteScroll->setWidget(createPalette());
        paletteScroll->setWidgetResizable(true);
        paletteScroll->setMinimumWidth(120);
        paletteScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        splitter->addWidget(scrollArea);
        splitter->addWidget(treeView_);
        splitter->addWidget(paletteScroll);

        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 0);
        splitter->setStretchFactor(2, 0);
        splitter->setChildrenCollapsible(true);
        splitter->setHandleWidth(6);
        splitter->setSizes(QList<int>() << 720 << 130 << 360);

        mainLayout->addWidget(splitter);

        mainLayout->addWidget(createComputePanel());

        connect(showTreeButton, &QPushButton::clicked, this, &MainWindow::showTreeStructure);

        setWindowTitle("Math Formula Widget Demo");
        resize(1280, 640);
    }

    QWidget* createPalette()
    {
        auto* tabs = new QTabWidget;
        tabs->setFocusPolicy(Qt::NoFocus);
        tabs->setDocumentMode(true);

        auto makeButton = [this](const QString& label, const QString& tip,
                                 std::function<void()> action) {
            auto* btn = new QPushButton(label);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setFixedSize(46, 34);
            QFont f = btn->font();
            f.setPointSize(15);
            btn->setFont(f);
            if (!tip.isEmpty()) btn->setToolTip(tip);
            connect(btn, &QPushButton::clicked, this, std::move(action));
            return btn;
        };

        auto tabScroll = [](QWidget* content) {
            auto* sc = new QScrollArea;
            sc->setWidget(content);
            sc->setWidgetResizable(true);
            sc->setFrameShape(QFrame::NoFrame);
            sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            return sc;
        };

        auto symbolTab = [&](const std::vector<std::pair<QString, QString>>& items, int cols) {
            auto* w = new QWidget;
            auto* g = new QGridLayout(w);
            g->setSpacing(5);
            g->setContentsMargins(8, 8, 8, 8);
            g->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            for (std::size_t i = 0; i < items.size(); ++i) {
                const QString cmd = items[i].second;
                auto* b = makeButton(items[i].first, "\\" + cmd,
                                     [this, cmd] { formulaWidget_->insertSymbol(cmd); });
                g->addWidget(b, static_cast<int>(i) / cols, static_cast<int>(i) % cols);
            }
            return tabScroll(w);
        };

        {
            auto* w = new QWidget;
            auto* g = new QGridLayout(w);
            g->setSpacing(5);
            g->setContentsMargins(8, 8, 8, 8);
            g->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            int i = 0;
            const int cols = 4;
            auto place = [&](QPushButton* b) { g->addWidget(b, i / cols, i % cols); ++i; };
            place(makeButton("a/b", "Дробь  ( / )",              [this] { formulaWidget_->insertFraction(); }));
            place(makeButton("√",   "Корень  (Ctrl+R)",          [this] { formulaWidget_->insertSqrt(); }));
            place(makeButton("x²",  "Степень  ( ^ )",            [this] { formulaWidget_->insertSuperscript(); }));
            place(makeButton("x₂",  "Индекс  ( _ )",             [this] { formulaWidget_->insertSubscript(); }));
            place(makeButton("∫",   "Интеграл с пределами",       [this] { formulaWidget_->insertBigOperator("int"); }));
            place(makeButton("∬",   "Двойной интеграл",           [this] { formulaWidget_->insertBigOperator("iint"); }));
            place(makeButton("∭",   "Тройной интеграл",           [this] { formulaWidget_->insertBigOperator("iiint"); }));
            place(makeButton("∮",   "Контурный интеграл",         [this] { formulaWidget_->insertBigOperator("oint"); }));
            place(makeButton("∑",   "Сумма с пределами",          [this] { formulaWidget_->insertBigOperator("sum"); }));
            place(makeButton("∏",   "Произведение с пределами",   [this] { formulaWidget_->insertBigOperator("prod"); }));
            place(makeButton("(▯)", "Скобки",                     [this] { formulaWidget_->insertBracketPair('(', ')'); }));
            place(makeButton("[▯]", "Квадратные скобки",          [this] { formulaWidget_->insertBracketPair('[', ']'); }));
            place(makeButton("|▯|", "Модуль",                     [this] { formulaWidget_->insertBracketPair('|', '|'); }));

            place(makeButton("x̂",   "Крышка  \\hat",              [this] { formulaWidget_->insertAccent("hat"); }));
            place(makeButton("x⃗",   "Вектор  \\vec",              [this] { formulaWidget_->insertAccent("vec"); }));
            place(makeButton("x̄",   "Черта  \\bar",               [this] { formulaWidget_->insertAccent("bar"); }));
            place(makeButton("x̃",   "Тильда  \\tilde",            [this] { formulaWidget_->insertAccent("tilde"); }));
            place(makeButton("‾x‾", "Верхняя черта  \\overline",  [this] { formulaWidget_->insertAccent("overline"); }));
            place(makeButton("_x_", "Нижняя черта  \\underline",  [this] { formulaWidget_->insertAccent("underline"); }));

            place(makeButton("(₂₂)", "Матрица 2×2 в скобках",     [this] { formulaWidget_->insertMatrix(2, 2, '(', ')'); }));
            place(makeButton("(₃₃)", "Матрица 3×3 в скобках",     [this] { formulaWidget_->insertMatrix(3, 3, '(', ')'); }));
            place(makeButton("[₂₂]", "Матрица 2×2 в [ ]",         [this] { formulaWidget_->insertMatrix(2, 2, '[', ']'); }));
            place(makeButton("(⋮)",  "Вектор-столбец 2×1",        [this] { formulaWidget_->insertMatrix(2, 1, '(', ')'); }));
            place(makeButton("{…",   "Система уравнений (cases)",  [this] { formulaWidget_->insertMatrix(2, 1, '{', 0); }));
            tabs->addTab(tabScroll(w), "Шаблоны");
        }

        {
            auto* w = new QWidget;
            auto* g = new QGridLayout(w);
            g->setSpacing(5);
            g->setContentsMargins(8, 8, 8, 8);
            g->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            int i = 0;
            const int cols = 4;
            auto place = [&](QPushButton* b) { g->addWidget(b, i / cols, i % cols); ++i; };
            for (const char* fn : {"sin", "cos", "tan", "cot", "sec", "csc",
                                   "ln", "log", "exp", "arcsin", "arccos", "arctan"}) {
                const QString f = QString::fromLatin1(fn);
                place(makeButton(f, "\\" + f + "( )", [this, f] { formulaWidget_->insertFunction(f); }));
            }
            place(makeButton("lim", "\\lim", [this] { formulaWidget_->insertSymbol("lim"); }));
            tabs->addTab(tabScroll(w), "Функции");
        }

        tabs->addTab(symbolTab({
            {"±", "pm"}, {"∓", "mp"}, {"×", "times"}, {"⋅", "cdot"}, {"÷", "div"}, {"∗", "ast"},
            {"≤", "leq"}, {"≥", "geq"}, {"≠", "neq"}, {"≈", "approx"}, {"≡", "equiv"}, {"∼", "sim"},
            {"∞", "infty"}, {"∂", "partial"}, {"∇", "nabla"}, {"∀", "forall"}, {"∃", "exists"}, {"∝", "propto"},
            {"∈", "in"}, {"∉", "notin"}, {"⊂", "subset"}, {"∪", "cup"}, {"∩", "cap"},
            {"→", "to"}, {"←", "leftarrow"}, {"⇒", "Rightarrow"}, {"↦", "mapsto"}, {"⋆", "star"},
        }, 6), "Знаки");

        tabs->addTab(symbolTab({
            {"α", "alpha"}, {"β", "beta"}, {"γ", "gamma"}, {"δ", "delta"}, {"ε", "varepsilon"}, {"ζ", "zeta"},
            {"η", "eta"}, {"θ", "theta"}, {"κ", "kappa"}, {"λ", "lambda"}, {"μ", "mu"}, {"ν", "nu"},
            {"ξ", "xi"}, {"π", "pi"}, {"ρ", "rho"}, {"σ", "sigma"}, {"τ", "tau"}, {"φ", "varphi"},
            {"χ", "chi"}, {"ψ", "psi"}, {"ω", "omega"},
            {"Γ", "Gamma"}, {"Δ", "Delta"}, {"Θ", "Theta"}, {"Λ", "Lambda"},
            {"Σ", "Sigma"}, {"Φ", "Phi"}, {"Ω", "Omega"},
        }, 6), "Греческие");

        return tabs;
    }

    QWidget* createComputePanel()
    {
        auto* group = new QGroupBox("Вычисления (SymPy)");
        auto* v = new QVBoxLayout(group);

        auto* btnRow = new QHBoxLayout;
        auto addBtn = [&](const QString& label, const QString& op, const QString& tip) {
            auto* b = new QPushButton(label);
            b->setToolTip(tip);
            b->setFocusPolicy(Qt::NoFocus);
            connect(b, &QPushButton::clicked, this, [this, op] { runCas(op); });
            btnRow->addWidget(b);
            return b;
        };
        addBtn("=  Вычислить", "eval",      "Вычислить/упростить (считает интегралы и суммы)");
        addBtn("Упростить",    "simplify",  "Упростить выражение");
        addBtn("d/dx",         "diff",      "Производная по главной переменной");
        addBtn("∫ dx",         "integrate", "Первообразная по главной переменной");
        addBtn("≈  Число",     "num",       "Численное значение");
        addBtn("График",       "plot",      "Построить график функции одной переменной");
        btnRow->addStretch();
        v->addLayout(btnRow);

        auto* resRow = new QHBoxLayout;
        resultWidget_ = new FormulaWidget;
        resultWidget_->setMinimumSize(200, 70);
        resultWidget_->setFontSize(24);
        resultWidget_->setDebugDrawBBoxes(false);
        resultWidget_->setCursorHighlightEnabled(false);
        auto* resScroll = new QScrollArea;
        resScroll->setWidget(resultWidget_);
        resScroll->setWidgetResizable(true);
        resScroll->setMinimumHeight(90);
        resRow->addWidget(resScroll, 1);

        plotLabel_ = new QLabel;
        plotLabel_->setAlignment(Qt::AlignCenter);
        plotLabel_->setMinimumSize(300, 90);
        plotLabel_->setVisible(false);
        resRow->addWidget(plotLabel_);
        v->addLayout(resRow);

        resultText_ = new QLabel(formula::casAvailable()
            ? "Введите формулу и нажмите операцию."
            : "SymPy недоступен: установите python3 + sympy (pip install sympy).");
        resultText_->setWordWrap(true);
        resultText_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(resultText_);

        return group;
    }

    void setupConnections()
    {
        connect(updateButton_, &QPushButton::clicked, this, &MainWindow::updateFormula);
        connect(formulaInput_, &QLineEdit::returnPressed, this, &MainWindow::updateFormula);
        connect(fontSizeSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &MainWindow::updateFontSize);
        connect(debugCheckBox_, &QCheckBox::toggled,
                formulaWidget_, &FormulaWidget::setDebugDrawBBoxes);

        connect(findGlyphButton_, &QPushButton::clicked, this, &MainWindow::findNearestGlyph);
        connect(formulaWidget_, &FormulaWidget::cursorGlyphChanged, this, &MainWindow::onCursorGlyphChanged);

        connect(cursorXSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::findNearestGlyph);
        connect(cursorYSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::findNearestGlyph);

        connect(formulaWidget_, &FormulaWidget::formulaChanged, this, [this]() {
            const QString new_tex = formulaWidget_->currentTexFormula();
            if (formulaInput_->text() != new_tex) {
                formulaInput_->blockSignals(true);
                formulaInput_->setText(new_tex);
                formulaInput_->blockSignals(false);
            }
        });
    }

private slots:
    void updateFormula()
    {
        formulaWidget_->setFormula(formulaInput_->text());
    }

    void updateFontSize(int size)
    {
        formulaWidget_->setFontSize(size);
    }

    void runCas(const QString& op)
    {
        const std::string tex = formulaWidget_->currentTexFormula().toStdString();
        const auto sem = formula::toSympy(formula::parseTex(tex));
        if (!sem.ok) {
            resultText_->setText("Не удалось разобрать формулу: " +
                                 QString::fromStdString(sem.error));
            return;
        }
        const QString expr = QString::fromStdString(sem.sympy);
        const QString var = QString::fromStdString(sem.mainVar);

        if (op == "plot") {
            const QString png = QDir::tempPath() + "/fw_plot.png";
            const auto r = formula::casPlot(expr, var, png);
            if (r.ok) {
                QPixmap pm(png);
                plotLabel_->setPixmap(pm.scaledToHeight(220, Qt::SmoothTransformation));
                plotLabel_->setVisible(true);
                resultText_->setText("График: y = f(" + var + "),  выражение: " + expr);
            } else {
                resultText_->setText("График не построен: " + r.error);
            }
            return;
        }

        const auto r = formula::casCompute(expr, op, var);
        if (r.ok) {
            plotLabel_->setVisible(false);
            resultWidget_->setFormula(r.latex);
            QString prefix = (op == "diff") ? ("d/d" + var + " = ")
                           : (op == "integrate") ? ("∫ … d" + var + " = ")
                           : "= ";
            resultText_->setText(prefix + r.text + "        [SymPy: " + expr + "]");
        } else {
            resultText_->setText("Ошибка вычисления: " + r.error);
        }
    }

private slots:
    void findNearestGlyph()
    {
        double x = cursorXSpinBox_->value();
        double y = cursorYSpinBox_->value();
        formulaWidget_->setCursorPositionMfl(mfl::points{x}, mfl::points{y});
    }

    void onCursorGlyphChanged(std::size_t glyph_index)
    {

        const auto& elements = formulaWidget_->layoutElements();

        if (glyph_index < elements.glyphs.size()) {
            const auto& glyph = elements.glyphs[glyph_index];

            QString info = QString("Glyph #%1: family=%2, index=%3, x=%4, y=%5, adv=%6")
                              .arg(glyph_index)
                              .arg(static_cast<int>(glyph.family))
                              .arg(glyph.index)
                              .arg(glyph.x.value())
                              .arg(glyph.y.value())
                              .arg(glyph.advance.value());

            cursorInfoLabel_->setText(info);

            qDebug() << "Glyph info:" << info;
        } else {
            cursorInfoLabel_->setText(QString("Glyph #%1 (out of range)").arg(glyph_index));
        }
    }

private:
     QLineEdit* formulaInput_;
     QPushButton* updateButton_;
     QSpinBox* fontSizeSpinBox_;
     QCheckBox* debugCheckBox_;
     FormulaWidget* formulaWidget_;
     QTextEdit* treeView_;

     FormulaWidget* resultWidget_ = nullptr;
     QLabel* resultText_ = nullptr;
     QLabel* plotLabel_ = nullptr;

     QDoubleSpinBox* cursorXSpinBox_;
     QDoubleSpinBox* cursorYSpinBox_;
     QPushButton* findGlyphButton_;
     QLabel* cursorInfoLabel_;

 private slots:
     void showTreeStructure()
     {

         const auto& elements = formulaWidget_->layoutElements();

         QString treeText = "Formula Tree Structure:\n\n";
         treeText += generateTreeText(elements.tree, 0);

         treeView_->setPlainText(treeText);
     }

     QString generateTreeText(const mfl::formula_node& node, int depth)
     {
         QString indent(depth * 2, ' ');
         QString text = indent + getNodeTypeName(node.type);

         if (node.bbox_width.value() > 0 || node.bbox_height.value() > 0) {
             text += QString(" [x=%1, y=%2, w=%3, h=%4]")
                         .arg(node.bbox_x.value(), 0, 'f', 1)
                         .arg(node.bbox_y.value(), 0, 'f', 1)
                         .arg(node.bbox_width.value(), 0, 'f', 1)
                         .arg(node.bbox_height.value(), 0, 'f', 1);
         }

         if (!node.glyph_indices.empty()) {
             text += " glyphs: [";
             for (size_t i = 0; i < node.glyph_indices.size(); ++i) {
                 if (i > 0) text += ", ";
                 text += QString::number(node.glyph_indices[i]);
             }
             text += "]";
         }

         text += "\n";

         for (const auto& child : node.children) {
             text += generateTreeText(child, depth + 1);
         }

         return text;
     }

     QString getNodeTypeName(mfl::formula_node_type type)
     {
         switch (type) {
             case mfl::formula_node_type::root: return "Root";
             case mfl::formula_node_type::symbol: return "Symbol";
             case mfl::formula_node_type::fraction: return "Fraction";
             case mfl::formula_node_type::numerator: return "Numerator";
             case mfl::formula_node_type::denominator: return "Denominator";
             case mfl::formula_node_type::radical: return "Radical";
             case mfl::formula_node_type::radicand: return "Radicand";
             case mfl::formula_node_type::degree: return "Degree";
             case mfl::formula_node_type::superscript: return "Superscript";
             case mfl::formula_node_type::subscript: return "Subscript";
             case mfl::formula_node_type::script_nucleus: return "ScriptNucleus";
             case mfl::formula_node_type::group: return "Group";
             case mfl::formula_node_type::overline: return "Overline";
             case mfl::formula_node_type::underline: return "Underline";
             case mfl::formula_node_type::accent: return "Accent";
             case mfl::formula_node_type::left_right: return "LeftRight";
             case mfl::formula_node_type::matrix: return "Matrix";
             case mfl::formula_node_type::matrix_cell: return "MatrixCell";
             default: return "Unknown";
         }
     }
};

static const char* kAppStyle = R"(
QWidget {
    background: #16273D;
    color: #D7E3F0;
    font-family: 'Segoe UI', 'Helvetica Neue', Arial, sans-serif;
    font-size: 13px;
}
QMainWindow { background: #122033; }
QLabel { color: #AFC6DE; background: transparent; }
QCheckBox { color: #C6D6E6; background: transparent; spacing: 6px; }
QCheckBox::indicator {
    width: 15px; height: 15px; border: 1px solid #3D6089; border-radius: 3px; background: #0E1B2C;
}
QCheckBox::indicator:checked { background: #2E6FB8; border-color: #4A8BD0; }

QGroupBox {
    background: #1C324D;
    border: 1px solid #2E4A6C;
    border-radius: 8px;
    margin-top: 16px;
    padding: 8px 6px 6px 6px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 2px 8px;
    color: #8FB4DC;
    background: transparent;
}

QPushButton {
    background: #2C568C;
    color: #F1F7FD;
    border: 1px solid #3A6BA0;
    border-radius: 6px;
    padding: 6px 12px;
    font-weight: 500;
}
QPushButton:hover  { background: #3670AE; border-color: #4E8AC6; }
QPushButton:pressed { background: #234873; }

QLineEdit, QSpinBox, QDoubleSpinBox {
    background: #0E1B2C;
    color: #EAF2FB;
    border: 1px solid #2E4A6C;
    border-radius: 5px;
    padding: 4px 6px;
    selection-background-color: #2E6FB8;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #4A8BD0; }

QTextEdit {
    background: #0E1B2C;
    color: #C6D6E6;
    border: 1px solid #2E4A6C;
    border-radius: 8px;
}

QScrollArea { border: 1px solid #2E4A6C; border-radius: 8px; background: #0E1B2C; }

QSplitter::handle { background: #24405F; }
QSplitter::handle:hover { background: #34618F; }

QScrollBar:vertical   { background: #122033; width: 12px; margin: 0; border-radius: 6px; }
QScrollBar:horizontal { background: #122033; height: 12px; margin: 0; border-radius: 6px; }
QScrollBar::handle { background: #34557C; border-radius: 6px; min-height: 24px; min-width: 24px; }
QScrollBar::handle:hover { background: #4373A8; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QToolTip { background: #0E1B2C; color: #EAF2FB; border: 1px solid #3A6BA0; padding: 4px 6px; }

QTabWidget::pane { border: 1px solid #2E4A6C; border-radius: 8px; background: #16273D; }
QTabBar { background: transparent; }
QTabBar::tab {
    background: #1B3350; color: #9FC0E4; padding: 6px 14px; margin-right: 2px;
    border: 1px solid #2E4A6C; border-bottom: none;
    border-top-left-radius: 6px; border-top-right-radius: 6px;
}
QTabBar::tab:selected { background: #2C568C; color: #FFFFFF; }
QTabBar::tab:hover:!selected { background: #24466B; }
)";

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyleSheet(kAppStyle);

    MainWindow window;
    window.show();

    if (qEnvironmentVariableIsSet("FW_SCREENSHOT")) {
        const QString path = qEnvironmentVariable("FW_SCREENSHOT");
        if (qEnvironmentVariableIsSet("FW_SIZE")) {
            const auto p = qEnvironmentVariable("FW_SIZE").split('x');
            if (p.size() == 2) window.resize(p[0].toInt(), p[1].toInt());
        }
        QTimer::singleShot(500, [&window, path]() {
            window.grab().save(path);
            QApplication::quit();
        });
    }

    return app.exec();
}

#include "main.moc"
