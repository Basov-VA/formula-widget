#include "formula_widget.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QRawFont>
#include <QApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QScreen>
#include <QDebug>

#include <format>
#include <cctype>
#include <array>
#include <QMouseEvent>

namespace
{
    QColor colorForNodeType(mfl::formula_node_type type)
    {
        switch (type)
        {
            case mfl::formula_node_type::root:
                return QColor(0, 0, 255, 100);
            case mfl::formula_node_type::symbol:
                return QColor(255, 0, 0, 100);
            case mfl::formula_node_type::fraction:
                return QColor(0, 255, 0, 100);
            case mfl::formula_node_type::numerator:
                return QColor(0, 255, 255, 100);
            case mfl::formula_node_type::denominator:
                return QColor(255, 0, 255, 100);
            case mfl::formula_node_type::radical:
                return QColor(255, 255, 0, 100);
            case mfl::formula_node_type::radicand:
                return QColor(128, 0, 128, 100);
            case mfl::formula_node_type::degree:
                return QColor(0, 128, 128, 100);
            case mfl::formula_node_type::superscript:
                return QColor(255, 165, 0, 100);
            case mfl::formula_node_type::subscript:
                return QColor(128, 0, 0, 100);
            case mfl::formula_node_type::script_nucleus:
                return QColor(0, 128, 0, 100);
            default:
                return QColor(128, 128, 128, 100);
        }
    }
}

FormulaWidget::FormulaWidget(QWidget* parent)
    : QWidget(parent)
{

    setMinimumSize(150, 60);

    blink_timer_.setInterval(blink_interval_ms_);
    connect(&blink_timer_, &QTimer::timeout, this, &FormulaWidget::onBlinkTimer);

    setFocusPolicy(Qt::StrongFocus);

    if (const auto* screen = QApplication::primaryScreen())
    {
        const auto screen_dpi = screen->logicalDotsPerInch();
        dpi_ = screen_dpi;
    }

    try
    {
        ft_lib_ = std::make_unique<fw::FtLibrary>();
    }
    catch (const std::exception& e)
    {

        qWarning("Failed to initialize FreeType library: %s", e.what());
    }
}

FormulaWidget::~FormulaWidget() = default;

void FormulaWidget::setFormula(const QString& tex)
{
    if (tex_formula_ != tex)
    {
        tex_formula_ = tex;
        ast_.setFromTex(tex.toStdString());
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

void FormulaWidget::paintEvent(QPaintEvent* )
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::white);

    if (layout_.error)
    {
        painter.setPen(Qt::red);
        painter.drawText(rect(), Qt::AlignCenter,
                        QString::fromStdString(*layout_.error));
        return;
    }

    painter.translate(margin_left_, 0);

    if (auto sel = selectionRect()) {
        painter.fillRect(*sel, QColor(90, 140, 205, 90));
    }

    for (const auto& g : layout_.glyphs)
    {
        renderGlyph(painter, g);
    }

    for (const auto& l : layout_.lines)
    {
        renderLine(painter, l);
    }

    if (debug_draw_bboxes_)
    {
        drawBoundingBoxes(painter, layout_.tree);
    }

    if (!layout_.error) {
        drawPlaceholders(painter, layout_.tree);
    }

    if (cursor_highlight_enabled_ && cursor_visible_) {
        if (auto caret = currentCaretRect()) {
            painter.fillRect(*caret, Qt::black);
        } else if (layout_.glyphs.empty() && hasFocus()) {

            const double caret_height = pointsToPixels(mfl::points{font_size_pt_});
            painter.fillRect(QRectF(0.0, margin_top_, 2.0, caret_height), Qt::black);
        }
    }

    if (cursor_position_.has_value()) {
        const QPointF pos = cursor_position_.value() - QPointF(margin_left_, 0.0);
        painter.setPen(QPen(Qt::red, 2));
        painter.drawEllipse(pos, 2, 2);
    }
}

void FormulaWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    recalculateLayout();
    update();
}

void FormulaWidget::recalculateLayout()
{
    if (!ft_lib_) return;
    if (tex_formula_.isEmpty())
    {

        layout_ = mfl::layout_elements{};
        cursor_.setLayout(&layout_);
        return;
    }

    qDebug() << "recalculateLayout: formula=" << tex_formula_ << "font_size=" << font_size_pt_;

    try
    {
        const auto font_size = mfl::points{font_size_pt_};

        const auto create_font_face_with_lib = [this, font_size](const mfl::font_family family) {
            if (font_faces_.find(family) == font_faces_.end()) {
                font_faces_[family] = std::make_unique<fw::FtFontFace>(family, *ft_lib_);
            }
            font_faces_[family]->set_size(font_size);

            return std::make_unique<fw::FtFontFace>(family, *ft_lib_);
        };

        layout_ = mfl::layout(ast_.toRenderTex(), font_size, create_font_face_with_lib);
        qDebug() << "recalculateLayout: done, glyphs=" << layout_.glyphs.size()
                 << "lines=" << layout_.lines.size()
                 << "error=" << (layout_.error ? QString::fromStdString(*layout_.error) : "none");
    }
    catch (const std::exception& e)
    {
        qDebug() << "recalculateLayout: exception:" << e.what();
        layout_ = mfl::layout_elements{.error = std::format("Exception during layout: {}", e.what())};
    }

    cursor_.setLayout(&layout_);
}

void FormulaWidget::setCursorPosition(double pixel_x, double pixel_y) {

    cursor_position_ = QPointF(pixel_x, pixel_y);

    mfl::points x = pixelToMflX(pixel_x);
    mfl::points y = pixelToMflY(pixel_y);
    setCursorPositionMfl(x, y);
}

void FormulaWidget::setCursorPositionMfl(mfl::points x, mfl::points y) {

    double pixel_x = mflToPixelX(x) + margin_left_;
    double pixel_y = mflToPixelY(y);
    cursor_position_ = QPointF(pixel_x, pixel_y);

    cursor_.setPosition(x, y);
    if (cursor_.hasHighlight()) {
        auto hit = cursor_.currentHighlight();
        if (hit) {
            emit cursorGlyphChanged(hit->glyph_index);
        }

        resetBlinking();
    }
    update();
}

std::optional<QPointF> FormulaWidget::getCursorPosition() const {
    return cursor_position_;
}

void FormulaWidget::setCursorHighlightEnabled(bool enabled) {
    if (cursor_highlight_enabled_ != enabled) {
        cursor_highlight_enabled_ = enabled;
        update();
    }
}

std::optional<formula::glyph_hit_result> FormulaWidget::currentCursorHit() const {
    return cursor_.currentHighlight();
}

mfl::points FormulaWidget::pixelToMflX(double pixel_x) const {

    double pt_x = (pixel_x - margin_left_) * 72.0 / dpi_;
    return mfl::points{pt_x};
}

mfl::points FormulaWidget::pixelToMflY(double pixel_y) const {

    const double baseline_y = margin_top_ + layout_.height.value() * dpi_ / 72.0;
    double pt_y = (baseline_y - pixel_y) * 72.0 / dpi_;
    return mfl::points{pt_y};
}

double FormulaWidget::mflToPixelX(mfl::points x) const {

    return x.value() * dpi_ / 72.0;
}

double FormulaWidget::mflToPixelY(mfl::points y) const {

    const double baseline_y = margin_top_ + layout_.height.value() * dpi_ / 72.0;
    return baseline_y - y.value() * dpi_ / 72.0;
}

void FormulaWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        const QPointF pos = event->pos();

        setCursorPosition(pos.x(), pos.y());

        if (cursor_.hasHighlight()) {
            auto glyph_idx = cursor_.currentGlyphIndex();
            if (glyph_idx.has_value()) {
                auto hit = cursor_.currentHighlight();
                double glyph_center_x = mflToPixelX(
                    mfl::points{(hit->bbox_left.value() + hit->bbox_right.value()) / 2.0});
                double click_x = pos.x() - margin_left_;

                cursor_after_glyph_ = (click_x > glyph_center_x);
                ast_.setFromGlyph(*glyph_idx, cursor_after_glyph_);
            }
        }

        resetBlinking();
    }
    QWidget::mousePressEvent(event);
}

double FormulaWidget::pointsToPixels(mfl::points pt) const
{
    return pt.value() * dpi_ / 72.0;
}

QPointF FormulaWidget::mflToQt(mfl::points x, mfl::points y) const
{

    const double baseline_y = margin_top_ + pointsToPixels(layout_.height);
    return QPointF(
        pointsToPixels(x),
        baseline_y - pointsToPixels(y)
    );
}

QPointF FormulaWidget::qtToMfl(QPointF qt_pos) const
{

    const double baseline_y = margin_top_ + pointsToPixels(layout_.height);
    const double x_points = qt_pos.x() * 72.0 / dpi_;
    const double y_points = (baseline_y - qt_pos.y()) * 72.0 / dpi_;
    return QPointF(x_points, y_points);
}

std::optional<mfl::formula_node> FormulaWidget::nodeAtPosition(const QPointF& pos) const
{
    if (layout_.error)
        return std::nullopt;

    const QPointF mfl_pos = qtToMfl(pos);

    const auto isPointInNode = [](const QPointF& point, const mfl::formula_node& node) {
        return (point.x() >= node.bbox_x.value() &&
                point.x() <= (node.bbox_x.value() + node.bbox_width.value()) &&
                point.y() >= node.bbox_y.value() &&
                point.y() <= (node.bbox_y.value() + node.bbox_height.value()));
    };

    const std::function<std::optional<mfl::formula_node>(const mfl::formula_node&)> findNode =
        [&](const mfl::formula_node& node) -> std::optional<mfl::formula_node> {

            if (!isPointInNode(mfl_pos, node))
                return std::nullopt;

            for (const auto& child : node.children)
            {
                if (auto found = findNode(child))
                    return found;
            }

            return node;
        };

    return findNode(layout_.tree);
}

void FormulaWidget::renderGlyph(QPainter& painter, const mfl::shaped_glyph& g)
{

    const QPointF qt_pos = mflToQt(g.x, g.y);

    const auto family_it = font_faces_.find(g.family);
    if (family_it == font_faces_.end())
    {

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

    const double pixels_per_em = pointsToPixels(g.size);
    const double units_per_em_val = static_cast<double>(family_it->second->units_per_em());
    const double scale = (units_per_em_val > 0) ? (pixels_per_em / units_per_em_val) : (pixels_per_em / 1000.0);

    const auto outline = family_it->second->get_glyph_outline_path(g.index);

    QPainterPath path;
    size_t i = 0;
    while (i < outline.size())
    {
        const auto& pt = outline[i];

        const double px = qt_pos.x() + pt.x * scale;
        const double py = qt_pos.y() - pt.y * scale;

        if (pt.type == 0)
        {
            path.moveTo(px, py);
            ++i;
        }
        else if (pt.type == 1)
        {
            path.lineTo(px, py);
            ++i;
        }
        else if (pt.type == 2 && i + 1 < outline.size())
        {
            const auto& end = outline[i + 1];
            const double ex = qt_pos.x() + end.x * scale;
            const double ey = qt_pos.y() - end.y * scale;
            path.quadTo(px, py, ex, ey);
            i += 2;
        }
        else if (pt.type == 3 && i + 2 < outline.size())
        {
            const auto& ctrl2 = outline[i + 1];
            const auto& end   = outline[i + 2];
            const double c2x = qt_pos.x() + ctrl2.x * scale;
            const double c2y = qt_pos.y() - ctrl2.y * scale;
            const double ex  = qt_pos.x() + end.x * scale;
            const double ey  = qt_pos.y() - end.y * scale;
            path.cubicTo(px, py, c2x, c2y, ex, ey);
            i += 3;
        }
        else
        {
            ++i;
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

    const QPointF qt_top_left = mflToQt(l.x, l.y + l.thickness);
    const double length_pixels = pointsToPixels(l.length);
    const double thickness_pixels = pointsToPixels(l.thickness);

    const QRectF line_rect(qt_top_left.x(), qt_top_left.y(),
                          length_pixels, thickness_pixels);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.drawRect(line_rect);
}

void FormulaWidget::drawBoundingBoxes(QPainter& painter, const mfl::formula_node& node)
{

    const QColor color = colorForNodeType(node.type);

    const QPointF qt_top_left     = mflToQt(node.bbox_x,                   node.bbox_y + node.bbox_height);
    const QPointF qt_bottom_right = mflToQt(node.bbox_x + node.bbox_width, node.bbox_y);

    const QRectF rect = QRectF(qt_top_left, qt_bottom_right).normalized();

    if (rect.width() > 0 && rect.height() > 0)
    {
        painter.setPen(QPen(color, 1, Qt::DashLine));
        painter.setBrush(color);
        painter.setOpacity(0.3);
        painter.drawRect(rect);
        painter.setOpacity(1.0);

        painter.setPen(QPen(color.darker(), 1));
        painter.setFont(QFont("Arial", 8));
        painter.drawText(rect, Qt::AlignTop | Qt::AlignLeft,
                        QString::fromStdString(std::to_string(static_cast<int>(node.type))));
    }

    for (const auto& child : node.children)
    {
        drawBoundingBoxes(painter, child);
    }
}

namespace {

    bool isPlainInputChar(char ch) {
        if (std::isalnum(static_cast<unsigned char>(ch))) return true;
        switch (ch) {
            case '+': case '-': case '*': case '=':
            case '(': case ')': case '[': case ']':
            case '<': case '>': case ',': case '.':
            case '|': case '!': case ':': case ';':
                return true;
            default:
                return false;
        }
    }
}

void FormulaWidget::drawPlaceholders(QPainter& painter, const mfl::formula_node& node)
{
    using T = mfl::formula_node_type;
    const bool isField = node.type == T::numerator || node.type == T::denominator ||
                         node.type == T::radicand  || node.type == T::superscript ||
                         node.type == T::subscript || node.type == T::degree ||
                         node.type == T::matrix_cell;
    if (isField) {
        const std::function<bool(const mfl::formula_node&)> hasGlyphs =
            [&](const mfl::formula_node& n) -> bool {
                if (!n.glyph_indices.empty()) return true;
                for (const auto& ch : n.children) if (hasGlyphs(ch)) return true;
                return false;
            };
        if (!hasGlyphs(node)) {
            const double hPt = font_size_pt_ * 0.72;
            const double wPt = font_size_pt_ * 0.55;
            const double cxMfl = node.bbox_x.value() + node.bbox_width.value() / 2.0;
            const double wPx = pointsToPixels(mfl::points{wPt});
            const double hPx = pointsToPixels(mfl::points{hPt});
            const double leftPx = mflToPixelX(mfl::points{cxMfl}) - wPx / 2.0;

            const double baseY = mflToPixelY(node.bbox_y);
            const QRectF box(leftPx, baseY - hPx * 0.82, wPx, hPx);
            painter.setPen(QPen(QColor(90, 140, 205, 210), 1.2, Qt::DashLine));
            painter.setBrush(QColor(90, 140, 205, 32));
            painter.drawRoundedRect(box, 2.0, 2.0);
        }
    }
    for (const auto& c : node.children) drawPlaceholders(painter, c);
}

void FormulaWidget::keyPressEvent(QKeyEvent* event) {
    const bool ctrl  = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    if (ctrl && event->key() == Qt::Key_Z && !shift) {
        if (ast_.undo()) applyAstEdit();
        return;
    }
    if (ctrl && (event->key() == Qt::Key_Y ||
                 (event->key() == Qt::Key_Z && shift))) {
        if (ast_.redo()) applyAstEdit();
        return;
    }

    if (ctrl && event->key() == Qt::Key_A) {
        ast_.selectAll();
        mapCaretFromAst();
        resetBlinking();
        update();
        return;
    }
    if (ctrl && event->key() == Qt::Key_C) {
        const QString sel = QString::fromStdString(ast_.selectedTex());
        if (!sel.isEmpty()) QGuiApplication::clipboard()->setText(sel);
        return;
    }
    if (ctrl && event->key() == Qt::Key_X) {
        const QString sel = QString::fromStdString(ast_.selectedTex());
        if (!sel.isEmpty()) {
            QGuiApplication::clipboard()->setText(sel);
            ast_.snapshot(false);
            ast_.deleteSelection();
            applyAstEdit();
        }
        return;
    }
    if (ctrl && event->key() == Qt::Key_V) {
        const QString clip = QGuiApplication::clipboard()->text();
        if (!clip.isEmpty()) {
            ast_.snapshot(false);
            ast_.insertTex(clip.toStdString());
            applyAstEdit();
        }
        return;
    }

    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down: {
            formula::Dir dir = formula::Dir::Left;
            switch (event->key()) {
                case Qt::Key_Left:  dir = formula::Dir::Left;  break;
                case Qt::Key_Right: dir = formula::Dir::Right; break;
                case Qt::Key_Up:    dir = formula::Dir::Up;    break;
                case Qt::Key_Down:  dir = formula::Dir::Down;  break;
            }

            const bool horizontal = (dir == formula::Dir::Left || dir == formula::Dir::Right);
            if (shift && horizontal) {
                ast_.extendSelection(dir);
            } else {
                ast_.clearSelection();
                ast_.move(dir);
            }
            mapCaretFromAst();
            startBlinking();
            resetBlinking();
            if (auto hit = cursor_.currentHighlight())
                emit cursorGlyphChanged(hit->glyph_index);
            update();
            return;
        }

        case Qt::Key_Tab:
            ast_.clearSelection();
            if (ast_.tabNextField()) {
                mapCaretFromAst();
                startBlinking();
                resetBlinking();
            }
            update();
            return;

        case Qt::Key_Backtab:
            ast_.clearSelection();
            if (ast_.tabPrevField()) {
                mapCaretFromAst();
                startBlinking();
                resetBlinking();
            }
            update();
            return;

        case Qt::Key_Backspace:
            if (ast_.hasSelection()) {
                ast_.snapshot(false);
                ast_.deleteSelection();
                applyAstEdit();
                return;
            }
            ast_.snapshot(false);
            if (ast_.deleteBack()) applyAstEdit(); else ast_.discardLastUndo();
            return;

        case Qt::Key_Delete:
            if (ast_.hasSelection()) {
                ast_.snapshot(false);
                ast_.deleteSelection();
                applyAstEdit();
                return;
            }
            ast_.snapshot(false);
            if (ast_.deleteForward()) applyAstEdit(); else ast_.discardLastUndo();
            return;

        default:
            break;
    }

    if (event->key() == Qt::Key_R && ctrl) {
        beginEdit(false);
        ast_.insertSqrt();
        applyAstEdit();
        return;
    }

    const QString text = event->text();
    if (!text.isEmpty() && !ctrl) {
        const char ch = text.at(0).toLatin1();
        if (ch == '/') {
            beginEdit(false);
            ast_.insertFraction();
            applyAstEdit();
            return;
        }
        if (ch == '^') {
            beginEdit(false);
            ast_.insertScript(true);
            applyAstEdit();
            return;
        }
        if (ch == '_') {
            beginEdit(false);
            ast_.insertScript(false);
            applyAstEdit();
            return;
        }
        if (isPlainInputChar(ch)) {
            beginEdit(true);
            ast_.insertChar(ch);
            applyAstEdit();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void FormulaWidget::beginEdit(bool typing) {
    ast_.snapshot(typing);
    if (ast_.hasSelection()) ast_.deleteSelection();
}

bool FormulaWidget::focusNextPrevChild(bool ) {

    return false;
}

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

namespace {

    void collectSubtreeGlyphs(const mfl::formula_node& n, std::vector<std::size_t>& out) {
        for (auto g : n.glyph_indices) out.push_back(g);
        for (const auto& c : n.children) collectSubtreeGlyphs(c, out);
    }

    bool subtreeWithin(const mfl::formula_node& n, std::size_t s0, std::size_t s1) {
        std::vector<std::size_t> gs;
        collectSubtreeGlyphs(n, gs);
        if (gs.empty()) return s0 == s1;
        for (auto g : gs) if (g < s0 || g >= s1) return false;
        return true;
    }
    const mfl::formula_node* firstChildOfType(const mfl::formula_node& p, mfl::formula_node_type t) {
        for (const auto& c : p.children) if (c.type == t) return &c;
        return nullptr;
    }

    const mfl::formula_node* findStructNode(const mfl::formula_node& n, mfl::formula_node_type structType,
                                            std::size_t s0, std::size_t s1) {
        if (n.type == structType && subtreeWithin(n, s0, s1)) return &n;
        for (const auto& c : n.children)
            if (const auto* r = findStructNode(c, structType, s0, s1)) return r;
        return nullptr;
    }

    const mfl::formula_node* findScriptField(const mfl::formula_node& n, std::size_t baseGlyph,
                                             mfl::formula_node_type ft) {
        auto isNucleus = [](const mfl::formula_node& c) {
            return c.type == mfl::formula_node_type::symbol ||
                   c.type == mfl::formula_node_type::script_nucleus;
        };
        for (std::size_t i = 0; i < n.children.size(); ++i) {
            const auto& c = n.children[i];
            bool matches = false;
            if (isNucleus(c))
                for (auto g : c.glyph_indices) if (g == baseGlyph) { matches = true; break; }
            if (!matches) continue;

            for (std::size_t j = i + 1; j < n.children.size(); ++j) {
                const auto& s = n.children[j];
                if (s.type == ft) return &s;
                if (isNucleus(s)) break;
            }
        }
        for (const auto& c : n.children)
            if (const auto* r = findScriptField(c, baseGlyph, ft)) return r;
        return nullptr;
    }

    void collectMatrixCells(const mfl::formula_node& n,
                            std::vector<const mfl::formula_node*>& out) {
        using T = mfl::formula_node_type;
        for (const auto& c : n.children) {
            if (c.type == T::matrix_cell) out.push_back(&c);
            else if (c.type == T::matrix) {  }
            else collectMatrixCells(c, out);
        }
    }

    const mfl::formula_node* findFieldNode(const mfl::formula_node& root,
                                           const formula::FieldContext& fc) {
        using T = mfl::formula_node_type;
        const std::size_t s0 = fc.glyphStart;
        const std::size_t s1 = fc.glyphStart + fc.glyphCount;
        switch (fc.structKind) {
            case formula::MathKind::Accent: {
                if (fc.command == "overline")  return findStructNode(root, T::overline, s0, s1);
                if (fc.command == "underline") return findStructNode(root, T::underline, s0, s1);
                return findStructNode(root, T::accent, s0, s1);
            }
            case formula::MathKind::Matrix: {
                const auto* m = findStructNode(root, T::matrix, s0, s1);
                if (!m) return nullptr;
                std::vector<const mfl::formula_node*> cells;
                collectMatrixCells(*m, cells);
                if (fc.fieldIndex < cells.size()) return cells[fc.fieldIndex];
                return nullptr;
            }
            case formula::MathKind::Frac: {
                const auto* p = findStructNode(root, T::fraction, s0, s1);
                if (!p) return nullptr;
                return firstChildOfType(*p, fc.fieldIndex == 0 ? T::numerator : T::denominator);
            }
            case formula::MathKind::Sqrt: {
                const auto* p = findStructNode(root, T::radical, s0, s1);
                if (!p) return nullptr;
                return firstChildOfType(*p, T::radicand);
            }
            case formula::MathKind::Group:
                return findStructNode(root, T::group, s0, s1);
            case formula::MathKind::Script: {
                const std::size_t supIdx = fc.has_sup ? 1u : SIZE_MAX;
                const std::size_t subIdx = fc.has_sub ? (fc.has_sup ? 2u : 1u) : SIZE_MAX;
                T ft;
                if (fc.fieldIndex == supIdx) ft = T::superscript;
                else if (fc.fieldIndex == subIdx) ft = T::subscript;
                else return nullptr;
                return findScriptField(root, s0, ft);
            }
            default:
                return nullptr;
        }
    }
}

std::optional<QRectF> FormulaWidget::emptyFieldCaretRect() const
{
    const auto fc = ast_.fieldContext();
    if (!fc) return std::nullopt;

    const mfl::formula_node* node = findFieldNode(layout_.tree, *fc);

    if (!node) return structCaretRect(*fc, false);

    const double left_px = mflToPixelX(node->bbox_x);

    const bool is_script = (fc->structKind == formula::MathKind::Script);
    const double default_h_pt = font_size_pt_ * (is_script ? 0.55 : 0.75);

    double top_px, bottom_px;
    if (node->bbox_height.value() > 0.01) {
        top_px = mflToPixelY(node->bbox_y + node->bbox_height);
        bottom_px = mflToPixelY(node->bbox_y);
    } else {

        bottom_px = mflToPixelY(node->bbox_y);
        top_px = mflToPixelY(mfl::points{node->bbox_y.value() + default_h_pt});
    }

    const double y = std::min(top_px, bottom_px);
    const double h = std::max(std::abs(bottom_px - top_px), pointsToPixels(mfl::points{default_h_pt}));
    return QRectF(left_px, y, 2.0, h);
}

std::optional<QRectF> FormulaWidget::glyphCaretRect(std::size_t glyph_index, bool after) const
{
    if (glyph_index >= layout_.glyphs.size()) return std::nullopt;
    const auto& g = layout_.glyphs[glyph_index];
    const double x = after ? mflToPixelX(g.x + g.advance) : mflToPixelX(g.x);
    double top = mflToPixelY(g.y + g.height);
    double bottom = mflToPixelY(g.y - g.depth);
    double h = std::abs(bottom - top);
    const double min_h = 20.0;
    if (h < min_h) {
        const double c = (top + bottom) / 2.0;
        top = c - min_h / 2.0;
        h = min_h;
    }
    return QRectF(x, std::min(top, bottom), 2.0, h);
}

std::optional<QRectF> FormulaWidget::structCaretRect(const formula::FieldContext& fc, bool after) const
{
    using T = mfl::formula_node_type;
    const std::size_t s0 = fc.glyphStart;
    const std::size_t s1 = fc.glyphStart + fc.glyphCount;

    const mfl::formula_node* n = nullptr;
    switch (fc.structKind) {
        case formula::MathKind::Frac:  n = findStructNode(layout_.tree, T::fraction, s0, s1); break;
        case formula::MathKind::Sqrt:  n = findStructNode(layout_.tree, T::radical,  s0, s1); break;
        case formula::MathKind::Group: n = findStructNode(layout_.tree, T::group,    s0, s1); break;
        default: break;
    }

    double bx, by, bw, bh;
    if (n) {
        bx = n->bbox_x.value(); by = n->bbox_y.value();
        bw = n->bbox_width.value(); bh = n->bbox_height.value();
    } else {

        if (fc.glyphCount == 0) return std::nullopt;
        double left = 1e18, right = -1e18, bottom = 1e18, top = -1e18;
        for (std::size_t i = s0; i < s1 && i < layout_.glyphs.size(); ++i) {
            const auto& g = layout_.glyphs[i];
            left   = std::min(left,   g.x.value());
            right  = std::max(right,  g.x.value() + g.advance.value());
            bottom = std::min(bottom, g.y.value() - g.depth.value());
            top    = std::max(top,    g.y.value() + g.height.value());
        }
        if (right < left) return std::nullopt;
        bx = left; by = bottom; bw = right - left; bh = top - bottom;
    }

    const double px = after ? mflToPixelX(mfl::points{bx + bw}) : mflToPixelX(mfl::points{bx});
    const double top    = mflToPixelY(mfl::points{by + bh});
    const double bottom = mflToPixelY(mfl::points{by});
    return QRectF(px, std::min(top, bottom), 2.0, std::abs(bottom - top));
}

std::optional<QRectF> FormulaWidget::currentCaretRect() const
{

    if (ast_.inEmptyRow()) return emptyFieldCaretRect();

    const auto anchor = ast_.caretAnchor();

    if (!anchor) return emptyFieldCaretRect();
    if (anchor->isStruct) return structCaretRect(anchor->structCtx, anchor->after);
    return glyphCaretRect(anchor->glyphIndex, anchor->after);
}

std::optional<QRectF> FormulaWidget::selectionRect() const
{
    const auto range = ast_.selectedGlyphRange();
    if (!range) return std::nullopt;
    const auto [g0, g1] = *range;

    double left = 1e18, right = -1e18, top = -1e18, bottom = 1e18;
    for (std::size_t i = g0; i < g1 && i < layout_.glyphs.size(); ++i) {
        const auto& g = layout_.glyphs[i];
        left   = std::min(left,   g.x.value());
        right  = std::max(right,  g.x.value() + g.advance.value());
        top    = std::max(top,    g.y.value() + g.height.value());
        bottom = std::min(bottom, g.y.value() - g.depth.value());
    }
    if (right < left) return std::nullopt;

    const double px0    = mflToPixelX(mfl::points{left});
    const double px1    = mflToPixelX(mfl::points{right});
    const double pyTop  = mflToPixelY(mfl::points{top});
    const double pyBot  = mflToPixelY(mfl::points{bottom});

    const double pad = 2.0;
    return QRectF(px0, std::min(pyTop, pyBot) - pad,
                  px1 - px0, std::abs(pyBot - pyTop) + 2 * pad);
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

    double caret_x;

    if (cursor_after_glyph_) {
        caret_x = mflToPixelX(hit->bbox_right);
    } else {

        caret_x = mflToPixelX(hit->bbox_left);
    }

    double caret_top = mflToPixelY(hit->bbox_top);
    double caret_bottom = mflToPixelY(hit->bbox_bottom);

    const double caret_width = 2.0;
    double caret_height = std::abs(caret_bottom - caret_top);

    const double min_caret_height = 20.0;
    if (caret_height < min_caret_height) {
        double caret_center = (caret_top + caret_bottom) / 2.0;
        caret_top = caret_center - min_caret_height / 2.0;
        caret_bottom = caret_center + min_caret_height / 2.0;
        caret_height = min_caret_height;
    }

    double actual_top = std::min(caret_top, caret_bottom);
    double actual_height = std::abs(caret_bottom - caret_top);

    return QRectF(caret_x, actual_top, caret_width, actual_height);
}

void FormulaWidget::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);

    mapCaretFromAst();
    startBlinking();
    update();
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

void FormulaWidget::insertSymbol(const QString& command) {
    beginEdit(false);
    ast_.insertSymbol(command.toStdString());
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertFunction(const QString& command) {

    beginEdit(false);
    ast_.insertSymbol(command.toStdString());
    ast_.insertChar('(');
    ast_.insertChar(')');
    ast_.move(formula::Dir::Left);
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertFraction() {
    beginEdit(false);
    ast_.insertFraction();
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertSqrt() {
    beginEdit(false);
    ast_.insertSqrt();
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertSuperscript() {
    beginEdit(false);
    ast_.insertScript(true);
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertBigOperator(const QString& command) {
    beginEdit(false);
    ast_.insertBigOperator(command.toStdString());
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertBracketPair(QChar open, QChar close) {
    beginEdit(false);
    ast_.insertChar(open.toLatin1());
    ast_.insertChar(close.toLatin1());
    ast_.move(formula::Dir::Left);
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertSubscript() {
    beginEdit(false);
    ast_.insertScript(false);
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertAccent(const QString& command) {
    beginEdit(false);
    ast_.insertAccent(command.toStdString());
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::insertMatrix(int rows, int cols, char open, char close) {
    beginEdit(false);
    ast_.insertMatrix(static_cast<std::size_t>(rows), static_cast<std::size_t>(cols),
                      open, close);
    applyAstEdit();
    setFocus(Qt::OtherFocusReason);
}

void FormulaWidget::applyAstEdit() {
    tex_formula_ = QString::fromStdString(ast_.toTex());
    recalculateLayout();
    mapCaretFromAst();
    emit formulaChanged();
    resetBlinking();
    update();
}

void FormulaWidget::mapCaretFromAst() {
    if (layout_.glyphs.empty()) {
        cursor_.clearHighlight();
        cursor_after_glyph_ = false;
        return;
    }

    const auto anchor = ast_.caretAnchor();
    if (!anchor) {
        cursor_.clearHighlight();
        cursor_after_glyph_ = false;
        return;
    }
    std::size_t gi = anchor->isStruct ? anchor->structCtx.glyphStart : anchor->glyphIndex;
    if (gi >= layout_.glyphs.size()) gi = layout_.glyphs.size() - 1;
    cursor_.setGlyphIndex(gi);
    cursor_after_glyph_ = anchor->after;
}

QString FormulaWidget::currentTexFormula() const {
    return tex_formula_;
}
