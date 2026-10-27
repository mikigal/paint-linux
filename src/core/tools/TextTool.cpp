#include "tools/TextTool.h"

#include "document/Document.h"
#include "document/History.h"

#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QPainter>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextLayout>

#include <cmath>

namespace paint {

namespace {

// Room between the box's edge and its text.
constexpr qreal Margin = 2;
// Width (in screen pixels) of the band along the border that moves the box.
constexpr qreal BorderBand = 4;
// A drag smaller than this (both ways) counts as a click.
constexpr qreal ClickSize = 6;

QFont fontFor(const TextFormat &format)
{
    QFont font = format.family.isEmpty() ? QFontDatabase::systemFont(QFontDatabase::GeneralFont)
                                         : QFont(format.family);
    font.setPointSizeF(format.pointSize);
    font.setBold(format.bold);
    font.setItalic(format.italic);
    font.setUnderline(format.underline);
    font.setStrikeOut(format.strikeOut);
    return font;
}

QString familyOf(const QTextCharFormat &format)
{
    const QStringList families = format.fontFamilies().toStringList();
    return families.isEmpty() ? format.font().family() : families.constFirst();
}

// Whole pixels covering the box from `a` to `b`.
QRectF pixelBox(QPointF a, QPointF b)
{
    return QRectF(QPointF(std::floor(std::min(a.x(), b.x())), std::floor(std::min(a.y(), b.y()))),
                  QPointF(std::ceil(std::max(a.x(), b.x())), std::ceil(std::max(a.y(), b.y()))));
}

bool isCommand(const KeyEvent &event)
{
    // Ctrl+Alt is AltGr on some keyboards and types characters.
    return (event.modifiers & Qt::ControlModifier) && !(event.modifiers & Qt::AltModifier);
}

bool typesText(const KeyEvent &event)
{
    return !event.text.isEmpty() && !isCommand(event) && !(event.modifiers & Qt::MetaModifier) &&
           event.text.front().isPrint();
}

bool isEditingKey(int key)
{
    switch (key) {
    case Qt::Key_Escape:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Tab:
        return true;
    default:
        return false;
    }
}

bool isCommandKey(int key)
{
    switch (key) {
    case Qt::Key_A:
    case Qt::Key_C:
    case Qt::Key_X:
    case Qt::Key_V:
    case Qt::Key_Z:
    case Qt::Key_Y:
        return true;
    default:
        return false;
    }
}

} // namespace

TextTool::TextTool(ToolContext &context)
    : InteractiveTool(context)
    , m_resolution(1, 1, QImage::Format_ARGB32_Premultiplied)
{
    m_resolution.setDotsPerMeterX(Document::DefaultDotsPerMeter);
    m_resolution.setDotsPerMeterY(Document::DefaultDotsPerMeter);
}

TextTool::~TextTool() = default;

QTransform TextTool::toDocument() const
{
    // Box coordinates (the text layout's) to document coordinates.
    return QTransform::fromTranslate(m_frame.left(), m_frame.top()) *
           box::rotationAround(m_frame.center(), m_angle);
}

QPointF TextTool::toBox(QPointF documentPoint) const
{
    return toDocument().inverted().map(documentPoint);
}

int TextTool::positionAt(QPointF documentPoint) const
{
    const int position = m_document->documentLayout()->hitTest(toBox(documentPoint), Qt::FuzzyHit);
    return position < 0 ? m_document->characterCount() - 1 : position;
}

void TextTool::open(QRectF frame)
{
    Document &document = m_context.document();
    const ToolSettings &settings = m_context.settings();
    m_layer = document.activeLayerIndex();
    m_before = document.layer(m_layer).image; // shared until the text is drawn
    m_dirty = {};
    m_shown = {};

    m_document = std::make_unique<QTextDocument>();
    m_document->documentLayout()->setPaintDevice(&m_resolution);
    m_document->setDocumentMargin(Margin);
    QTextOption option = m_document->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_document->setDefaultTextOption(option);
    const QFont font = fontFor(settings.textFormat());
    m_document->setDefaultFont(font);
    m_cursor = QTextCursor(m_document.get());
    // Every character carries its full format, so the format bar can always show it.
    QTextCharFormat format;
    format.setFont(font, QTextCharFormat::FontPropertiesAll);
    QTextBlockFormat block;
    block.setAlignment(settings.textAlignment());
    m_cursor.setBlockFormat(block);
    m_cursor.setBlockCharFormat(format);
    m_cursor.setCharFormat(format);
    m_document->clearUndoRedoStacks();

    m_frame = frame;
    m_minimumHeight = frame.height();
    m_angle = 0;
    m_mode = Mode::Editing;
    m_drag = Drag::None;
    changed();
}

void TextTool::press(const PointerEvent &event)
{
    if (event.button != Qt::LeftButton) {
        return;
    }
    switch (m_mode) {
    case Mode::Idle:
        m_mode = Mode::Defining;
        m_drag = Drag::Define;
        m_pressPosition = event.position;
        m_frame = pixelBox(event.position, event.position);
        notify();
        return;
    case Mode::Defining:
        return;
    case Mode::Editing:
        break;
    }
    const BoxHandle handle = hitTest(event.position, event.screenPixel);
    if (handle == BoxHandle::None) {
        // Outside the box a click finishes the text.
        if (!isOverText(event.position, event.screenPixel)) {
            commit();
            return;
        }
        m_drag = Drag::Select;
        const int position = positionAt(event.position);
        if (event.doubleClick) {
            m_cursor.setPosition(position);
            m_cursor.select(QTextCursor::WordUnderCursor);
        } else {
            m_cursor.setPosition(position, event.modifiers & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                                                               : QTextCursor::MoveAnchor);
        }
        render();
        notify();
        syncFormat();
        return;
    }
    m_handle = handle;
    m_drag = handle == BoxHandle::Inside   ? Drag::Move
             : handle == BoxHandle::Rotate ? Drag::Rotate
                                           : Drag::Resize;
    m_pressPosition = event.position;
    m_startFrame = m_frame;
    m_startAngle = m_angle;
    notify();
}

void TextTool::move(const PointerEvent &event)
{
    const QPointF position = event.position;
    const QPointF delta = position - m_pressPosition;
    switch (m_drag) {
    case Drag::None:
        return;
    case Drag::Define:
        m_frame = pixelBox(m_pressPosition, position);
        notify();
        return;
    case Drag::Select:
        m_cursor.setPosition(positionAt(position), QTextCursor::KeepAnchor);
        render();
        notify();
        syncFormat();
        return;
    case Drag::Move:
        // Whole pixels keep the text crisp.
        m_frame = m_startFrame.translated(std::round(delta.x()), std::round(delta.y()));
        render();
        notify();
        return;
    case Drag::Resize: {
        QRectF frame =
            box::resized(m_startFrame, m_angle, m_handle, position, event.modifiers & Qt::ShiftModifier);
        if (m_angle == 0) {
            frame = QRectF(QPointF(std::round(frame.left()), std::round(frame.top())),
                           QPointF(std::round(frame.right()), std::round(frame.bottom())));
        }
        // The text re-wraps to the new width; the box stays tall enough for it.
        frame.setWidth(std::max<qreal>(frame.width(), 2 * Margin + 1));
        m_frame = frame;
        m_minimumHeight = frame.height();
        changed();
        return;
    }
    case Drag::Rotate:
        m_angle = box::rotated(m_startFrame.center(), m_startAngle, m_pressPosition, position,
                               event.modifiers & Qt::ShiftModifier);
        render();
        notify();
        return;
    }
}

void TextTool::release(const PointerEvent &event)
{
    if (m_drag == Drag::None) {
        return;
    }
    move(event);
    const Drag drag = m_drag;
    m_drag = Drag::None;
    m_handle = BoxHandle::None;
    if (drag != Drag::Define) {
        notify();
        return;
    }
    QRectF frame = m_frame;
    const QFontMetricsF metrics(fontFor(m_context.settings().textFormat()), &m_resolution);
    const qreal line = std::ceil(metrics.height() + 2 * Margin);
    if (frame.width() < ClickSize && frame.height() < ClickSize) {
        // A click opens a box for a line of text where it was made.
        frame =
            QRectF(frame.topLeft(), QSizeF(std::ceil(metrics.averageCharWidth() * 20 + 2 * Margin), line));
    }
    // Room for at least one character.
    frame.setWidth(std::max(frame.width(), std::ceil(metrics.height() + 2 * Margin)));
    frame.setHeight(std::max(frame.height(), line));
    open(frame);
}

void TextTool::changed()
{
    fitFrame();
    render();
    notify();
    syncFormat();
}

void TextTool::fitFrame()
{
    m_document->setTextWidth(m_frame.width());
    const qreal height = std::max(m_minimumHeight, std::ceil(m_document->size().height()));
    if (height == m_frame.height()) {
        return;
    }
    // The box grows downwards: its (possibly turned) top-left corner stays put.
    const QPointF anchor = toDocument().map(QPointF());
    m_frame.setHeight(height);
    m_frame.translate(anchor - toDocument().map(QPointF()));
}

void TextTool::render(bool showSelection)
{
    Document &document = m_context.document();
    QImage &layer = document.layer(m_layer).image;
    const ToolSettings &settings = m_context.settings();
    const QTransform transform = toDocument();
    const QRectF box(QPointF(), m_frame.size());
    const QRect area =
        transform.mapRect(box).toAlignedRect().adjusted(-1, -1, 1, 1).intersected(document.rect());

    QPainter painter(&layer);
    if (!m_shown.isEmpty()) {
        // Put back what the text covered last time.
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(m_shown.topLeft(), m_before, m_shown);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    if (!area.isEmpty()) {
        painter.setClipRect(area);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                               QPainter::SmoothPixmapTransform);
        painter.setTransform(transform);
        if (settings.textBackground()) {
            painter.fillRect(box, settings.color(ColorSlot::Secondary));
        }
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette.setColor(QPalette::Text, settings.color(ColorSlot::Primary));
        context.clip = box;
        if (showSelection && m_cursor.hasSelection()) {
            const QPalette palette = QGuiApplication::palette();
            QAbstractTextDocumentLayout::Selection selection;
            selection.cursor = m_cursor;
            selection.format.setBackground(palette.color(QPalette::Highlight));
            selection.format.setForeground(palette.color(QPalette::HighlightedText));
            context.selections.append(selection);
        }
        m_document->documentLayout()->draw(&painter, context);
    }
    painter.end();

    const QRect changed = m_shown | area;
    m_shown = area;
    m_dirty |= area;
    if (!changed.isEmpty()) {
        m_context.documentChanged(changed);
    }
}

void TextTool::syncFormat()
{
    const QTextCharFormat format = m_cursor.charFormat();
    m_syncedFormat =
        TextFormat{familyOf(format),
                   format.fontPointSize() > 0 ? format.fontPointSize() : format.font().pointSizeF(),
                   format.fontWeight() >= QFont::Bold,
                   format.fontItalic(),
                   format.fontUnderline(),
                   format.fontStrikeOut()};
    const Qt::Alignment alignment = m_cursor.blockFormat().alignment() & Qt::AlignHorizontal_Mask;
    m_syncedAlignment = alignment ? alignment : Qt::AlignLeft;
    ToolSettings &settings = m_context.settings();
    settings.setTextFormat(m_syncedFormat);
    settings.setTextAlignment(m_syncedAlignment);
}

void TextTool::applyFormat(const TextFormat &format)
{
    if (!isEditing()) {
        return;
    }
    QTextCharFormat change;
    if (format.family != m_syncedFormat.family) {
        change.setFontFamilies(QStringList{fontFor(format).family()});
    }
    if (format.pointSize != m_syncedFormat.pointSize) {
        change.setFontPointSize(format.pointSize);
    }
    if (format.bold != m_syncedFormat.bold) {
        change.setFontWeight(format.bold ? QFont::Bold : QFont::Normal);
    }
    if (format.italic != m_syncedFormat.italic) {
        change.setFontItalic(format.italic);
    }
    if (format.underline != m_syncedFormat.underline) {
        change.setFontUnderline(format.underline);
    }
    if (format.strikeOut != m_syncedFormat.strikeOut) {
        change.setFontStrikeOut(format.strikeOut);
    }
    m_syncedFormat = format;
    if (change.properties().isEmpty()) {
        return;
    }
    // Applies to the selection, or to what is typed next; an empty line also takes its height
    // from it.
    if (m_cursor.block().length() == 1) {
        m_cursor.mergeBlockCharFormat(change);
    }
    m_cursor.mergeCharFormat(change);
    changed();
}

void TextTool::applyAlignment(Qt::Alignment alignment)
{
    if (!isEditing() || alignment == m_syncedAlignment) {
        return;
    }
    QTextBlockFormat format;
    format.setAlignment(alignment);
    m_cursor.mergeBlockFormat(format);
    changed();
}

void TextTool::refresh()
{
    if (isEditing()) {
        render();
    }
}

void TextTool::inputMethod(const QString &commit, const QString &preedit)
{
    if (!isEditing()) {
        return;
    }
    if (!commit.isEmpty()) {
        m_cursor.insertText(commit);
    }
    const QTextBlock block = m_cursor.block();
    block.layout()->setPreeditArea(preedit.isEmpty() ? -1 : m_cursor.positionInBlock(), preedit);
    m_document->markContentsDirty(block.position(), block.length());
    changed();
}

bool TextTool::keyPress(const KeyEvent &event)
{
    if (m_mode != Mode::Editing || m_drag != Drag::None) {
        return false;
    }
    const QTextCursor::MoveMode mode =
        event.modifiers & Qt::ShiftModifier ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor;
    const bool command = isCommand(event);
    const auto go = [this, mode](QTextCursor::MoveOperation operation) {
        m_cursor.movePosition(operation, mode);
        render();
        notify();
        syncFormat();
        return true;
    };
    switch (event.key) {
    case Qt::Key_Escape:
        commit();
        return true;
    case Qt::Key_Left:
        return go(command ? QTextCursor::WordLeft : QTextCursor::Left);
    case Qt::Key_Right:
        return go(command ? QTextCursor::WordRight : QTextCursor::Right);
    case Qt::Key_Up:
        return go(QTextCursor::Up);
    case Qt::Key_Down:
        return go(QTextCursor::Down);
    case Qt::Key_Home:
        return go(command ? QTextCursor::Start : QTextCursor::StartOfLine);
    case Qt::Key_End:
        return go(command ? QTextCursor::End : QTextCursor::EndOfLine);
    case Qt::Key_Backspace:
    case Qt::Key_Delete: {
        const bool back = event.key == Qt::Key_Backspace;
        if (!m_cursor.hasSelection() && command) {
            m_cursor.movePosition(back ? QTextCursor::WordLeft : QTextCursor::WordRight,
                                  QTextCursor::KeepAnchor);
        }
        if (m_cursor.hasSelection()) {
            m_cursor.removeSelectedText();
        } else if (back) {
            m_cursor.deletePreviousChar();
        } else {
            m_cursor.deleteChar();
        }
        changed();
        return true;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
        m_cursor.insertBlock();
        changed();
        return true;
    case Qt::Key_Tab:
        m_cursor.insertText(QStringLiteral("\t"));
        changed();
        return true;
    default:
        break;
    }
    if (command) {
        switch (event.key) {
        case Qt::Key_A:
            m_cursor.select(QTextCursor::Document);
            render();
            notify();
            syncFormat();
            return true;
        case Qt::Key_C:
            copySelection();
            return true;
        case Qt::Key_X:
            copySelection();
            m_cursor.removeSelectedText();
            changed();
            return true;
        case Qt::Key_V:
            paste();
            return true;
        case Qt::Key_Z:
            if (event.modifiers & Qt::ShiftModifier) {
                redo();
            } else if (!undo()) {
                cancel(); // nothing typed to undo: drop the box, like Undo from the menu
            }
            return true;
        case Qt::Key_Y:
            redo();
            return true;
        default:
            return false;
        }
    }
    if (typesText(event)) {
        m_cursor.insertText(event.text);
        changed();
        return true;
    }
    return false;
}

bool TextTool::wantsKey(const KeyEvent &event) const
{
    if (m_mode != Mode::Editing) {
        return false;
    }
    if (isEditingKey(event.key)) {
        return true;
    }
    return isCommand(event) ? isCommandKey(event.key) : typesText(event);
}

bool TextTool::undo()
{
    if (!isEditing() || !m_document->isUndoAvailable()) {
        return false;
    }
    m_document->undo(&m_cursor);
    changed();
    return true;
}

bool TextTool::redo()
{
    if (!isEditing() || !m_document->isRedoAvailable()) {
        return false;
    }
    m_document->redo(&m_cursor);
    changed();
    return true;
}

void TextTool::copySelection() const
{
    if (m_cursor.hasSelection() && qGuiApp) {
        QGuiApplication::clipboard()->setText(m_cursor.selection().toPlainText());
    }
}

void TextTool::paste()
{
    const QString text = qGuiApp ? QGuiApplication::clipboard()->text() : QString();
    if (!text.isEmpty()) {
        m_cursor.insertText(text);
        changed();
    }
}

void TextTool::finish()
{
    // The finished text without the selection highlight or unfinished input.
    m_cursor.block().layout()->setPreeditArea(-1, QString());
    render(false);
    const QImage &layer = m_context.document().layer(m_layer).image;
    if (!m_dirty.isEmpty() && m_before.copy(m_dirty) != layer.copy(m_dirty)) {
        m_context.history().recordPixelEdit(QCoreApplication::translate("paint::Tool", "Text"), m_layer,
                                            m_dirty, m_before.copy(m_dirty));
    }
    reset();
    notify();
}

void TextTool::commit()
{
    switch (m_mode) {
    case Mode::Idle:
        return;
    case Mode::Defining:
        reset();
        notify();
        return;
    case Mode::Editing:
        finish();
        return;
    }
}

void TextTool::cancel()
{
    if (m_mode == Mode::Idle) {
        return;
    }
    const QRect dirty = m_dirty;
    if (!dirty.isEmpty()) {
        QPainter painter(&m_context.document().layer(m_layer).image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(dirty.topLeft(), m_before, dirty);
    }
    reset();
    if (!dirty.isEmpty()) {
        m_context.documentChanged(dirty);
    }
    notify();
}

void TextTool::reset()
{
    m_mode = Mode::Idle;
    m_drag = Drag::None;
    m_handle = BoxHandle::None;
    m_cursor = QTextCursor();
    m_document.reset();
    m_before = {};
    m_dirty = {};
    m_shown = {};
    m_angle = 0;
}

QPainterPath TextTool::frameOutline() const
{
    if (m_mode == Mode::Idle) {
        return {};
    }
    QPainterPath frame;
    frame.addRect(m_frame);
    return m_angle == 0 ? frame : box::rotationAround(m_frame.center(), m_angle).map(frame);
}

QList<QPointF> TextTool::handlePositions(qreal screenPixel) const
{
    return isEditing() ? box::handles(m_frame, m_angle, screenPixel) : QList<QPointF>();
}

BoxHandle TextTool::hitTest(QPointF position, qreal screenPixel) const
{
    if (!isEditing()) {
        return BoxHandle::None;
    }
    if (const BoxHandle handle =
            box::hitTest(handlePositions(screenPixel), QPainterPath(), position, screenPixel);
        handle != BoxHandle::None) {
        return handle;
    }
    const QPointF local = toBox(position);
    const qreal band = BorderBand * screenPixel;
    const QRectF box(QPointF(), m_frame.size());
    const bool onBorder = box.adjusted(-band, -band, band, band).contains(local) &&
                          !box.adjusted(band, band, -band, -band).contains(local);
    return onBorder ? BoxHandle::Inside : BoxHandle::None;
}

bool TextTool::isOverText(QPointF position, qreal screenPixel) const
{
    if (!isEditing()) {
        return false;
    }
    const qreal band = BorderBand * screenPixel;
    return QRectF(QPointF(), m_frame.size()).adjusted(band, band, -band, -band).contains(toBox(position));
}

std::optional<QLineF> TextTool::caret() const
{
    if (!isEditing()) {
        return std::nullopt;
    }
    const QTextBlock block = m_cursor.block();
    const QTextLayout *layout = block.layout();
    if (!layout || layout->lineCount() == 0) {
        return std::nullopt;
    }
    const int position = m_cursor.positionInBlock();
    QTextLine line = layout->lineForTextPosition(position);
    if (!line.isValid()) {
        line = layout->lineAt(layout->lineCount() - 1);
    }
    const QPointF top = layout->position() + QPointF(line.cursorToX(position), line.y());
    const QTransform transform = toDocument();
    return QLineF(transform.map(top), transform.map(top + QPointF(0, line.height())));
}

QRectF TextTool::extent() const
{
    switch (m_mode) {
    case Mode::Idle:
        return {};
    case Mode::Defining:
        return m_frame;
    case Mode::Editing:
        break;
    }
    return box::rotationAround(m_frame.center(), m_angle).mapRect(m_frame);
}

QString TextTool::text() const
{
    return m_document ? m_document->toPlainText() : QString();
}

void TextTool::notify()
{
    m_context.selectionChanged();
}

} // namespace paint
