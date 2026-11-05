#pragma once

#include <QColor>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>

class Controller final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool deviceAvailable READ deviceAvailable NOTIFY deviceChanged)
    Q_PROPERTY(int maxBrightness READ maxBrightness NOTIFY deviceChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY deviceChanged)
    Q_PROPERTY(QColor currentColor READ currentColor NOTIFY deviceChanged)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool error READ error NOTIFY statusChanged)
public:
    explicit Controller(QString cliPath, QString deviceDir, QObject* parent = nullptr);
    bool busy() const { return m_process.state() != QProcess::NotRunning; }
    bool deviceAvailable() const { return m_deviceAvailable; }
    int maxBrightness() const { return m_maxBrightness; }
    int brightness() const { return m_brightness; }
    QColor currentColor() const { return m_color; }
    QStringList profiles() const { return m_profiles; }
    QString statusMessage() const { return m_status; }
    bool error() const { return m_error; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void apply(const QString& color, const QString& targetColor, int brightness,
                           const QString& effect, int periodMs);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void saveProfile(const QString& name, const QString& color,
                                 const QString& targetColor, int brightness,
                                 const QString& effect, int periodMs);
    Q_INVOKABLE void applyProfile(const QString& name);
    Q_INVOKABLE void deleteProfile(const QString& name);
    Q_INVOKABLE QVariantMap profileSettings(const QString& name);
signals:
    void busyChanged();
    void deviceChanged();
    void profilesChanged();
    void statusChanged();
private:
    QStringList lightingArguments(const QString& color, const QString& targetColor,
                                  int brightness, const QString& effect, int periodMs);
    void run(QStringList arguments, bool hardware, const QString& successMessage);
    void readDevice();
    bool readProfiles();
    void setStatus(const QString& message, bool error = false);
    QString m_cliPath;
    QString m_deviceDir;
    QProcess m_process;
    bool m_deviceAvailable{false};
    int m_maxBrightness{255};
    int m_brightness{0};
    QColor m_color{84, 106, 202};
    QStringList m_profiles;
    QString m_status;
    QString m_successMessage;
    bool m_error{false};
};
