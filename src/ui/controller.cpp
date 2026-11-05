#include "controller.h"
#include "../profiles.h"

#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QTextStream>
#include <limits>
#include <utility>
#include <unistd.h>

Controller::Controller(QString cliPath, QString deviceDir, QObject* parent)
    : QObject(parent), m_cliPath(std::move(cliPath)), m_deviceDir(std::move(deviceDir)) {
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_statusProcess.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::stateChanged, this, &Controller::busyChanged);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            setStatus(QStringLiteral("Komut başlatılamadı: ") + m_process.errorString(), true);
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exitStatus) {
        const QString output = QString::fromUtf8(m_process.readAll()).trimmed();
        if (code != 0 || exitStatus != QProcess::NormalExit) {
            setStatus(output.isEmpty() ? QStringLiteral("İşlem başarısız oldu veya yetkilendirme iptal edildi.")
                                      : output, true);
            probeEffectStatus();
            return;
        }
        readDevice();
        if (readProfiles()) setStatus(m_successMessage);
        probeEffectStatus();
    });
    connect(&m_statusProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            setEffectStatus(QStringLiteral("unknown"));
    });
    connect(&m_statusProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exitStatus) {
        const QString state = QString::fromUtf8(m_statusProcess.readAllStandardOutput()).trimmed();
        if (code != 0 || exitStatus != QProcess::NormalExit) {
            setEffectStatus(QStringLiteral("unknown"));
            return;
        }
        if (state == QLatin1String("active"))
            setEffectStatus(QStringLiteral("active"));
        else if (state == QLatin1String("inactive"))
            setEffectStatus(QStringLiteral("inactive"));
        else
            setEffectStatus(QStringLiteral("unknown"));
    });
    m_effectStatusTimer.setInterval(5000);
    connect(&m_effectStatusTimer, &QTimer::timeout, this, &Controller::probeEffectStatus);
    m_effectStatusTimer.start();
    refresh();
}

Controller::~Controller() {
    if (m_statusProcess.state() != QProcess::NotRunning) {
        m_statusProcess.disconnect(this);
        m_statusProcess.kill();
        m_statusProcess.waitForFinished(1000);
    }
}

void Controller::setStatus(const QString& message, bool error) {
    m_status = message;
    m_error = error;
    emit statusChanged();
}

void Controller::readDevice() {
    QFile colorFile(m_deviceDir + "/multi_intensity");
    QFile brightnessFile(m_deviceDir + "/brightness");
    QFile maximumFile(m_deviceDir + "/max_brightness");
    int r = -1, g = -1, b = -1;
    qlonglong brightness = -1, maximum = -1;
    bool valid = colorFile.open(QIODevice::ReadOnly) && brightnessFile.open(QIODevice::ReadOnly)
                 && maximumFile.open(QIODevice::ReadOnly);
    if (valid) {
        QTextStream colors(&colorFile), levels(&brightnessFile), limits(&maximumFile);
        colors >> r >> g >> b;
        levels >> brightness;
        limits >> maximum;
        valid = colors.status() == QTextStream::Ok && levels.status() == QTextStream::Ok
                && limits.status() == QTextStream::Ok && r >= 0 && r <= 255 && g >= 0
                && g <= 255 && b >= 0 && b <= 255 && maximum > 0
                && maximum <= std::numeric_limits<int>::max() && brightness >= 0
                && brightness <= maximum;
    }
    m_deviceAvailable = valid;
    if (valid) {
        m_color = QColor(r, g, b);
        m_brightness = static_cast<int>(brightness);
        m_maxBrightness = static_cast<int>(maximum);
    }
    emit deviceChanged();
}

bool Controller::readProfiles() {
    const auto names = list_profiles();
    if (!names) {
        setStatus(QStringLiteral("Profiller okunamadı. Yapılandırma dizininin sahipliğini ve izinlerini kontrol et."), true);
        return false;
    }
    QStringList profiles;
    for (const auto& name : *names) profiles.append(QString::fromStdString(name));
    if (profiles != m_profiles) {
        m_profiles = std::move(profiles);
        emit profilesChanged();
    }
    return true;
}

void Controller::refresh() {
    probeEffectStatus();
    if (busy()) return;
    readDevice();
    if (readProfiles())
        setStatus(m_deviceAvailable ? QStringLiteral("Hazır. Ayarlar yalnızca Uygula ile klavyeye gönderilir.")
                                   : QStringLiteral("Cihaz bulunamadı. Profil kaydetme ve yönetimi kullanılabilir."));
}

QStringList Controller::lightingArguments(const QString& color, const QString& targetColor,
                                         int brightness, const QString& effect, int periodMs) {
    if (effect != "static" && effect != "breathe" && effect != "cycle" && effect != "transition") {
        setStatus(QStringLiteral("Geçersiz efekt."), true);
        return {};
    }
    if (brightness < 0 || brightness > m_maxBrightness ||
        (effect != "static" && (periodMs < 100 || periodMs > 3600000))) {
        setStatus(QStringLiteral("Parlaklık veya efekt süresi izin verilen aralığın dışında."), true);
        return {};
    }
    QStringList arguments;
    const auto appendColor = [&](const QString& flag, const QString& text) {
        const QColor parsed(text);
        if (!parsed.isValid() || parsed.alpha() != 255) return false;
        arguments << flag << QString::number(parsed.red()) << QString::number(parsed.green())
                  << QString::number(parsed.blue());
        return true;
    };
    if (effect != "cycle" && !appendColor("--color", color)) {
        setStatus(QStringLiteral("Geçerli bir RGB rengi seç."), true);
        return {};
    }
    if (effect == "transition" && !appendColor("--to-color", targetColor)) {
        setStatus(QStringLiteral("Geçiş için geçerli bir ikinci renk seç."), true);
        return {};
    }
    arguments << "--brightness" << QString::number(brightness);
    if (effect != "static")
        arguments << "--effect" << effect << "--period-ms" << QString::number(periodMs);
    return arguments;
}

bool Controller::needsPrivilegedCli() const {
    // Same decision as hardware elevation: /sys or a non-writable brightness node
    // means the detached effect lives under the system manager, not --user.
    return m_deviceDir.startsWith(QLatin1String("/sys/")) ||
           (::access(QFile::encodeName(m_deviceDir + QLatin1String("/brightness")).constData(), W_OK) != 0);
}

void Controller::setEffectStatus(const QString& status) {
    if (m_effectStatus == status) return;
    m_effectStatus = status;
    emit effectStatusChanged();
}

void Controller::probeEffectStatus() {
    if (m_statusProcess.state() != QProcess::NotRunning) return;
    const QString systemctl = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
    if (systemctl.isEmpty()) {
        setEffectStatus(QStringLiteral("unknown"));
        return;
    }
    QStringList arguments;
    if (!needsPrivilegedCli())
        arguments << QStringLiteral("--user");
    // Read-only query; never prompt for credentials merely to inspect ActiveState.
    arguments << QStringLiteral("--no-ask-password") << QStringLiteral("show")
              << QStringLiteral("--property=ActiveState") << QStringLiteral("--value")
              << QStringLiteral("clevo-rgb-effect.service");
    m_statusProcess.start(systemctl, arguments);
}

void Controller::run(QStringList arguments, bool hardware, const QString& successMessage) {
    if (busy()) return;
    if (!QFileInfo(m_cliPath).isExecutable()) {
        setStatus(QStringLiteral("clevo-rgb çalıştırılabilir dosyası bulunamadı: ") + m_cliPath, true);
        return;
    }
    QString program = m_cliPath;
    if (hardware) {
        // LED operations on sysfs use the system manager, as with sudo CLI invocations.
        // User-writable temporary device directories use the user manager for smoke runs.
        const bool privileged = needsPrivilegedCli();
        arguments.prepend(m_deviceDir);
        arguments.prepend(QStringLiteral("--device-dir"));
        if (privileged) {
            program = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
            if (program.isEmpty()) {
                setStatus(QStringLiteral("pkexec bulunamadı. Polkit ve masaüstü yetkilendirme ajanı gerekiyor."), true);
                return;
            }
            arguments.prepend(m_cliPath);
        }
    }
    m_successMessage = successMessage;
    setStatus(hardware ? QStringLiteral("İşlem sürüyor; gerekirse masaüstü yetkilendirmesini onayla…")
                       : QStringLiteral("Profil işlemi sürüyor…"));
    m_process.start(program, arguments);
}

void Controller::apply(const QString& color, const QString& targetColor, int brightness,
                       const QString& effect, int periodMs) {
    if (busy()) return;
    if (!m_deviceAvailable) {
        setStatus(QStringLiteral("Klavye LED arayüzü okunamıyor; ayarlar uygulanmadı."), true);
        return;
    }
    const auto arguments = lightingArguments(color, targetColor, brightness, effect, periodMs);
    if (!arguments.isEmpty())
        run(arguments, true, effect == "static" ? QStringLiteral("Renk ve parlaklık uygulandı.")
                                               : QStringLiteral("Efekt arka planda başlatıldı."));
}

void Controller::stop() {
    run({"--stop"}, true, QStringLiteral("Efekt durduruldu; önceki ayarlar geri yüklendi."));
}

void Controller::saveProfile(const QString& name, const QString& color, const QString& targetColor,
                             int brightness, const QString& effect, int periodMs) {
    if (busy()) return;
    auto arguments = lightingArguments(color, targetColor, brightness, effect, periodMs);
    if (arguments.isEmpty()) return;
    arguments.prepend(name);
    arguments.prepend("--save-profile");
    run(arguments, false, QStringLiteral("Profil kaydedildi: ") + name);
}

void Controller::applyProfile(const QString& name) {
    if (busy()) return;
    if (!m_deviceAvailable) {
        setStatus(QStringLiteral("Klavye LED arayüzü okunamıyor; profil uygulanmadı."), true);
        return;
    }
    // Resolve the user's profile before elevation. pkexec does not provide SUDO_UID
    // and must never switch this lookup to root's profile store.
    const auto profile = load_profile(name.toStdString());
    if (!profile) {
        setStatus(QStringLiteral("Profil okunamadı veya güvenli bir kullanıcı profili değil: ") + name, true);
        return;
    }
    QStringList arguments;
    for (const auto& argument : *profile) arguments.append(QString::fromStdString(argument));
    run(arguments, true, QStringLiteral("Profil uygulandı: ") + name);
}

void Controller::deleteProfile(const QString& name) {
    run({"--delete-profile", name}, false, QStringLiteral("Profil silindi: ") + name);
}

QVariantMap Controller::profileSettings(const QString& name) {
    const auto profile = load_profile(name.toStdString());
    if (!profile) {
        setStatus(QStringLiteral("Profil okunamadı: ") + name, true);
        return {};
    }
    QVariantMap settings{
        {QStringLiteral("color"), m_color},
        {QStringLiteral("targetColor"), QColor(255, 0, 0)},
        {QStringLiteral("brightness"), m_brightness},
        {QStringLiteral("effect"), QStringLiteral("static")},
        {QStringLiteral("periodMs"), 3000}
    };
    const auto& args = *profile;
    for (std::size_t i = 0; i < args.size();) {
        const QString option = QString::fromStdString(args[i++]);
        if (option == QStringLiteral("--palette") || option == QStringLiteral("--duration-ms")) {
            setStatus(QStringLiteral("Bu profil özel palet/süre içeriyor. Profili Uygula özgün ayarları kullanır; düzenleyiciye yüklenmedi."), true);
            return {};
        }
        if (option == QStringLiteral("--color") || option == QStringLiteral("--to-color")) {
            if (args.size() - i < 3) break;
            bool okR, okG, okB;
            const int r = QString::fromStdString(args[i++]).toInt(&okR);
            const int g = QString::fromStdString(args[i++]).toInt(&okG);
            const int b = QString::fromStdString(args[i++]).toInt(&okB);
            if (!okR || !okG || !okB || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) break;
            settings.insert(option == QStringLiteral("--color") ? QStringLiteral("color")
                                                                 : QStringLiteral("targetColor"), QColor(r, g, b));
        } else {
            if (i == args.size()) break;
            const QString value = QString::fromStdString(args[i++]);
            if (option == QStringLiteral("--effect")) {
                if (value != QStringLiteral("breathe") && value != QStringLiteral("cycle")
                    && value != QStringLiteral("transition")) break;
                settings.insert(QStringLiteral("effect"), value);
            } else if (option == QStringLiteral("--brightness") || option == QStringLiteral("--period-ms")) {
                bool ok;
                const int number = value.toInt(&ok);
                if (!ok || number < 0 || (option == QStringLiteral("--brightness") && number > m_maxBrightness)
                    || (option == QStringLiteral("--period-ms") && (number < 100 || number > 3600000))) break;
                settings.insert(option == QStringLiteral("--brightness") ? QStringLiteral("brightness")
                                                                          : QStringLiteral("periodMs"), number);
            } else {
                break;
            }
        }
        if (i == args.size()) {
            setStatus(QStringLiteral("Profil düzenleyiciye yüklendi: ") + name);
            return settings;
        }
    }
    setStatus(QStringLiteral("Profil ayarları geçersiz: ") + name, true);
    return {};
}
