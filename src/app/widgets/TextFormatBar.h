#pragma once

#include <QToolBar>

class ActionRegistry;
class QComboBox;
class QFontComboBox;

namespace paint {
class ToolSettings;
}

// Paint's text toolbar, shown under the ribbon while the text tool is active: font, size,
// bold/italic/underline/strikethrough, alignment and "Fill background". It edits the tool
// settings, which the text tool applies to the text being typed.
class TextFormatBar : public QToolBar
{
    Q_OBJECT

public:
    TextFormatBar(paint::ToolSettings *settings, ActionRegistry *actions, QWidget *parent = nullptr);

Q_SIGNALS:
    // A font or size was picked: typing can go on in the canvas.
    void finished();

private:
    void sync();
    void applySize();

    paint::ToolSettings *m_settings;
    QFontComboBox *m_font;
    QComboBox *m_size;
};
