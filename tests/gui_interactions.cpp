#include "../src/ui/controller.h"

#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

namespace {

QString g_cliPath;
QString g_uiDir;


QString readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

template <typename T>
T* childByName(QObject* root, const char* name)
{
    return root->findChild<T*>(QLatin1String(name), Qt::FindChildrenRecursively);
}

QQuickItem* itemByName(QObject* root, const char* name)
{
    return childByName<QQuickItem>(root, name);
}

bool clickItem(QQuickItem* item)
{
    if (!item)
        return false;
    // AbstractButton exposes click(); plain Items fall back to the clicked signal.
    if (QMetaObject::invokeMethod(item, "click", Qt::DirectConnection))
        return true;
    return QMetaObject::invokeMethod(item, "clicked", Qt::DirectConnection);
}

QQuickItem* buttonWithText(QObject* root, const QString& text)
{
    const auto items = root->findChildren<QQuickItem*>(QString(), Qt::FindChildrenRecursively);
    for (QQuickItem* item : items) {
        if (item->property("text").toString() == text)
            return item;
    }
    return nullptr;
}

} // namespace

class GuiInteractionsTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void editorProfileSelectionLeavesDeviceUntouched();
    void applyWritesTemporaryLedFiles();
    void saveDeleteAndConfirmThroughQml();
    void refreshPreservesUnsavedEditorSettings();

private:
    void writeDevice(const QString& color, const QString& brightness);
    void waitIdle();
    void loadUi();
    void unloadUi();

    QTemporaryDir m_root;
    QString m_deviceDir;
    QString m_configHome;
    Controller* m_controller = nullptr;
    QQmlApplicationEngine* m_engine = nullptr;
    QQuickWindow* m_window = nullptr;
};

void GuiInteractionsTest::writeDevice(const QString& color, const QString& brightness)
{
    QVERIFY(QFile::exists(m_deviceDir + QStringLiteral("/max_brightness")));
    QFile colorFile(m_deviceDir + QStringLiteral("/multi_intensity"));
    QVERIFY(colorFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    QTextStream(&colorFile) << color;
    colorFile.close();
    QFile brightnessFile(m_deviceDir + QStringLiteral("/brightness"));
    QVERIFY(brightnessFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    QTextStream(&brightnessFile) << brightness;
    brightnessFile.close();
}

void GuiInteractionsTest::waitIdle()
{
    QTRY_VERIFY_WITH_TIMEOUT(m_controller && !m_controller->busy(), 20000);
}

void GuiInteractionsTest::loadUi()
{
    QVERIFY(m_engine == nullptr);
    m_controller = new Controller(g_cliPath, m_deviceDir);
    m_engine = new QQmlApplicationEngine;
    m_engine->addImportPath(g_uiDir);
    m_engine->rootContext()->setContextProperty(QStringLiteral("controller"), m_controller);
    m_engine->load(QUrl::fromLocalFile(QDir(g_uiDir).filePath(QStringLiteral("Main.qml"))));
    QVERIFY2(!m_engine->rootObjects().isEmpty(), "Main.qml failed to load");
    m_window = qobject_cast<QQuickWindow*>(m_engine->rootObjects().constFirst());
    QVERIFY(m_window);
    QTRY_VERIFY(itemByName(m_window, "brightnessSlider") != nullptr);
    waitIdle();
    QTRY_COMPARE(m_controller->effectStatus(), QStringLiteral("inactive"));
    auto* indicator = itemByName(m_window, "effectStatusLabel");
    auto* stopButton = itemByName(m_window, "stopButton");
    QVERIFY(indicator && stopButton);
    QCOMPARE(indicator->property("text").toString(), QStringLiteral("Efekt yok"));
    QVERIFY(!stopButton->property("enabled").toBool());
}

void GuiInteractionsTest::unloadUi()
{
    delete m_engine;
    m_engine = nullptr;
    m_window = nullptr;
    delete m_controller;
    m_controller = nullptr;
}

void GuiInteractionsTest::initTestCase()
{
    g_cliPath = qEnvironmentVariable("CLEVO_TEST_CLI_PATH");
    g_uiDir = qEnvironmentVariable("CLEVO_TEST_UI_DIR");
    QVERIFY2(!g_cliPath.isEmpty() && !g_uiDir.isEmpty(), "CTest must provide the CLI and UI paths");
    QVERIFY(QFileInfo(g_cliPath).isExecutable());
    QVERIFY(QFileInfo(QDir(g_uiDir).filePath(QStringLiteral("Main.qml"))).isFile());

    QVERIFY(m_root.isValid());
    m_configHome = m_root.filePath(QStringLiteral("config"));
    m_deviceDir = m_root.filePath(QStringLiteral("led"));
    QVERIFY(QDir().mkpath(m_configHome));
    QVERIFY(QDir().mkpath(m_deviceDir));
    QVERIFY(!m_deviceDir.startsWith(QLatin1String("/sys/")));

    qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_configHome));
    qputenv("HOME", QFile::encodeName(m_root.path()));
    qunsetenv("SUDO_UID");
    // Qt's offscreen platform is selected by CTest before QApplication starts.

    QFile maxFile(m_deviceDir + QStringLiteral("/max_brightness"));
    QVERIFY(maxFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    QTextStream(&maxFile) << "255\n";
    maxFile.close();
    // The CLI asks systemctl whether a detached effect exists before static writes.
    // Isolate that query so this test cannot stop a user's real service.
    const QString toolsDir = m_root.filePath(QStringLiteral("tools"));
    QVERIFY(QDir().mkpath(toolsDir));
    QFile systemctl(QDir(toolsDir).filePath(QStringLiteral("systemctl")));
    QVERIFY(systemctl.open(QIODevice::WriteOnly | QIODevice::Text));
    systemctl.write("#!/bin/sh\ncase \" $* \" in *\" show \"*) printf 'inactive\\n';; *) exit 90;; esac\n");
    systemctl.close();
    QVERIFY(systemctl.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner));
    qputenv("PATH", QFile::encodeName(toolsDir + QDir::listSeparator() + qEnvironmentVariable("PATH")));
    writeDevice(QStringLiteral("23 45 67\n"), QStringLiteral("80\n"));

    // No user/system service is started or stopped by this suite.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}


void GuiInteractionsTest::init()
{
    writeDevice(QStringLiteral("23 45 67\n"), QStringLiteral("80\n"));
    // Isolate profile storage between tests.
    QDir(m_configHome).removeRecursively();
    QVERIFY(QDir().mkpath(m_configHome));
    loadUi();
    QVERIFY(m_controller->deviceAvailable());
    QCOMPARE(m_controller->brightness(), 80);
    QCOMPARE(m_controller->currentColor(), QColor(23, 45, 67));
}
void GuiInteractionsTest::cleanup()
{
    unloadUi();
}

void GuiInteractionsTest::editorProfileSelectionLeavesDeviceUntouched()
{
    auto* nameField = itemByName(m_window, "profileNameField");
    auto* saveButton = itemByName(m_window, "saveProfileButton");
    auto* effectCombo = itemByName(m_window, "effectCombo");
    auto* periodSpin = itemByName(m_window, "periodSpin");
    auto* brightness = itemByName(m_window, "brightnessSlider");
    auto* hexField = itemByName(m_window, "hexField");
    auto* targetHex = itemByName(m_window, "targetHexField");
    QVERIFY(nameField && saveButton && effectCombo && periodSpin && brightness && hexField && targetHex);

    // Prepare distinct editor settings, then save without applying to the LED files.
    QVERIFY(effectCombo->setProperty("currentIndex", 3)); // transition
    QVERIFY(periodSpin->setProperty("value", 1500));
    QVERIFY(brightness->setProperty("value", 64));
    QVERIFY(hexField->setProperty("text", QStringLiteral("#112233")));
    QMetaObject::invokeMethod(hexField, "editingFinished", Qt::DirectConnection);
    QVERIFY(targetHex->setProperty("text", QStringLiteral("#AABBCC")));
    QMetaObject::invokeMethod(targetHex, "editingFinished", Qt::DirectConnection);

    QVERIFY(nameField->setProperty("text", QStringLiteral("desk")));
    QVERIFY(clickItem(saveButton));
    waitIdle();
    QVERIFY2(!m_controller->error(), qPrintable(m_controller->statusMessage()));
    QVERIFY(m_controller->profiles().contains(QStringLiteral("desk")));
    QCOMPARE(readFile(m_deviceDir + QStringLiteral("/multi_intensity")), QStringLiteral("23 45 67\n"));
    QCOMPARE(readFile(m_deviceDir + QStringLiteral("/brightness")), QStringLiteral("80\n"));

    // Mutate the editor away from the saved profile, then select the profile in QML.
    QVERIFY(effectCombo->setProperty("currentIndex", 0));
    QVERIFY(brightness->setProperty("value", 10));
    QVERIFY(hexField->setProperty("text", QStringLiteral("#0000FF")));
    QMetaObject::invokeMethod(hexField, "editingFinished", Qt::DirectConnection);

    auto* profileCombo = itemByName(m_window, "profileCombo");
    QVERIFY(profileCombo);
    const int index = m_controller->profiles().indexOf(QStringLiteral("desk"));
    QVERIFY(index >= 0);
    QVERIFY(profileCombo->setProperty("currentIndex", index));
    QVERIFY(QMetaObject::invokeMethod(profileCombo, "activated", Qt::DirectConnection, Q_ARG(int, index)));
    QTest::qWait(50);

    QCOMPARE(m_window->property("selectedEffect").toString(), QStringLiteral("transition"));
    QCOMPARE(m_window->property("periodMs").toInt(), 1500);
    QCOMPARE(m_window->property("selectedBrightness").toInt(), 64);
    QCOMPARE(m_window->property("selectedColor").value<QColor>(), QColor(0x11, 0x22, 0x33));
    QCOMPARE(m_window->property("targetColor").value<QColor>(), QColor(0xaa, 0xbb, 0xcc));
    // Profile load is editor-only: the temporary LED files must remain untouched.
    QCOMPARE(readFile(m_deviceDir + QStringLiteral("/multi_intensity")), QStringLiteral("23 45 67\n"));
    QCOMPARE(readFile(m_deviceDir + QStringLiteral("/brightness")), QStringLiteral("80\n"));
}

void GuiInteractionsTest::applyWritesTemporaryLedFiles()
{
    auto* effectCombo = itemByName(m_window, "effectCombo");
    auto* brightness = itemByName(m_window, "brightnessSlider");
    auto* hexField = itemByName(m_window, "hexField");
    auto* applyButton = itemByName(m_window, "applyButton");
    QVERIFY(effectCombo && brightness && hexField && applyButton);

    QVERIFY(effectCombo->setProperty("currentIndex", 0)); // static
    QVERIFY(brightness->setProperty("value", 42));
    QVERIFY(hexField->setProperty("text", QStringLiteral("#FF8800")));
    QMetaObject::invokeMethod(hexField, "editingFinished", Qt::DirectConnection);
    QVERIFY(clickItem(applyButton));
    waitIdle();
    QVERIFY2(!m_controller->error(), qPrintable(m_controller->statusMessage()));

    QCOMPARE(readFile(m_deviceDir + QStringLiteral("/multi_intensity")).trimmed(), QStringLiteral("255 136 0"));
    QCOMPARE(readFile(m_deviceDir + QStringLiteral("/brightness")).trimmed(), QStringLiteral("42"));
    QCOMPARE(m_controller->currentColor(), QColor(255, 136, 0));
    QCOMPARE(m_controller->brightness(), 42);
    // Static apply uses the real CLI against the temp device only (no pkexec /sys path).
    QVERIFY(!m_deviceDir.startsWith(QLatin1String("/sys/")));
}

void GuiInteractionsTest::saveDeleteAndConfirmThroughQml()
{
    auto* nameField = itemByName(m_window, "profileNameField");
    auto* saveButton = itemByName(m_window, "saveProfileButton");
    auto* deleteButton = itemByName(m_window, "deleteProfileButton");
    auto* confirmDialog = childByName<QObject>(m_window, "confirmDialog");
    auto* confirmButton = itemByName(m_window, "confirmButton");
    auto* effectCombo = itemByName(m_window, "effectCombo");
    auto* brightness = itemByName(m_window, "brightnessSlider");
    auto* hexField = itemByName(m_window, "hexField");
    QVERIFY(nameField && saveButton && deleteButton && confirmDialog && confirmButton);
    QVERIFY(effectCombo && brightness && hexField);

    QVERIFY(effectCombo->setProperty("currentIndex", 1)); // breathe
    QVERIFY(brightness->setProperty("value", 55));
    QVERIFY(hexField->setProperty("text", QStringLiteral("#010203")));
    QMetaObject::invokeMethod(hexField, "editingFinished", Qt::DirectConnection);
    QVERIFY(nameField->setProperty("text", QStringLiteral("night")));
    QVERIFY(clickItem(saveButton));
    waitIdle();
    QVERIFY(m_controller->profiles().contains(QStringLiteral("night")));

    // Overwrite confirmation path.
    QVERIFY(brightness->setProperty("value", 77));
    QVERIFY(nameField->setProperty("text", QStringLiteral("night")));
    QVERIFY(clickItem(saveButton));
    QTRY_VERIFY(confirmDialog->property("opened").toBool() || confirmDialog->property("visible").toBool());
    QVERIFY(clickItem(confirmButton));
    waitIdle();
    QVERIFY2(!m_controller->error(), qPrintable(m_controller->statusMessage()));

    const QVariantMap settings = m_controller->profileSettings(QStringLiteral("night"));
    QVERIFY(!settings.isEmpty());
    QCOMPARE(settings.value(QStringLiteral("brightness")).toInt(), 77);
    QCOMPARE(settings.value(QStringLiteral("effect")).toString(), QStringLiteral("breathe"));

    auto* profileCombo = itemByName(m_window, "profileCombo");
    QVERIFY(profileCombo);
    const int index = m_controller->profiles().indexOf(QStringLiteral("night"));
    QVERIFY(index >= 0);
    QVERIFY(profileCombo->setProperty("currentIndex", index));
    QVERIFY(clickItem(deleteButton));
    QTRY_VERIFY(confirmDialog->property("opened").toBool() || confirmDialog->property("visible").toBool());
    QVERIFY(clickItem(confirmButton));
    waitIdle();
    QVERIFY2(!m_controller->error(), qPrintable(m_controller->statusMessage()));
    QVERIFY(!m_controller->profiles().contains(QStringLiteral("night")));
    QCOMPARE(readFile(m_deviceDir + QStringLiteral("/multi_intensity")), QStringLiteral("23 45 67\n"));
    QCOMPARE(readFile(m_deviceDir + QStringLiteral("/brightness")), QStringLiteral("80\n"));
}

void GuiInteractionsTest::refreshPreservesUnsavedEditorSettings()
{
    auto* effectCombo = itemByName(m_window, "effectCombo");
    auto* periodSpin = itemByName(m_window, "periodSpin");
    auto* brightness = itemByName(m_window, "brightnessSlider");
    auto* hexField = itemByName(m_window, "hexField");
    QVERIFY(effectCombo && periodSpin && brightness && hexField);

    QVERIFY(effectCombo->setProperty("currentIndex", 2)); // cycle
    QVERIFY(periodSpin->setProperty("value", 2400));
    QVERIFY(brightness->setProperty("value", 33));
    QVERIFY(hexField->setProperty("text", QStringLiteral("#C0FFEE")));
    QMetaObject::invokeMethod(hexField, "editingFinished", Qt::DirectConnection);

    // Device changes underneath the editor; Yenile must re-read device without clobbering editor state.
    writeDevice(QStringLiteral("9 8 7\n"), QStringLiteral("12\n"));
    QQuickItem* refresh = buttonWithText(m_window, QStringLiteral("↻  Yenile"));
    QVERIFY2(refresh, "refresh button not found");
    QVERIFY(clickItem(refresh));
    waitIdle();

    QCOMPARE(m_controller->currentColor(), QColor(9, 8, 7));
    QCOMPARE(m_controller->brightness(), 12);
    QCOMPARE(m_window->property("selectedEffect").toString(), QStringLiteral("cycle"));
    QCOMPARE(m_window->property("periodMs").toInt(), 2400);
    QCOMPARE(m_window->property("selectedBrightness").toInt(), 33);
    QCOMPARE(m_window->property("selectedColor").value<QColor>(), QColor(0xc0, 0xff, 0xee));
}


QTEST_MAIN(GuiInteractionsTest)
#include "gui_interactions.moc"
