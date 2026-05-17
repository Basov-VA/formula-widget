#pragma once

#include "ft_library.hpp"
#include "ft_font_face.hpp"

#include "mfl/layout.hpp"
#include "formula_cursor.hpp"

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

class FormulaWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FormulaWidget(QWidget* parent = nullptr);
    ~FormulaWidget() override;

    // Non-copyable
    FormulaWidget(const FormulaWidget&) = delete;
    FormulaWidget& operator=(const FormulaWidget&) = delete;

    void setFormula(const QString& tex);
    void setFontSize(double pt);
    void setDpi(double dpi);

    [[nodiscard]] const mfl::layout_elements& layoutElements() const;

    // For debugging: visualize bounding boxes
    void setDebugDrawBBoxes(bool enable);
    [[nodiscard]] bool debugDrawBBoxes() const;

    // Hit-testing functionality
    [[nodiscard]] std::optional<mfl::formula_node> nodeAtPosition(const QPointF& pos) const;
    [[nodiscard]] QPointF qtToMfl(QPointF qt_pos) const;

    // Cursor functionality
    void setCursorPosition(double pixel_x, double pixel_y);
    void setCursorPositionMfl(mfl::points x, mfl::points y);
    void setCursorHighlightEnabled(bool enabled);
    std::optional<formula::glyph_hit_result> currentCursorHit() const;

    // Get cursor position in pixel coordinates
    std::optional<QPointF> getCursorPosition() const;

    // Arrow key navigation
    bool moveCursorLeft();
    bool moveCursorRight();
    bool moveCursorUp();
    bool moveCursorDown();

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

private:
    void recalculateLayout();

    // Blinking cursor functionality
    void onBlinkTimer();
    void startBlinking();
    void stopBlinking();
    void resetBlinking();
    bool isBlinkingEnabled() const;
    bool isCursorVisible() const;
    QRectF getCaretRect() const;

    // Coordinate conversion functions
    [[nodiscard]] double pointsToPixels(mfl::points pt) const;
    [[nodiscard]] QPointF mflToQt(mfl::points x, mfl::points y) const;
    [[nodiscard]] mfl::points pixelToMflX(double pixel_x) const;
    [[nodiscard]] mfl::points pixelToMflY(double pixel_y) const;
    [[nodiscard]] double mflToPixelX(mfl::points x) const;
    [[nodiscard]] double mflToPixelY(mfl::points y) const;

    // Rendering functions
    void renderGlyph(QPainter& painter, const mfl::shaped_glyph& g);
    void renderLine(QPainter& painter, const mfl::line& l);

    // Debug rendering
    void drawBoundingBoxes(QPainter& painter, const mfl::formula_node& node);

    QString tex_formula_;
    double font_size_pt_ = 12.0;
    double dpi_ = 96.0;
    mfl::layout_elements layout_;

    bool debug_draw_bboxes_ = false;

    // Cursor functionality
    formula::FormulaCursor cursor_;
    bool cursor_highlight_enabled_ = true;
    std::optional<QPointF> cursor_position_;

    // FreeType/HarfBuzz resources
    std::unique_ptr<fw::FtLibrary> ft_lib_;
    std::map<mfl::font_family, std::unique_ptr<fw::FtFontFace>> font_faces_;

    // Margin for rendering
    double margin_left_ = 10.0;
    double margin_top_ = 10.0;
    double margin_bottom_ = 10.0;

    // Blinking cursor functionality
    QTimer blink_timer_;
    bool cursor_visible_ = true;
    bool blinking_enabled_ = true;
    int blink_interval_ms_ = 500; // Standard cursor blink interval
};
