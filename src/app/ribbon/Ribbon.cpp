#include "ribbon/Ribbon.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int GroupMargin = 6;
constexpr int RibbonMargin = 4;
// Keeps the toolbar options chevron in the bottom-right corner clear of the groups.
constexpr int TrailingSpace = 28;

} // namespace

RibbonGroup::RibbonGroup(const QString &title, QWidget *content, QWidget *parent)
    : QWidget(parent)
    , m_content(content)
    , m_title(new QLabel(title, this))
{
    setObjectName(QStringLiteral("ribbon.group.") + title.toLower().remove(QLatin1Char(' ')));
    m_title->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    m_title->setForegroundRole(QPalette::PlaceholderText);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(GroupMargin, 4, GroupMargin, 2);
    layout->setSpacing(0);
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(content, 0, Qt::AlignCenter);
    layout->addLayout(row, 1);
    layout->addWidget(m_title);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
}

QString RibbonGroup::title() const
{
    return m_title->text();
}

void RibbonGroup::setCompactWidget(QWidget *widget)
{
    m_compactWidget = widget;
    widget->setParent(this);
    static_cast<QHBoxLayout *>(static_cast<QVBoxLayout *>(layout())->itemAt(0)->layout())
        ->addWidget(widget, 0, Qt::AlignCenter);
    widget->setVisible(m_compact);
}

void RibbonGroup::setCompact(bool compact)
{
    if (!m_compactWidget || compact == m_compact) {
        return;
    }
    m_compact = compact;
    m_content->setVisible(!compact);
    m_compactWidget->setVisible(compact);
}

int RibbonGroup::expandedWidth() const
{
    return std::max(m_content->sizeHint().width(), m_title->sizeHint().width()) + 2 * GroupMargin;
}

int RibbonGroup::compactWidth() const
{
    if (!m_compactWidget) {
        return expandedWidth();
    }
    return std::max(m_compactWidget->sizeHint().width(), m_title->sizeHint().width()) + 2 * GroupMargin;
}

Ribbon::Ribbon(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ribbon"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(RibbonMargin, 2, TrailingSpace, 3);
    layout->setSpacing(0);
    layout->addStretch(1);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
}

void Ribbon::addGroup(RibbonGroup *group, int collapseOrder)
{
    auto *layout = static_cast<QHBoxLayout *>(this->layout());
    const int stretchIndex = layout->count() - 1;
    if (!m_entries.isEmpty()) {
        auto *separator = new QFrame(this);
        separator->setFrameShape(QFrame::VLine);
        separator->setFrameShadow(QFrame::Sunken);
        separator->setContentsMargins(0, 6, 0, 6);
        layout->insertWidget(stretchIndex, separator);
        m_separators.append(separator);
    }
    layout->insertWidget(layout->count() - 1, group);
    m_entries.append({group, collapseOrder});
}

QList<RibbonGroup *> Ribbon::groups() const
{
    QList<RibbonGroup *> result;
    for (const Entry &entry : m_entries) {
        result.append(entry.group);
    }
    return result;
}

int Ribbon::widthFor(const QList<bool> &compact) const
{
    int width = RibbonMargin + TrailingSpace;
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        width +=
            compact.at(i) ? m_entries.at(i).group->compactWidth() : m_entries.at(i).group->expandedWidth();
    }
    for (QWidget *separator : m_separators) {
        width += separator->sizeHint().width();
    }
    return width;
}

QSize Ribbon::minimumSizeHint() const
{
    QList<bool> compact(m_entries.size());
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        compact[i] = m_entries.at(i).group->canCompact() && m_entries.at(i).collapseOrder >= 0;
    }
    return QSize(widthFor(compact), sizeHint().height());
}

void Ribbon::updateCompactness()
{
    if (m_updating || m_entries.isEmpty()) {
        return;
    }
    m_updating = true;

    QList<qsizetype> order;
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).collapseOrder >= 0 && m_entries.at(i).group->canCompact()) {
            order.append(i);
        }
    }
    std::ranges::sort(order, {}, [this](qsizetype i) { return m_entries.at(i).collapseOrder; });

    // Collapse groups in order until the expanded ones fit.
    QList<bool> compact(m_entries.size(), false);
    for (qsizetype next = 0; widthFor(compact) > width() && next < order.size(); ++next) {
        compact[order.at(next)] = true;
    }
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        m_entries.at(i).group->setCompact(compact.at(i));
    }
    m_updating = false;
}

void Ribbon::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateCompactness();
}

void Ribbon::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateCompactness();
}

void Ribbon::paintEvent(QPaintEvent *)
{
    // A hairline under the ribbon separates it from the workspace, as in Paint.
    QPainter painter(this);
    QColor line = palette().color(QPalette::WindowText);
    line.setAlphaF(0.12);
    painter.fillRect(QRect(0, height() - 1, width(), 1), line);
}
