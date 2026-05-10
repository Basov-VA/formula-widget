#include "formula_widget.hpp"

#include <QApplication>
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

#include <memory>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow()
    {
        setupUI();
        setupConnections();

        // Set a default formula
        formulaInput_->setText(R"(\frac{1}{2\pi i} \int_\gamma \frac{f(z)}{z-a} \, dz)");
        fontSizeSpinBox_->setValue(20);

        // Initialize the formula widget with the default formula
        formulaWidget_->setFormula(formulaInput_->text());
    }

private:
    void setupUI()
    {
        // Create central widget and layout
        auto* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        auto* mainLayout = new QVBoxLayout(centralWidget);

        // Create formula input controls
        auto* controlsWidget = new QWidget;
        auto* controlsLayout = new QVBoxLayout(controlsWidget);

        auto* formulaLayout = new QHBoxLayout;
        formulaLayout->addWidget(new QLabel("Formula:"));
        formulaInput_ = new QLineEdit;
        formulaLayout->addWidget(formulaInput_);

        updateButton_ = new QPushButton("Update");
        formulaLayout->addWidget(updateButton_);

        // Add button to show tree structure
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
        debugCheckBox_->setChecked(true);  // Enable by default for debugging
        settingsLayout->addWidget(debugCheckBox_);

        settingsLayout->addStretch();

        controlsLayout->addLayout(settingsLayout);

        mainLayout->addWidget(controlsWidget);

        // Create splitter for formula widget and tree view
        auto* splitter = new QSplitter(Qt::Horizontal);

        // Create formula widget inside a scroll area
        auto* scrollArea = new QScrollArea;
        formulaWidget_ = new FormulaWidget;
        formulaWidget_->setMinimumSize(600, 400);
        scrollArea->setWidget(formulaWidget_);
        scrollArea->setWidgetResizable(true);

        // Create text area for tree structure
        treeView_ = new QTextEdit;
        treeView_->setReadOnly(true);
        treeView_->setMinimumWidth(300);
        treeView_->setFont(QFont("Courier New", 10));

        splitter->addWidget(scrollArea);
        splitter->addWidget(treeView_);
        splitter->setSizes(QList<int>() << 600 << 300);

        mainLayout->addWidget(splitter);

        // Connect the show tree button
        connect(showTreeButton, &QPushButton::clicked, this, &MainWindow::showTreeStructure);

        // Set window properties
        setWindowTitle("Math Formula Widget Demo");
        resize(1200, 600);
    }

    void setupConnections()
    {
        connect(updateButton_, &QPushButton::clicked, this, &MainWindow::updateFormula);
        connect(formulaInput_, &QLineEdit::returnPressed, this, &MainWindow::updateFormula);
        connect(fontSizeSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &MainWindow::updateFontSize);
        connect(debugCheckBox_, &QCheckBox::toggled,
                formulaWidget_, &FormulaWidget::setDebugDrawBBoxes);
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

private:
     QLineEdit* formulaInput_;
     QPushButton* updateButton_;
     QSpinBox* fontSizeSpinBox_;
     QCheckBox* debugCheckBox_;
     FormulaWidget* formulaWidget_;
     QTextEdit* treeView_;

 private slots:
     void showTreeStructure()
     {
         // Get the layout elements from the formula widget
         const auto& elements = formulaWidget_->layoutElements();

         // Generate tree structure as text
         QString treeText = "Formula Tree Structure:\n\n";
         treeText += generateTreeText(elements.tree, 0);

         // Display in the text area
         treeView_->setPlainText(treeText);
     }

     QString generateTreeText(const mfl::formula_node& node, int depth)
     {
         QString indent(depth * 2, ' ');
         QString text = indent + getNodeTypeName(node.type);

         // Add bounding box information
         if (node.bbox_width.value() > 0 || node.bbox_height.value() > 0) {
             text += QString(" [x=%1, y=%2, w=%3, h=%4]")
                         .arg(node.bbox_x.value(), 0, 'f', 1)
                         .arg(node.bbox_y.value(), 0, 'f', 1)
                         .arg(node.bbox_width.value(), 0, 'f', 1)
                         .arg(node.bbox_height.value(), 0, 'f', 1);
         }

         // Add glyph indices if any
         if (!node.glyph_indices.empty()) {
             text += " glyphs: [";
             for (size_t i = 0; i < node.glyph_indices.size(); ++i) {
                 if (i > 0) text += ", ";
                 text += QString::number(node.glyph_indices[i]);
             }
             text += "]";
         }

         text += "\n";

         // Recursively add children
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
