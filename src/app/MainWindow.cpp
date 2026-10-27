#include "MainWindow.h"

#include "PaintConfig.h"

#include <QApplication>
#include <QJsonArray>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

namespace {

// Provisional; M2 picks the default canvas size the way Paint does.
constexpr QSize DefaultCanvasSize(1152, 648);
constexpr QSize DefaultWindowSize(1280, 800);

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_document(DefaultCanvasSize)
{
    setObjectName(QStringLiteral("mainWindow"));
    // Qt appends the application display name ("— Paint for Linux").
    setWindowTitle(tr("Untitled") + QStringLiteral("[*]"));
    resize(DefaultWindowSize);

    createMenus();

    // Placeholder until the workspace and canvas view land in M1.
    auto *workspace = new QWidget(this);
    workspace->setObjectName(QStringLiteral("workspace"));
    workspace->setBackgroundRole(QPalette::Dark);
    workspace->setAutoFillBackground(true);
    setCentralWidget(workspace);

    auto *canvasSize =
        new QLabel(tr("%1 × %2px").arg(m_document.size().width()).arg(m_document.size().height()), this);
    canvasSize->setObjectName(QStringLiteral("status.canvasSize"));
    statusBar()->setObjectName(QStringLiteral("statusBar"));
    statusBar()->addWidget(canvasSize);
}

void MainWindow::createMenus()
{
    menuBar()->setObjectName(QStringLiteral("menuBar"));

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->setObjectName(QStringLiteral("menu.file"));
    QAction *quit = fileMenu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), tr("&Quit"),
                                        QKeySequence::Quit, this, &QWidget::close);
    quit->setObjectName(QStringLiteral("action.file.quit"));
    quit->setMenuRole(QAction::QuitRole);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->setObjectName(QStringLiteral("menu.help"));
    QAction *about = helpMenu->addAction(QIcon::fromTheme(QStringLiteral("help-about")),
                                         tr("&About %1").arg(QApplication::applicationDisplayName()), this,
                                         &MainWindow::showAbout);
    about->setObjectName(QStringLiteral("action.help.about"));
    about->setMenuRole(QAction::AboutRole);
    QAction *aboutQt = helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQt->setObjectName(QStringLiteral("action.help.aboutQt"));
    aboutQt->setMenuRole(QAction::AboutQtRole);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About %1").arg(QApplication::applicationDisplayName()),
                       tr("<h3>%1 %2</h3><p>%3.</p>")
                           .arg(QApplication::applicationDisplayName(), QApplication::applicationVersion(),
                                QStringLiteral(PAINT_DESCRIPTION)));
}

QJsonObject MainWindow::automationState() const
{
    QJsonArray layers;
    for (qsizetype i = 0; i < m_document.layerCount(); ++i) {
        const paint::Layer &layer = m_document.layer(i);
        layers.append(
            QJsonObject{{QStringLiteral("name"), layer.name}, {QStringLiteral("visible"), layer.visible}});
    }
    return QJsonObject{
        {QStringLiteral("canvasSize"), QJsonArray{m_document.size().width(), m_document.size().height()}},
        {QStringLiteral("backgroundColor"), m_document.backgroundColor().name(QColor::HexArgb)},
        {QStringLiteral("backgroundVisible"), m_document.isBackgroundVisible()},
        {QStringLiteral("activeLayer"), m_document.activeLayerIndex()},
        {QStringLiteral("layers"), layers},
        {QStringLiteral("modified"), isWindowModified()},
    };
}
