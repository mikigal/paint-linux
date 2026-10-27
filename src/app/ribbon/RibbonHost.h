#pragma once

#include <QWidget>

class QFrame;
class QMenu;
class QToolButton;
class Ribbon;

// Holds the ribbon and implements Paint's "Automatically hide toolbar" mode: the ribbon is
// replaced by a slim strip, and "Show toolbar" drops it down over the workspace until the
// next command or a click elsewhere.
class RibbonHost : public QWidget
{
    Q_OBJECT

public:
    // `options` is the menu behind the chevron button (Always show / Automatically hide).
    RibbonHost(Ribbon *ribbon, QMenu *options, QWidget *parent = nullptr);

    Ribbon *ribbon() const { return m_ribbon; }
    bool autoHide() const { return m_autoHide; }
    void setAutoHide(bool autoHide);

    void showRibbonPopup();
    void hideRibbonPopup();
    bool isRibbonPopupVisible() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void dockRibbon();

    Ribbon *m_ribbon;
    QWidget *m_strip;
    QToolButton *m_options;
    QFrame *m_popup;
    bool m_autoHide = false;
};
