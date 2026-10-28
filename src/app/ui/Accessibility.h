#pragma once

class QWidget;

namespace paint::ui {

// Names an icon-only control after its tooltip for screen readers, and keeps the name in step
// when the tooltip changes (a custom colour slot that gets a colour, say).
void nameAfterToolTip(QWidget *widget);

} // namespace paint::ui
