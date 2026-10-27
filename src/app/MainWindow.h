#pragma once

#include "document/Document.h"

#include <QJsonObject>
#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Snapshot of the editor state reported to UI scenarios by the automation server.
    QJsonObject automationState() const;

private:
    void createMenus();
    void showAbout();

    paint::Document m_document;
};
