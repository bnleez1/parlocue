#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QColor accentControl READ accentControl NOTIFY changed)
    Q_PROPERTY(QColor accentControlForeground READ accentControlForeground NOTIFY changed)
    Q_PROPERTY(QColor accentText READ accentText NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    Q_PROPERTY(bool isDark READ isDark NOTIFY changed)

    Q_PROPERTY(QColor bgTop READ bgTop NOTIFY changed)
    Q_PROPERTY(QColor bgMid READ bgMid NOTIFY changed)
    Q_PROPERTY(QColor bgBottom READ bgBottom NOTIFY changed)

    Q_PROPERTY(QColor surfaceBar READ surfaceBar NOTIFY changed)
    Q_PROPERTY(QColor surfaceCard READ surfaceCard NOTIFY changed)
    Q_PROPERTY(QColor surfaceCardSelected READ surfaceCardSelected NOTIFY changed)
    Q_PROPERTY(QColor surfaceCardPlaying READ surfaceCardPlaying NOTIFY changed)
    Q_PROPERTY(QColor surfaceInput READ surfaceInput NOTIFY changed)
    Q_PROPERTY(QColor surfaceDialog READ surfaceDialog NOTIFY changed)
    Q_PROPERTY(QColor surfacePill READ surfacePill NOTIFY changed)

    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor borderStrong READ borderStrong NOTIFY changed)
    Q_PROPERTY(QColor buttonBg READ buttonBg NOTIFY changed)
    Q_PROPERTY(QColor buttonHover READ buttonHover NOTIFY changed)
    Q_PROPERTY(QColor buttonDown READ buttonDown NOTIFY changed)
    Q_PROPERTY(QColor disabledBg READ disabledBg NOTIFY changed)
    Q_PROPERTY(QColor disabledFg READ disabledFg NOTIFY changed)

    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY changed)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY changed)
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY changed)
    Q_PROPERTY(QColor textDim READ textDim NOTIFY changed)

    Q_PROPERTY(QColor statusOk READ statusOk NOTIFY changed)
    Q_PROPERTY(QColor statusError READ statusError NOTIFY changed)
    Q_PROPERTY(QColor bannerOkBg READ bannerOkBg NOTIFY changed)
    Q_PROPERTY(QColor bannerOkBorder READ bannerOkBorder NOTIFY changed)
    Q_PROPERTY(QColor bannerErrorBg READ bannerErrorBg NOTIFY changed)
    Q_PROPERTY(QColor bannerErrorBorder READ bannerErrorBorder NOTIFY changed)
    Q_PROPERTY(QColor overrideBg READ overrideBg NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QColor accentControl() const { return m_accentControl; }
    QColor accentControlForeground() const { return m_accentControlForeground; }
    QColor accentText() const { return m_accentText; }
    int radius() const { return m_radius; }
    bool isDark() const { return m_isDark; }

    QColor bgTop() const { return m_bgTop; }
    QColor bgMid() const { return m_bgMid; }
    QColor bgBottom() const { return m_bgBottom; }

    QColor surfaceBar() const { return m_surfaceBar; }
    QColor surfaceCard() const { return m_surfaceCard; }
    QColor surfaceCardSelected() const { return m_surfaceCardSelected; }
    QColor surfaceCardPlaying() const { return m_surfaceCardPlaying; }
    QColor surfaceInput() const { return m_surfaceInput; }
    QColor surfaceDialog() const { return m_surfaceDialog; }
    QColor surfacePill() const { return m_surfacePill; }

    QColor border() const { return m_border; }
    QColor borderStrong() const { return m_borderStrong; }
    QColor buttonBg() const { return m_buttonBg; }
    QColor buttonHover() const { return m_buttonHover; }
    QColor buttonDown() const { return m_buttonDown; }
    QColor disabledBg() const { return m_disabledBg; }
    QColor disabledFg() const { return m_disabledFg; }

    QColor textPrimary() const { return m_textPrimary; }
    QColor textSecondary() const { return m_textSecondary; }
    QColor textMuted() const { return m_textMuted; }
    QColor textDim() const { return m_textDim; }

    QColor statusOk() const { return m_statusOk; }
    QColor statusError() const { return m_statusError; }
    QColor bannerOkBg() const { return m_bannerOkBg; }
    QColor bannerOkBorder() const { return m_bannerOkBorder; }
    QColor bannerErrorBg() const { return m_bannerErrorBg; }
    QColor bannerErrorBorder() const { return m_bannerErrorBorder; }
    QColor overrideBg() const { return m_overrideBg; }

    static QString readAccent(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();
    bool applyColorsToml(const QString &path);

    QString m_directory;
    QString m_accent = "#FFD60A";
    QColor m_accentControl;
    QColor m_accentControlForeground;
    QColor m_accentText;
    bool m_isDark = false;

    QColor m_bgTop;
    QColor m_bgMid;
    QColor m_bgBottom;
    QColor m_surfaceBar;
    QColor m_surfaceCard;
    QColor m_surfaceCardSelected;
    QColor m_surfaceCardPlaying;
    QColor m_surfaceInput;
    QColor m_surfaceDialog;
    QColor m_surfacePill;
    QColor m_border;
    QColor m_borderStrong;
    QColor m_buttonBg;
    QColor m_buttonHover;
    QColor m_buttonDown;
    QColor m_disabledBg;
    QColor m_disabledFg;
    QColor m_textPrimary;
    QColor m_textSecondary;
    QColor m_textMuted;
    QColor m_textDim;
    QColor m_statusOk;
    QColor m_statusError;
    QColor m_bannerOkBg;
    QColor m_bannerOkBorder;
    QColor m_bannerErrorBg;
    QColor m_bannerErrorBorder;
    QColor m_overrideBg;

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
