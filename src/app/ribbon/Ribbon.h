#pragma once

#include <QList>
#include <QWidget>

class QLabel;

// A titled group of ribbon controls. Groups that have a compact form (usually one button with
// the group's commands in its menu) switch to it when the window gets too narrow, like Paint's.
class RibbonGroup : public QWidget
{
    Q_OBJECT

public:
    RibbonGroup(const QString &title, QWidget *content, QWidget *parent = nullptr);

    QString title() const;
    void setCompactWidget(QWidget *widget);
    bool canCompact() const { return m_compactWidget != nullptr; }
    bool isCompact() const { return m_compact; }
    void setCompact(bool compact);

    int expandedWidth() const;
    int compactWidth() const;

private:
    QWidget *m_content;
    QWidget *m_compactWidget = nullptr;
    QLabel *m_title;
    bool m_compact = false;
};

// Paint's one-row command ribbon: groups separated by thin lines, collapsing in a fixed
// order as space runs out.
class Ribbon : public QWidget
{
    Q_OBJECT

public:
    explicit Ribbon(QWidget *parent = nullptr);

    // Groups with a lower `collapseOrder` collapse first; -1 never collapses.
    void addGroup(RibbonGroup *group, int collapseOrder = -1);
    QList<RibbonGroup *> groups() const;

    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct Entry
    {
        RibbonGroup *group;
        int collapseOrder;
    };

    void updateCompactness();
    int widthFor(const QList<bool> &compact) const;

    QList<Entry> m_entries;
    QList<QWidget *> m_separators;
    bool m_updating = false;
};
