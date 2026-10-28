#include "MainWindow.h"
#include "PaintConfig.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QProcess>
#include <QUrl>

#ifdef PAINT_AUTOMATION_ENABLED
#include "automation/AutomationServer.h"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral(PAINT_EXECUTABLE));
    QApplication::setApplicationDisplayName(QStringLiteral(PAINT_APP_NAME));
    QApplication::setApplicationVersion(QStringLiteral(PAINT_VERSION));
    QApplication::setOrganizationDomain(QStringLiteral("mikigal.pl"));
    QGuiApplication::setDesktopFileName(QStringLiteral(PAINT_APP_ID));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(PAINT_APP_ID),
                                                 QIcon(QStringLiteral(":/paint/app/" PAINT_APP_ID ".svg"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(PAINT_DESCRIPTION));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QApplication::translate("main", "Image to open."),
                                 QStringLiteral("[file]"));
#ifdef PAINT_AUTOMATION_ENABLED
    const QCommandLineOption automationOption(
        QStringLiteral("automation-socket"),
        QApplication::translate("main", "Listen for automation commands on the given local socket."),
        QStringLiteral("path"), qEnvironmentVariable("PAINT_AUTOMATION_SOCKET"));
    parser.addOption(automationOption);
#endif
    parser.process(app);

    MainWindow window;
    window.show();
    if (const QStringList files = parser.positionalArguments(); !files.isEmpty()) {
        // Arguments may be paths or file:// URLs. "Open with" may pass several images: this window
        // takes the first, each of the others opens in a window of its own, as in Paint.
        const auto localPath = [](const QString &argument) {
            const QUrl url = QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile);
            return url.isLocalFile() ? url.toLocalFile() : argument;
        };
        window.openFile(localPath(files.first()));
        for (const QString &file : files.mid(1)) {
            QProcess::startDetached(QApplication::applicationFilePath(), {localPath(file)});
        }
    }

#ifdef PAINT_AUTOMATION_ENABLED
    AutomationServer automation;
    if (const QString socketPath = parser.value(automationOption); !socketPath.isEmpty()) {
        automation.setStateProvider([&window] { return window.automationState(); });
        if (!automation.listen(socketPath)) {
            return 1;
        }
    }
#endif

    return QApplication::exec();
}
