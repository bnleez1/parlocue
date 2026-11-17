// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace {

double relativeLuminance(const QColor &c) {
    auto linear = [](double v) {
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(c.redF()) + 0.7152 * linear(c.greenF()) + 0.0722 * linear(c.blueF());
}

double contrastRatio(const QColor &a, const QColor &b) {
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor mixColor(const QColor &a, const QColor &b, double t) {
    const double clamped = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(
        a.redF() * (1.0 - clamped) + b.redF() * clamped,
        a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
        a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
        1.0
    );
}

QColor withAlpha(QColor c, double alpha) {
    c.setAlphaF(std::clamp(alpha, 0.0, 1.0));
    return c;
}

// Blend foreground toward targetFg until it meets minRatio against bg.
QColor ensureContrast(const QColor &fg, const QColor &bg, const QColor &targetFg, double minRatio) {
    if (contrastRatio(fg, bg) >= minRatio)
        return fg;
    double lo = 0.0;
    double hi = 1.0;
    for (int i = 0; i < 16; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (contrastRatio(mixColor(fg, targetFg, mid), bg) >= minRatio)
            hi = mid;
        else
            lo = mid;
    }
    return mixColor(fg, targetFg, hi);
}

QHash<QString, QString> parseTomlTokens(const QString &path) {
    QHash<QString, QString> tokens;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return tokens;

    const QRegularExpression rx(
        R"re(^\s*([a-zA-Z0-9_]+)\s*=\s*["']([^"']+)["'])re",
        QRegularExpression::MultilineOption
    );
    auto it = rx.globalMatch(QString::fromUtf8(file.readAll()));
    while (it.hasNext()) {
        const auto m = it.next();
        tokens.insert(m.captured(1), m.captured(2));
    }
    return tokens;
}

} // namespace

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    // Without Hyprland the corners stay sharp, like Omarchy's default.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    m_roundingPoll.setInterval(1000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();

    applyColorsToml(QString());
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

QString Theme::readAccent(const QString &path) {
    const auto tokens = parseTomlTokens(path);
    const QString accent = tokens.value("accent");
    if (QRegularExpression(QStringLiteral("^#[0-9a-fA-F]{6}$")).match(accent).hasMatch())
        return accent;
    return "#FFD60A";
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QString Theme::accentForeground() const {
    return relativeLuminance(QColor(m_accent)) > 0.179 ? "black" : "white";
}

bool Theme::applyColorsToml(const QString &path) {
    const auto tokens = path.isEmpty() ? QHash<QString, QString>() : parseTomlTokens(path);

    const QString newAccent = [&] {
        const QString val = tokens.value("accent");
        return QRegularExpression(QStringLiteral("^#[0-9a-fA-F]{6}$")).match(val).hasMatch()
            ? val : QStringLiteral("#FFD60A");
    }();
    const QColor accentCol(newAccent);

    // Warm Ristretto-latte light gradient so the workspace never sits on black,
    // while keeping deep espresso ink and elevated light cards at high contrast.
    const bool newIsDark = false;
    const QColor newBgTop("#F5EFEA");
    const QColor newBgMid("#E8DFDA");
    const QColor newBgBottom("#DAD0C9");

    const QColor safePrimary("#1E1717");
    const QColor safeSecondary("#433737");
    const QColor safeMuted("#5E5050");
    const QColor safeDim("#685959");

    const QColor newSurfaceBar = withAlpha(QColor("#EFE7E2"), 0.90);
    const QColor newSurfaceCard = withAlpha(QColor("#FAF6F3"), 0.88);
    const QColor newSurfaceCardSelected = withAlpha(QColor("#FFFFFF"), 0.96);
    const QColor newSurfaceCardPlaying = withAlpha(mixColor(QColor("#FFFFFF"), accentCol, 0.11), 0.96);
    const QColor newSurfaceInput("#FFFFFF");
    const QColor newSurfaceDialog("#FAF6F3");
    const QColor newSurfacePill("#ECE3DD");

    const QColor newBorder("#D4C7BF");
    const QColor newBorderStrong("#B5A59C");
    const QColor newButtonBg("#E7DDD7");
    const QColor newButtonHover("#DDD0C8");
    const QColor newButtonDown("#D0C1B8");
    const QColor newDisabledBg("#E2D9D3");
    const QColor newDisabledFg("#6E6060");

    // Ensure accent controls (primary buttons, sliders, checkboxes, active borders)
    // and accent labels both clear WCAG AA against light surfaces and white button text.
    const QColor newAccentControl = ensureContrast(accentCol, QColor("#FFFFFF"), safePrimary, 4.8);
    const QColor newAccentControlForeground("#FFFFFF");
    const QColor newAccentText = ensureContrast(accentCol, newBgBottom, safePrimary, 5.0);

    const QColor ok("#1B7A43");
    const QColor err("#C2274B");
    const QColor newBannerOkBg("#E3F2E9");
    const QColor newBannerOkBorder("#8BC9A4");
    const QColor newBannerErrorBg("#FCE8EC");
    const QColor newBannerErrorBorder("#E59AA8");
    const QColor newOverrideBg = mixColor(newSurfaceDialog, accentCol, 0.16);

    const bool dirty = (newAccent != m_accent) || (newIsDark != m_isDark)
        || (newAccentControl != m_accentControl)
        || (newAccentControlForeground != m_accentControlForeground)
        || (newAccentText != m_accentText)
        || (newBgTop != m_bgTop) || (newBgMid != m_bgMid) || (newBgBottom != m_bgBottom)
        || (newSurfaceBar != m_surfaceBar) || (newSurfaceCard != m_surfaceCard)
        || (newSurfaceCardSelected != m_surfaceCardSelected)
        || (newSurfaceCardPlaying != m_surfaceCardPlaying)
        || (newSurfaceInput != m_surfaceInput) || (newSurfaceDialog != m_surfaceDialog)
        || (newSurfacePill != m_surfacePill) || (newBorder != m_border)
        || (newBorderStrong != m_borderStrong) || (newButtonBg != m_buttonBg)
        || (newButtonHover != m_buttonHover) || (newButtonDown != m_buttonDown)
        || (newDisabledBg != m_disabledBg) || (newDisabledFg != m_disabledFg)
        || (safePrimary != m_textPrimary) || (safeSecondary != m_textSecondary)
        || (safeMuted != m_textMuted) || (safeDim != m_textDim)
        || (ok != m_statusOk) || (err != m_statusError)
        || (newBannerOkBg != m_bannerOkBg) || (newBannerOkBorder != m_bannerOkBorder)
        || (newBannerErrorBg != m_bannerErrorBg) || (newBannerErrorBorder != m_bannerErrorBorder)
        || (newOverrideBg != m_overrideBg);

    m_accent = newAccent;
    m_accentControl = newAccentControl;
    m_accentControlForeground = newAccentControlForeground;
    m_accentText = newAccentText;
    m_isDark = newIsDark;
    m_bgTop = newBgTop;
    m_bgMid = newBgMid;
    m_bgBottom = newBgBottom;
    m_surfaceBar = newSurfaceBar;
    m_surfaceCard = newSurfaceCard;
    m_surfaceCardSelected = newSurfaceCardSelected;
    m_surfaceCardPlaying = newSurfaceCardPlaying;
    m_surfaceInput = newSurfaceInput;
    m_surfaceDialog = newSurfaceDialog;
    m_surfacePill = newSurfacePill;
    m_border = newBorder;
    m_borderStrong = newBorderStrong;
    m_buttonBg = newButtonBg;
    m_buttonHover = newButtonHover;
    m_buttonDown = newButtonDown;
    m_disabledBg = newDisabledBg;
    m_disabledFg = newDisabledFg;
    m_textPrimary = safePrimary;
    m_textSecondary = safeSecondary;
    m_textMuted = safeMuted;
    m_textDim = safeDim;
    m_statusOk = ok;
    m_statusError = err;
    m_bannerOkBg = newBannerOkBg;
    m_bannerOkBorder = newBannerOkBorder;
    m_bannerErrorBg = newBannerErrorBg;
    m_bannerErrorBorder = newBannerErrorBorder;
    m_overrideBg = newOverrideBg;

    return dirty;
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    if (applyColorsToml(m_directory + "/theme/colors.toml"))
        emit changed();
}
