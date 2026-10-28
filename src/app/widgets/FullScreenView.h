#pragma once

#include <QImage>
#include <QWidget>

// Paint's full screen view (F11): the picture alone on black, until a key press or a click.
class FullScreenView : public QWidget
{
    Q_OBJECT

public:
    FullScreenView(const QImage &picture, QWidget *parent);

Q_SIGNALS:
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QImage m_picture;
};
