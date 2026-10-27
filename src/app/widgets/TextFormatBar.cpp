#include "widgets/TextFormatBar.h"

#include "ActionRegistry.h"
#include "tools/ToolSettings.h"

#include <QAction>
#include <QComboBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QIntValidator>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QToolButton>

#include <cmath>

using namespace paint;

namespace {

// Paint's font size list.
constexpr int Sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72};

} // namespace

TextFormatBar::TextFormatBar(ToolSettings *settings, ActionRegistry *actions, QWidget *parent)
    : QToolBar(tr("Text"), parent)
    , m_settings(settings)
    , m_font(new QFontComboBox(this))
    , m_size(new QComboBox(this))
{
    setObjectName(QStringLiteral("textFormatBar"));
    setMovable(false);
    setFloatable(false);
    setContextMenuPolicy(Qt::PreventContextMenu);

    m_font->setObjectName(QStringLiteral("textFormat.font"));
    m_font->setAccessibleName(tr("Font"));
    m_font->setToolTip(tr("Font"));
    m_font->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_font->setMinimumContentsLength(14);
    m_size->setObjectName(QStringLiteral("textFormat.size"));
    m_size->setAccessibleName(tr("Font size"));
    m_size->setToolTip(tr("Font size"));
    m_size->setEditable(true);
    m_size->setInsertPolicy(QComboBox::NoInsert);
    m_size->lineEdit()->setValidator(
        new QIntValidator(int(ToolSettings::MinTextSize), int(ToolSettings::MaxTextSize), m_size));
    for (int size : Sizes) {
        m_size->addItem(QString::number(size));
    }
    addWidget(m_font);
    addWidget(m_size);
    addSeparator();
    for (ActionId id :
         {ActionId::TextBold, ActionId::TextItalic, ActionId::TextUnderline, ActionId::TextStrikethrough}) {
        addAction(actions->get(id));
    }
    addSeparator();
    for (ActionId id : {ActionId::TextAlignLeft, ActionId::TextAlignCenter, ActionId::TextAlignRight}) {
        addAction(actions->get(id));
    }
    addSeparator();
    QAction *background = actions->get(ActionId::TextBackground);
    addAction(background);
    if (auto *button = qobject_cast<QToolButton *>(widgetForAction(background))) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }

    connect(m_font, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
        TextFormat format = m_settings->textFormat();
        format.family = font.family();
        m_settings->setTextFormat(format);
    });
    connect(m_font, &QComboBox::activated, this, &TextFormatBar::finished);
    connect(m_size, &QComboBox::activated, this, [this] {
        applySize();
        Q_EMIT finished();
    });
    connect(m_size->lineEdit(), &QLineEdit::returnPressed, this, [this] {
        applySize();
        Q_EMIT finished();
    });
    connect(m_size->lineEdit(), &QLineEdit::editingFinished, this, &TextFormatBar::applySize);
    connect(settings, &ToolSettings::textFormatChanged, this, &TextFormatBar::sync);
    sync();
}

void TextFormatBar::sync()
{
    const QSignalBlocker fontBlocker(m_font);
    const QSignalBlocker sizeBlocker(m_size);
    const TextFormat format = m_settings->textFormat();
    m_font->setCurrentFont(format.family.isEmpty() ? QFontDatabase::systemFont(QFontDatabase::GeneralFont)
                                                   : QFont(format.family));
    m_size->setCurrentText(QString::number(format.pointSize, 'g', 4));
}

void TextFormatBar::applySize()
{
    bool ok = false;
    const int size = m_size->currentText().toInt(&ok);
    if (!ok) {
        sync();
        return;
    }
    TextFormat format = m_settings->textFormat();
    format.pointSize = size;
    m_settings->setTextFormat(format);
}
