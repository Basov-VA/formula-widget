#include "formula_widget.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QRawFont>
#include <QApplication>
#include <QScreen>
#include <QDebug>

#include <format>
#include <QMouseEvent>

namespace
{
    QColor colorForNodeType(mfl::formula_node_type type)
    {
        switch (type)
        {
            case mfl::formula_node_type::root:
                return QColor(0, 0, 255, 100);  // Blue
            case mfl::formula_node_type::symbol:
                return QColor(255, 0, 0, 100);  // Red
            case mfl::formula_node_type::fraction:
                return QColor(0, 255, 0, 100);  // Green
            case mfl::formula_node_type::numerator:
                return QColor(0, 255, 255, 100);  // Cyan
            case mfl::formula_node_type::denominator:
                return QColor(255, 0, 255, 100);  // Magenta
            case mfl::formula_node_type::radical:
                return QColor(255, 255, 0, 100);  // Yellow
            case mfl::formula_node_type::radicand:
                return QColor(128, 0, 128, 100);  // Purple
            case mfl::formula_node_type::degree:
                return QColor(0, 128, 128, 100);  // Teal
            case mfl::formula_node_type::superscript:
                return QColor(255, 165, 0, 100);  // Orange
            case mfl::formula_node_type::subscript:
                return QColor(128, 0, 0, 100);  // Maroon
            case mfl::formula_node_type::script_nucleus:
                return QColor(0, 128, 0, 100);  // Dark Green
            default:
                return QColor(128, 128, 128, 100);  // Gray
        }
    }
}

FormulaWidget::FormulaWidget(QWidget* parent)
    : QWidget(parent)
{
    // Set a minimum size for the widget
    setMinimumSize(400, 100);

    // Initialize blinking timer
    blink_timer_.setInterval(blink_interval_ms_);
    connect(&blink_timer_, &QTimer::timeout, this, &FormulaWidget::onBlinkTimer);

    // Enable keyboard focus for arrow key navigation
    setFocusPolicy(Qt::StrongFocus);

    // Try to get the actual screen DPI
    if (const auto* screen = QApplication::primaryScreen())
    {
        const auto screen_dpi = screen->logicalDotsPerInch();
        dpi_ = screen_dpi;
    }

    // Initialize FreeType library
    try
    {
        ft_lib_ = std::make_unique<fw::FtLibrary>();
    }
    catch (const std::exception& e)
    {
        // Handle error - maybe show a message to the user
        qWarning("Failed to initialize FreeType library: %s", e.what());
    }
}

FormulaWidget::~FormulaWidget() = default;

void FormulaWidget::setFormula(const QString& tex)
{
    if (tex_formula_ != tex)
    {
        tex_formula_ = tex;
        recalculateLayout();
        stopBlinking();
        emit formulaChanged();
        update();
    }
}

void FormulaWidget::setFontSize(double pt)
{
    if (font_size_pt_ != pt)
    {
        font_size_pt_ = pt;
        recalculateLayout();
        update();
    }
}

void FormulaWidget::setDpi(double dpi)
{
    if (dpi_ != dpi)
    {
        dpi_ = dpi;
        recalculateLayout();
        update();
    }
}

const mfl::layout_elements& FormulaWidget::layoutElements() const
{
    return layout_;
}

void FormulaWidget::setDebugDrawBBoxes(bool enable)
{
    if (debug_draw_bboxes_ != enable)
    {
        debug_draw_bboxes_ = enable;
        update();
    }
}

bool FormulaWidget::debugDrawBBoxes() const
{
    return debug_draw_bboxes_;
}

void FormulaWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Fill background
    painter.fillRect(rect(), Qt::white);

    if (layout_.error)
    {
        painter.setPen(Qt::red);
        painter.drawText(rect(), Qt::AlignCenter,
                        QString::fromStdString(*layout_.error));
        return;
    }

    // Apply margin
    painter.translate(margin_left_, 0);

    // Render glyphs
    for (const auto& g : layout_.glyphs)
    {
        renderGlyph(painter, g);
    }

    // Render lines
    for (const auto& l : layout_.lines)
    {
        renderLine(painter, l);
    }

    // Debug rendering of bounding boxes
    if (debug_draw_bboxes_)
    {
        drawBoundingBoxes(painter, layout_.tree);
    }

    // Рисование мигающего курсора-каретки
    if (cursor_highlight_enabled_ && cursor_.hasHighlight() && cursor_visible_) {
        QRectF caret_rect = getCaretRect();
        if (!caret_rect.isEmpty()) {
            painter.fillRect(caret_rect, Qt::black);
        }
    }

    // Рисование красной точки на позиции курсора для дебага
    if (cursor_position_.has_value()) {
        QPointF pos = cursor_position_.value();
        painter.setPen(QPen(Qt::red, 2));
        painter.drawEllipse(pos, 2, 2);
    }
}

void FormulaWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Recalculate layout when widget is resized
    recalculateLayout();
    update();
}

void FormulaWidget::recalculateLayout()
{
    if (tex_formula_.isEmpty() || !ft_lib_)
    {
        qDebug() << "recalculateLayout: skipped - formula empty=" << tex_formula_.isEmpty()
                 << "ft_lib_ null=" << (ft_lib_ == nullptr);
        return;
    }

    qDebug() << "recalculateLayout: formula=" << tex_formula_ << "font_size=" << font_size_pt_;

    try
    {
        const auto font_size = mfl::points{font_size_pt_};
        // Create font faces for each family and store them
        const auto create_font_face_with_lib = [this, font_size](const mfl::font_family family) {
            if (font_faces_.find(family) == font_faces_.end()) {
                font_faces_[family] = std::make_unique<fw::FtFontFace>(family, *ft_lib_);
            }
            font_faces_[family]->set_size(font_size);
            // Return a new instance rather than cloning
            return std::make_unique<fw::FtFontFace>(family, *ft_lib_);
        };
        layout_ = mfl::layout(tex_formula_.toStdString(), font_size, create_font_face_with_lib);
        qDebug() << "recalculateLayout: done, glyphs=" << layout_.glyphs.size()
                 << "lines=" << layout_.lines.size()
                 << "error=" << (layout_.error ? QString::fromStdString(*layout_.error) : "none");
    }
    catch (const std::exception& e)
    {
        qDebug() << "recalculateLayout: exception:" << e.what();
        layout_ = mfl::layout_elements{.error = std::format("Exception during layout: {}", e.what())};
    }

    // Update cursor with new layout
    cursor_.setLayout(&layout_);
}

// Cursor functionality implementation
void FormulaWidget::setCursorPosition(double pixel_x, double pixel_y) {
    // Store the pixel position
    cursor_position_ = QPointF(pixel_x, pixel_y);

    mfl::points x = pixelToMflX(pixel_x);
    mfl::points y = pixelToMflY(pixel_y);
    setCursorPositionMfl(x, y);
}

void FormulaWidget::setCursorPositionMfl(mfl::points x, mfl::points y) {
    // Convert MFL coordinates to pixel coordinates for cursor visualization
    double pixel_x = mflToPixelX(x);
    double pixel_y = mflToPixelY(y);
    cursor_position_ = QPointF(pixel_x, pixel_y);

    // Store the position in mfl coordinates
    cursor_.setPosition(x, y);
    if (cursor_.hasHighlight()) {
        auto hit = cursor_.currentHighlight();
        if (hit) {
            emit cursorGlyphChanged(hit->glyph_index);
        }
        // Reset blinking when cursor position is set
        resetBlinking();
    }
    update(); // Trigger repaint to show highlight
}

std::optional<QPointF> FormulaWidget::getCursorPosition() const {
    return cursor_position_;
}

void FormulaWidget::setCursorHighlightEnabled(bool enabled) {
    if (cursor_highlight_enabled_ != enabled) {
        cursor_highlight_enabled_ = enabled;
        update(); // Trigger repaint
    }
}

std::optional<formula::glyph_hit_result> FormulaWidget::currentCursorHit() const {
    return cursor_.currentHighlight();
}

// Coordinate conversion functions
mfl::points FormulaWidget::pixelToMflX(double pixel_x) const {
    // pixel_x → mfl_x: убрать отступ, конвертировать px → pt
    double pt_x = (pixel_x - margin_left_) * 72.0 / dpi_;
    return mfl::points{pt_x};
}

mfl::points FormulaWidget::pixelToMflY(double pixel_y) const {
    // Qt Y-down → mfl Y-up: инвертировать
    // В Qt: pixel_y=0 — верх виджета
    // В mfl: y=0 — baseline (примерно низ формулы)
    // Use the same conversion as qtToMfl for consistency
    const double baseline_y = margin_top_ + layout_.height.value() * dpi_ / 72.0;
    double pt_y = (baseline_y - pixel_y) * 72.0 / dpi_;
    return mfl::points{pt_y};
}

double FormulaWidget::mflToPixelX(mfl::points x) const {
    // Do NOT add margin_left_ here: paintEvent already calls painter.translate(margin_left_, 0)
    // before drawing glyphs and the cursor highlight, so the painter coordinate system
    // is already shifted. Adding margin_left_ again would double-offset the highlight rect.
    return x.value() * dpi_ / 72.0;
}

double FormulaWidget::mflToPixelY(mfl::points y) const {
    // mfl Y-up → Qt Y-down: инвертировать
    // Use the same conversion as mflToQt for consistency
    const double baseline_y = margin_top_ + layout_.height.value() * dpi_ / 72.0;
    return baseline_y - y.value() * dpi_ / 72.0;
}

void FormulaWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        const QPointF pos = event->pos();
        if (auto node = nodeAtPosition(pos))
        {
            // Print the node type and try to get character information
            QString info = QString("Clicked on node type: %1").arg(static_cast<int>(node->type));

            // Try to get character information from the first glyph if available
            if (!node->glyph_indices.empty() && !layout_.glyphs.empty()) {
                size_t glyph_index = node->glyph_indices[0];
                if (glyph_index < layout_.glyphs.size()) {
                    const auto& glyph = layout_.glyphs[glyph_index];
                    // Try to convert the glyph index to a character
                    uint char_code = static_cast<uint>(glyph.index);
                    QChar ch(static_cast<char16_t>(char_code));
                    if (!ch.isPrint() || ch.category() == QChar::Other_NotAssigned) {
                        // If not a printable character, show the code
                        info += QString(", glyph: #%1 (code: %2)")
                                   .arg(glyph_index)
                                   .arg(glyph.index);
                    } else {
                        // Show the actual character
                        info += QString(", glyph: #%1 ('%2')")
                                   .arg(glyph_index)
                                   .arg(ch);
                    }
                }
            }

            qDebug() << info;

            // Set cursor position and reset blinking
            setCursorPosition(pos.x(), pos.y());
            resetBlinking();
        }
    }

    QWidget::mousePressEvent(event);
}

double FormulaWidget::pointsToPixels(mfl::points pt) const
{
    return pt.value() * dpi_ / 72.0;
}

QPointF FormulaWidget::mflToQt(mfl::points x, mfl::points y) const
{
    // MFL uses Y-up coordinates with baseline at y=0
    // Qt uses Y-down coordinates with origin at top-left
    // We place the baseline at a fixed position from the top
    const double baseline_y = margin_top_ + pointsToPixels(layout_.height);
    return QPointF(
        pointsToPixels(x),
        baseline_y - pointsToPixels(y)  // invert Y axis relative to baseline
    );
}

QPointF FormulaWidget::qtToMfl(QPointF qt_pos) const
{
    // Convert Qt coordinates to MFL coordinates (inverse of mflToQt)
    const double baseline_y = margin_top_ + pointsToPixels(layout_.height);
    const double x_points = qt_pos.x() * 72.0 / dpi_;
    const double y_points = (baseline_y - qt_pos.y()) * 72.0 / dpi_;
    return QPointF(x_points, y_points);
}

std::optional<mfl::formula_node> FormulaWidget::nodeAtPosition(const QPointF& pos) const
{
    if (layout_.error)
        return std::nullopt;

    // Convert Qt position to MFL coordinates
    const QPointF mfl_pos = qtToMfl(pos);

    // Helper function to check if a point is inside a node's bounding box
    const auto isPointInNode = [](const QPointF& point, const mfl::formula_node& node) {
        return (point.x() >= node.bbox_x.value() &&
                point.x() <= (node.bbox_x.value() + node.bbox_width.value()) &&
                point.y() >= node.bbox_y.value() &&
                point.y() <= (node.bbox_y.value() + node.bbox_height.value()));
    };

    // Recursive function to find the deepest node containing the point
    const std::function<std::optional<mfl::formula_node>(const mfl::formula_node&)> findNode =
        [&](const mfl::formula_node& node) -> std::optional<mfl::formula_node> {
            // Check if point is in this node
            if (!isPointInNode(mfl_pos, node))
                return std::nullopt;

            // Check children first (deepest node)
            for (const auto& child : node.children)
            {
                if (auto found = findNode(child))
                    return found;
            }

            // If no children contain the point, this node is the one
            return node;
        };

    return findNode(layout_.tree);
}

void FormulaWidget::renderGlyph(QPainter& painter, const mfl::shaped_glyph& g)
{
    // Convert MFL coordinates to Qt coordinates (baseline position)
    const QPointF qt_pos = mflToQt(g.x, g.y);

    // Get the font face for this glyph's family
    const auto family_it = font_faces_.find(g.family);
    if (family_it == font_faces_.end())
    {
        // Fallback: draw a rectangle with glyph index
        const double size_pixels = pointsToPixels(g.size);
        const QRectF glyph_rect(qt_pos.x(), qt_pos.y() - size_pixels, size_pixels, size_pixels);
        painter.setPen(Qt::black);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(glyph_rect);
        painter.setPen(Qt::blue);
        painter.setFont(QFont("Arial", static_cast<int>(size_pixels / 2)));
        painter.drawText(glyph_rect, Qt::AlignCenter, QString::number(g.index));
        return;
    }

    // Scale: font units -> pixels
    // pixels_per_em = font_size_pt * dpi / 72
    const double pixels_per_em = pointsToPixels(g.size);
    const double units_per_em_val = static_cast<double>(family_it->second->units_per_em());
    const double scale = (units_per_em_val > 0) ? (pixels_per_em / units_per_em_val) : (pixels_per_em / 1000.0);

    // Get glyph outline using FT_Outline_Decompose (proper Bezier curves)
    const auto outline = family_it->second->get_glyph_outline_path(g.index);

    QPainterPath path;
    size_t i = 0;
    while (i < outline.size())
    {
        const auto& pt = outline[i];
        // Transform: x stays same, Y is inverted (FreeType Y-up -> Qt Y-down)
        const double px = qt_pos.x() + pt.x * scale;
        const double py = qt_pos.y() - pt.y * scale;

        if (pt.type == 0)  // moveTo
        {
            path.moveTo(px, py);
            ++i;
        }
        else if (pt.type == 1)  // lineTo
        {
            path.lineTo(px, py);
            ++i;
        }
        else if (pt.type == 2 && i + 1 < outline.size())  // conicTo (quadratic bezier): ctrl + end
        {
            const auto& end = outline[i + 1];  // type == 6
            const double ex = qt_pos.x() + end.x * scale;
            const double ey = qt_pos.y() - end.y * scale;
            path.quadTo(px, py, ex, ey);
            i += 2;
        }
        else if (pt.type == 3 && i + 2 < outline.size())  // cubicTo: ctrl1 + ctrl2 + end
        {
            const auto& ctrl2 = outline[i + 1];  // type == 4
            const auto& end   = outline[i + 2];  // type == 5
            const double c2x = qt_pos.x() + ctrl2.x * scale;
            const double c2y = qt_pos.y() - ctrl2.y * scale;
            const double ex  = qt_pos.x() + end.x * scale;
            const double ey  = qt_pos.y() - end.y * scale;
            path.cubicTo(px, py, c2x, c2y, ex, ey);
            i += 3;
        }
        else
        {
            ++i;  // skip unknown
        }
    }

    if (!path.isEmpty())
    {
        path.setFillRule(Qt::WindingFill);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.drawPath(path);
    }
}
void FormulaWidget::renderLine(QPainter& painter, const mfl::line& l)
{
    // l.y is the bottom of the rule in mfl Y-up coordinates
    // l.y + l.thickness is the top of the rule in mfl Y-up coordinates
    // In Qt Y-down: top of rule = mflToQt(l.x, l.y + l.thickness)
    const QPointF qt_top_left = mflToQt(l.x, l.y + l.thickness);
    const double length_pixels = pointsToPixels(l.length);
    const double thickness_pixels = pointsToPixels(l.thickness);

    // Draw a filled rectangle for the rule
    const QRectF line_rect(qt_top_left.x(), qt_top_left.y(),
                          length_pixels, thickness_pixels);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.drawRect(line_rect);
}

void FormulaWidget::drawBoundingBoxes(QPainter& painter, const mfl::formula_node& node)
{
    // Draw bounding box for this node
    const QColor color = colorForNodeType(node.type);

    // bbox_y = lowest MFL Y (visual bottom), bbox_y + bbox_height = highest MFL Y (visual top)
    // mflToQt inverts Y: higher MFL Y → smaller Qt Y (higher on screen)
    const QPointF qt_top_left     = mflToQt(node.bbox_x,                   node.bbox_y + node.bbox_height);
    const QPointF qt_bottom_right = mflToQt(node.bbox_x + node.bbox_width, node.bbox_y);

    // Use normalized rect to handle any edge cases
    const QRectF rect = QRectF(qt_top_left, qt_bottom_right).normalized();

    if (rect.width() > 0 && rect.height() > 0)
    {
        painter.setPen(QPen(color, 1, Qt::DashLine));
        painter.setBrush(color);
        painter.setOpacity(0.3);
        painter.drawRect(rect);
        painter.setOpacity(1.0);

        // Draw node type label
        painter.setPen(QPen(color.darker(), 1));
        painter.setFont(QFont("Arial", 8));
        painter.drawText(rect, Qt::AlignTop | Qt::AlignLeft,
                        QString::fromStdString(std::to_string(static_cast<int>(node.type))));
    }

    // Recursively draw children
    for (const auto& child : node.children)
    {
        drawBoundingBoxes(painter, child);
    }
}

void FormulaWidget::keyPressEvent(QKeyEvent* event) {
    // Если курсор не установлен — инициализировать
    if (!cursor_.hasHighlight() && !layout_.glyphs.empty()) {
        switch (event->key()) {
            case Qt::Key_Left:
                cursor_.setGlyphIndex(layout_.glyphs.size() - 1);
                break;
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
                cursor_.setGlyphIndex(0);
                break;
            default:
                QWidget::keyPressEvent(event);
                return;
        }
        if (auto hit = cursor_.currentHighlight())
            emit cursorGlyphChanged(hit->glyph_index);
        update();
        return;
    }

    bool moved = false;
    switch (event->key()) {
        case Qt::Key_Left:  moved = moveCursorLeft();  break;
        case Qt::Key_Right: moved = moveCursorRight(); break;
        case Qt::Key_Up:    moved = moveCursorUp();    break;
        case Qt::Key_Down:  moved = moveCursorDown();  break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }

    if (moved) {
        resetBlinking();
    }
    if (auto hit = cursor_.currentHighlight())
        emit cursorGlyphChanged(hit->glyph_index);
    update();
}

// Blinking cursor functionality
void FormulaWidget::onBlinkTimer()
{
    cursor_visible_ = !cursor_visible_;
    update();
}

void FormulaWidget::startBlinking()
{
    if (!blinking_enabled_ || !cursor_.hasHighlight()) {
        return;
    }

    cursor_visible_ = true;
    blink_timer_.start();
    update();
}

void FormulaWidget::stopBlinking()
{
    blink_timer_.stop();
    cursor_visible_ = false;
    update();
}

void FormulaWidget::resetBlinking()
{
    cursor_visible_ = true;
    if (blink_timer_.isActive()) {
        blink_timer_.stop();
        blink_timer_.start();
    }
    update();
}

bool FormulaWidget::isBlinkingEnabled() const
{
    return blinking_enabled_ && cursor_.hasHighlight();
}

bool FormulaWidget::isCursorVisible() const
{
    return cursor_visible_;
}

QRectF FormulaWidget::getCaretRect() const
{
    if (!cursor_.hasHighlight()) {
        return QRectF();
    }

    auto hit = cursor_.currentHighlight();
    if (!hit) {
        return QRectF();
    }

    // Position caret at the left edge of the glyph
    double caret_x = mflToPixelX(hit->bbox_left);
    double caret_top = mflToPixelY(hit->bbox_top);
    double caret_bottom = mflToPixelY(hit->bbox_bottom);

    const double caret_width = 2.0; // 2 pixels wide
    const double caret_height = caret_bottom - caret_top;

    return QRectF(caret_x, caret_top, caret_width, caret_height);
}

void FormulaWidget::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (cursor_.hasHighlight()) {
        startBlinking();
    }
}

void FormulaWidget::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    stopBlinking();
}


bool FormulaWidget::moveCursorLeft() {
    bool moved = cursor_.moveToDirection(formula::NavigationDirection::Left);
    if (moved) {
        resetBlinking();
    }
    return moved;
}

bool FormulaWidget::moveCursorRight() {
    bool moved = cursor_.moveToDirection(formula::NavigationDirection::Right);
    if (moved) {
        resetBlinking();
    }
    return moved;
}

bool FormulaWidget::moveCursorUp() {
    bool moved = cursor_.moveToDirection(formula::NavigationDirection::Up);
    if (moved) {
        resetBlinking();
    }
    return moved;
}

bool FormulaWidget::moveCursorDown() {
    bool moved = cursor_.moveToDirection(formula::NavigationDirection::Down);
    if (moved) {
        resetBlinking();
    }
    return moved;
}
