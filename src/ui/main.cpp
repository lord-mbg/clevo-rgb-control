#include "controller.h"

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QEvent>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <cstdio>
#include <unistd.h>

namespace {

QIcon makeTrayIcon()
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(0x1a, 0x1e, 0x24));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(4, 10, 56, 40), 6, 6);
    const QColor keys[] = {
        QColor(0xe74c3c), QColor(0x2ecc71), QColor(0x3498db),
        QColor(0xf1c40f), QColor(0x9b59b6), QColor(0x1abc9c),
    };
    for (int i = 0; i < 6; ++i) {
        p.setBrush(keys[i]);
        p.drawRoundedRect(QRectF(10 + i * 8.0, 18, 6.5, 10), 1.5, 1.5);
    }
    p.setBrush(QColor(0xec, 0xf0, 0xf1));
    p.drawRoundedRect(QRectF(18, 34, 28, 8), 2, 2);
    p.end();
    return QIcon(pm);
}

void showMainWindow(QQuickWindow* window)
{
    if (!window)
        return;
    window->show();
    window->raise();
    window->requestActivate();
}

} // namespace

class CloseToTrayFilter final : public QObject {
public:
    CloseToTrayFilter(QQuickWindow* window, bool* quitting)
        : m_window(window), m_quitting(quitting) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_window && event->type() == QEvent::Close && !*m_quitting) {
            if (QSystemTrayIcon::isSystemTrayAvailable()) {
                event->ignore();
                m_window->hide();
                return true;
            }
            QCoreApplication::quit();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QQuickWindow* m_window;
    bool* m_quitting;
};

int main(int argc, char** argv)
{
    if (::geteuid() == 0) {
        std::fputs("Run clevo-rgb-ui as your normal desktop user, not with sudo.\n", stderr);
        return 1;
    }
    QQuickStyle::setStyle("Basic");
    QApplication app(argc, argv);
    app.setApplicationName("clevo-rgb-ui");
    app.setOrganizationName("Clevo RGB Control");
    app.setApplicationDisplayName("Clevo RGB Control");
    const QIcon appIcon = makeTrayIcon();
    app.setWindowIcon(appIcon);

    QCommandLineParser parser;
    parser.setApplicationDescription("Desktop controls for the clevo-rgb Linux CLI.");
    parser.addHelpOption();
    parser.addOption({"device-dir", "Alternate LED directory (also for hardware-free smoke runs).", "path",
                      "/sys/class/leds/rgb:kbd_backlight"});
    parser.process(app);

    const QString cli = QDir(QCoreApplication::applicationDirPath()).filePath("clevo-rgb");
    Controller controller(cli, QDir::cleanPath(QFileInfo(parser.value("device-dir")).absoluteFilePath()));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl("qrc:/ui/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (!window)
        return 1;
    window->setIcon(appIcon);

    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    QMenu trayMenu;
    QSystemTrayIcon trayIcon;
    bool quitting = false;
    if (trayAvailable) {
        app.setQuitOnLastWindowClosed(false);

        auto* showAction = trayMenu.addAction(QObject::tr("Göster"));
        auto* hideAction = trayMenu.addAction(QObject::tr("Gizle"));
        trayMenu.addSeparator();
        auto* quitAction = trayMenu.addAction(QObject::tr("Çıkış"));

        QObject::connect(showAction, &QAction::triggered, &app, [window] { showMainWindow(window); });
        QObject::connect(hideAction, &QAction::triggered, &app, [window] { window->hide(); });
        QObject::connect(quitAction, &QAction::triggered, &app, [&app, &quitting] {
            quitting = true;
            app.quit();
        });

        trayIcon.setIcon(appIcon);
        trayIcon.setToolTip(QObject::tr("Clevo RGB Control"));
        trayIcon.setContextMenu(&trayMenu);
        trayIcon.show();

        QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &app,
                         [window](QSystemTrayIcon::ActivationReason reason) {
                             if (reason != QSystemTrayIcon::Trigger
                                 && reason != QSystemTrayIcon::DoubleClick) {
                                 return;
                             }
                             if (window->isVisible() && window->isActive())
                                 window->hide();
                             else
                                 showMainWindow(window);
                         });

        CloseToTrayFilter closeFilter(window, &quitting);
        window->installEventFilter(&closeFilter);
        const int result = app.exec();
        window->removeEventFilter(&closeFilter);
        return result;
    }

    return app.exec();
}
