#include "ui/Accessibility.h"

#include <QEvent>
#include <QWidget>

namespace paint::ui {

namespace {

class ToolTipNamer : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::ToolTipChange) {
            name(static_cast<QWidget *>(watched));
        }
        return false;
    }

    static void name(QWidget *widget)
    {
        // Rich-text tooltips would be read out with their markup; ours are plain.
        widget->setAccessibleName(widget->toolTip());
    }
};

} // namespace

void nameAfterToolTip(QWidget *widget)
{
    widget->installEventFilter(new ToolTipNamer(widget));
    ToolTipNamer::name(widget);
}

} // namespace paint::ui
