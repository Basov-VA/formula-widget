#pragma once

#include "ft_library.hpp"
#include "ft_font_face.hpp"

#include "mfl/layout.hpp"
#include "formula_cursor.hpp"
#include "ast_cursor.hpp"

#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QString>
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QKeyEvent>
#include <QTimer>
#include <QFocusEvent>
#include <optional>

#include <memory>

class FormulaWidgetTest;

class FormulaWidget : public QWidget
{
    Q_OBJECT

    friend class FormulaWidgetTest;

public:
    explicit FormulaWidget(QWidget* parent = nullptr);
    ~FormulaWidget() override;

    FormulaWidget(const FormulaWidget&) = delete;
    FormulaWidget& operator=(const FormulaWidget&) = delete;

    void setFormula(const QString& tex);
    void setFontSize(double pt);
    void setDpi(double dpi);

    [[nodiscard]] const mfl::layout_elements& layoutElements() const;

    [[nodiscard]] QString currentTexFormula() const;

    void setDebugDrawBBoxes(bool enable);
    [[nodiscard]] bool debugDrawBBoxes() const;

    [[nodiscard]] std::optional<mfl::formula_node> nodeAtPosition(const QPointF& pos) const;
    [[nodiscard]] QPointF qtToMfl(QPointF qt_pos) const;

    void setCursorPosition(double pixel_x, double pixel_y);
    void setCursorPositionMfl(mfl::points x, mfl::points y);
    void setCursorHighlightEnabled(bool enabled);
    std::optional<formula::glyph_hit_result> currentCursorHit() const;

    std::optional<QPointF> getCursorPosition() const;

    bool moveCursorLeft();
    bool moveCursorRight();
    bool moveCursorUp();
    bool moveCursorDown();

    void insertSymbol(const QString& command);

    void insertFunction(const QString& command);
    void insertFraction();
    void insertSqrt();
    void insertSuperscript();
    void insertSubscript();

    void insertBigOperator(const QString& command);
    void insertBracketPair(QChar open, QChar close);
    void insertAccent(const QString& command);

    void insertMatrix(int rows, int cols, char open = 0, char close = 0);

signals:
    void formulaChanged();
    void cursorGlyphChanged(std::size_t glyph_index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

    bool focusNextPrevChild(bool next) override;

private:
    void recalculateLayout();

    void applyAstEdit();

    void mapCaretFromAst();

    void onBlinkTimer();
    void startBlinking();
    void stopBlinking();
    void resetBlinking();
    bool isBlinkingEnabled() const;
    bool isCursorVisible() const;
    QRectF getCaretRect() const;

    std::optional<QRectF> currentCaretRect() const;

    std::optional<QRectF> glyphCaretRect(std::size_t glyph_index, bool after) const;

    std::optional<QRectF> structCaretRect(const formula::FieldContext& fc, bool after) const;

    std::optional<QRectF> emptyFieldCaretRect() const;

    std::optional<QRectF> selectionRect() const;

    void beginEdit(bool typing);

    [[nodiscard]] double pointsToPixels(mfl::points pt) const;
    [[nodiscard]] QPointF mflToQt(mfl::points x, mfl::points y) const;
    [[nodiscard]] mfl::points pixelToMflX(double pixel_x) const;
    [[nodiscard]] mfl::points pixelToMflY(double pixel_y) const;
    [[nodiscard]] double mflToPixelX(mfl::points x) const;
    [[nodiscard]] double mflToPixelY(mfl::points y) const;

    void renderGlyph(QPainter& painter, const mfl::shaped_glyph& g);
    void renderLine(QPainter& painter, const mfl::line& l);

    void drawBoundingBoxes(QPainter& painter, const mfl::formula_node& node);

    void drawPlaceholders(QPainter& painter, const mfl::formula_node& node);

    QString tex_formula_;
    double font_size_pt_ = 12.0;
    double dpi_ = 96.0;
    mfl::layout_elements layout_;

    formula::AstCursor ast_;

    bool debug_draw_bboxes_ = false;

    formula::FormulaCursor cursor_;
    bool cursor_highlight_enabled_ = true;
    std::optional<QPointF> cursor_position_;

    bool cursor_after_glyph_ = false;

    std::unique_ptr<fw::FtLibrary> ft_lib_;
    std::map<mfl::font_family, std::unique_ptr<fw::FtFontFace>> font_faces_;

    double margin_left_ = 10.0;
    double margin_top_ = 10.0;
    double margin_bottom_ = 10.0;

    QTimer blink_timer_;
    bool cursor_visible_ = true;
    bool blinking_enabled_ = true;
    int blink_interval_ms_ = 500;
};
