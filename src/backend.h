#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class QAudioOutput;
class QMediaPlayer;
class FilePicker;

struct Speaker {
    QString id;
    QString name;
    QString voice = "en-US-JennyNeural";
    int rate = 0;           // -50 to +100 (%)
    int pitch = 0;          // -50 to +50 (Hz)
    int defaultPauseMs = 600;
    QString color = "#FFD60A";
};

struct ScriptLine {
    QString id;
    QString speakerId;
    QString text;
    int pauseMs = 600;
    bool hasRateOverride = false;
    int rateOverride = 0;
    bool hasPitchOverride = false;
    int pitchOverride = 0;

    // Runtime synthesis state (not saved in .parlocue project JSON).
    QString clipState = "idle"; // idle | synthesizing | ready | error
    QString clipError;
    qint64 durationMs = 0;
};

struct VoiceEntry {
    QString shortName;
    QString displayName;
    QString friendlyName;
    QString locale;
    QString gender;
    QStringList categories;
    QStringList personalities;
    bool isFavorite = false;
};

struct ProjectSnapshot {
    QString title;
    int defaultPauseMs = 600;
    QVector<Speaker> speakers;
    QVector<ScriptLine> lines;
};

class SpeakerModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        VoiceRole,
        VoiceDisplayRole,
        RateRole,
        PitchRole,
        DefaultPauseMsRole,
        ColorRole
    };

    explicit SpeakerModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSpeakers(const QVector<Speaker> &speakers);
    void setVoiceDisplayLookup(const QHash<QString, QString> &lookup);
    void notifyUpdated(int row);

    const QVector<Speaker> &items() const { return m_speakers; }
    QVector<Speaker> &items() { return m_speakers; }

private:
    QVector<Speaker> m_speakers;
    QHash<QString, QString> m_voiceDisplays;
};

class ScriptLineModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        SpeakerIdRole,
        SpeakerNameRole,
        SpeakerColorRole,
        SpeakerVoiceRole,
        SpeakerVoiceDisplayRole,
        TextRole,
        PauseMsRole,
        HasRateOverrideRole,
        RateOverrideRole,
        HasPitchOverrideRole,
        PitchOverrideRole,
        ClipStateRole,
        ClipErrorRole,
        DurationMsRole
    };

    explicit ScriptLineModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setLines(const QVector<ScriptLine> &lines);
    void insertLine(int row, const ScriptLine &line);
    void removeLineAt(int row);
    void moveLineAt(int fromRow, int toRow);
    void setSpeakers(const QVector<Speaker> *speakers);
    void notifyUpdated(int row);
    void notifySpeakerChanged();

    const QVector<ScriptLine> &items() const { return m_lines; }
    QVector<ScriptLine> &items() { return m_lines; }

private:
    const Speaker *findSpeaker(const QString &id) const;

    QVector<ScriptLine> m_lines;
    const QVector<Speaker> *m_speakers = nullptr;
};

class VoiceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filterChanged)
    Q_PROPERTY(QString localeFilter READ localeFilter WRITE setLocaleFilter NOTIFY filterChanged)
    Q_PROPERTY(QString genderFilter READ genderFilter WRITE setGenderFilter NOTIFY filterChanged)
    Q_PROPERTY(bool favoritesOnly READ favoritesOnly WRITE setFavoritesOnly NOTIFY filterChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY filterChanged)

public:
    enum Roles {
        ShortNameRole = Qt::UserRole + 1,
        DisplayNameRole,
        FriendlyNameRole,
        LocaleRole,
        GenderRole,
        PersonalitiesRole,
        CategoriesRole,
        IsFavoriteRole
    };

    explicit VoiceModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setAllVoices(const QVector<VoiceEntry> &voices);
    void setFavorites(const QStringList &favorites);
    QStringList favorites() const { return m_favorites; }

    QString search() const { return m_search; }
    void setSearch(const QString &s);
    QString localeFilter() const { return m_localeFilter; }
    void setLocaleFilter(const QString &l);
    QString genderFilter() const { return m_genderFilter; }
    void setGenderFilter(const QString &g);
    bool favoritesOnly() const { return m_favoritesOnly; }
    void setFavoritesOnly(bool f);
    int totalCount() const { return m_allVoices.size(); }

    const QVector<VoiceEntry> &allVoices() const { return m_allVoices; }

signals:
    void filterChanged();

private:
    void rebuildFiltered();

    QVector<VoiceEntry> m_allVoices;
    QVector<VoiceEntry> m_filtered;
    QStringList m_favorites;
    QString m_search;
    QString m_localeFilter = "en";
    QString m_genderFilter;
    bool m_favoritesOnly = false;
};

class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(SpeakerModel *speakerModel READ speakerModel CONSTANT)
    Q_PROPERTY(ScriptLineModel *lineModel READ lineModel CONSTANT)
    Q_PROPERTY(VoiceModel *voiceModel READ voiceModel CONSTANT)

    Q_PROPERTY(QString projectTitle READ projectTitle WRITE setProjectTitle NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY projectChanged)
    Q_PROPERTY(int defaultPauseMs READ defaultPauseMs WRITE setDefaultPauseMs NOTIFY projectChanged)
    Q_PROPERTY(int selectedSpeakerIndex READ selectedSpeakerIndex WRITE setSelectedSpeakerIndex NOTIFY selectionChanged)
    Q_PROPERTY(int selectedLineIndex READ selectedLineIndex WRITE setSelectedLineIndex NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedSpeaker READ selectedSpeaker NOTIFY selectedSpeakerChanged)
    Q_PROPERTY(QVariantMap selectedLine READ selectedLine NOTIFY selectedLineChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY linesChanged)
    Q_PROPERTY(int wordCount READ wordCount NOTIFY linesChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

    Q_PROPERTY(bool ttsAvailable READ ttsAvailable NOTIFY ttsStatusChanged)
    Q_PROPERTY(bool ttsChecking READ ttsChecking NOTIFY ttsStatusChanged)
    Q_PROPERTY(QString ttsStatusMessage READ ttsStatusMessage NOTIFY ttsStatusChanged)
    Q_PROPERTY(bool ffmpegAvailable READ ffmpegAvailable NOTIFY ttsStatusChanged)
    Q_PROPERTY(bool voicesLoading READ voicesLoading NOTIFY voicesChanged)

    Q_PROPERTY(bool busy READ busy NOTIFY workChanged)
    Q_PROPERTY(int workCompleted READ workCompleted NOTIFY workChanged)
    Q_PROPERTY(int workTotal READ workTotal NOTIFY workChanged)
    Q_PROPERTY(QString workMessage READ workMessage NOTIFY workChanged)

    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(bool playingFullScript READ playingFullScript NOTIFY playbackChanged)
    Q_PROPERTY(int playingLineIndex READ playingLineIndex NOTIFY playbackChanged)
    Q_PROPERTY(qint64 playbackPositionMs READ playbackPositionMs NOTIFY playbackChanged)
    Q_PROPERTY(qint64 playbackDurationMs READ playbackDurationMs NOTIFY playbackChanged)

    Q_PROPERTY(QString statusBanner READ statusBanner NOTIFY bannerChanged)
    Q_PROPERTY(bool statusBannerIsError READ statusBannerIsError NOTIFY bannerChanged)

public:
    explicit Backend(FilePicker *picker = nullptr, const QString &cacheDir = {},
                     const QString &pythonOverride = {}, QObject *parent = nullptr);
    ~Backend() override;

    SpeakerModel *speakerModel() { return &m_speakerModel; }
    ScriptLineModel *lineModel() { return &m_lineModel; }
    VoiceModel *voiceModel() { return &m_voiceModel; }

    QString projectTitle() const { return m_title; }
    void setProjectTitle(const QString &title);
    QString projectPath() const { return m_projectPath; }
    bool dirty() const { return m_dirty; }
    int defaultPauseMs() const { return m_defaultPauseMs; }
    void setDefaultPauseMs(int ms);
    int selectedSpeakerIndex() const { return m_selectedSpeakerIndex; }
    void setSelectedSpeakerIndex(int idx);
    int selectedLineIndex() const { return m_selectedLineIndex; }
    void setSelectedLineIndex(int idx);
    int lineCount() const { return m_lineModel.items().size(); }
    int wordCount() const;
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }

    bool ttsAvailable() const { return m_ttsAvailable; }
    bool ttsChecking() const { return m_ttsChecking; }
    QString ttsStatusMessage() const { return m_ttsStatusMessage; }
    bool ffmpegAvailable() const { return m_ffmpegAvailable; }
    bool voicesLoading() const { return m_voicesLoading; }

    bool busy() const { return m_busy; }
    int workCompleted() const { return m_workCompleted; }
    int workTotal() const { return m_workTotal; }
    QString workMessage() const { return m_workMessage; }

    bool playing() const { return m_playing; }
    bool playingFullScript() const { return m_playingFullScript; }
    int playingLineIndex() const { return m_playingLineIndex; }
    qint64 playbackPositionMs() const { return m_playbackPositionMs; }
    qint64 playbackDurationMs() const { return m_playbackDurationMs; }

    QString statusBanner() const { return m_statusBanner; }
    bool statusBannerIsError() const { return m_statusBannerIsError; }

    // Primary / transport / undo actions
    Q_INVOKABLE void primaryAction();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void saveUndoPoint();
    Q_INVOKABLE void dismissBanner();

    QVariantMap selectedSpeaker() const;
    QVariantMap selectedLine() const;

    // Project lifecycle
    Q_INVOKABLE void newProject();
    Q_INVOKABLE void openProjectDialog();
    Q_INVOKABLE void saveProjectDialog();
    Q_INVOKABLE void saveProject();
    Q_INVOKABLE bool loadFromFile(const QString &path, QString *errorOut = nullptr);
    Q_INVOKABLE bool saveToFile(const QString &path, QString *errorOut = nullptr);

    // Script editing
    Q_INVOKABLE void addLine(int afterIndex = -1, const QString &speakerId = {}, const QString &text = {});
    Q_INVOKABLE void duplicateLine(int index);
    Q_INVOKABLE void removeLine(int index);
    Q_INVOKABLE void moveLine(int fromIndex, int toIndex);
    Q_INVOKABLE void setLineText(int index, const QString &text);
    Q_INVOKABLE void commitLineText(int index, const QString &text);
    Q_INVOKABLE void setLineSpeaker(int index, const QString &speakerId);
    Q_INVOKABLE void cycleLineSpeaker(int index);
    Q_INVOKABLE void setLinePause(int index, int pauseMs);
    Q_INVOKABLE void setLineRateOverride(int index, bool enabled, int rate);
    Q_INVOKABLE void setLinePitchOverride(int index, bool enabled, int pitch);
    Q_INVOKABLE void applyDefaultPauseToAll();

    // Speaker editing
    Q_INVOKABLE void addSpeaker(const QString &name = {});
    Q_INVOKABLE void removeSpeaker(int index);
    Q_INVOKABLE void setSpeakerName(int index, const QString &name);
    Q_INVOKABLE void setSpeakerVoice(int index, const QString &shortName);
    Q_INVOKABLE void randomizeSpeakerVoices();
    Q_INVOKABLE void setSpeakerRate(int index, int rate);
    Q_INVOKABLE void setSpeakerPitch(int index, int pitch);
    Q_INVOKABLE void setSpeakerDefaultPause(int index, int pauseMs);
    Q_INVOKABLE int speakerIndexForId(const QString &speakerId) const;

    // Voice catalog & sampling
    Q_INVOKABLE void refreshVoices();
    Q_INVOKABLE void toggleFavoriteVoice(const QString &shortName);
    Q_INVOKABLE void sampleVoice(const QString &shortName, int rate = 0, int pitch = 0);
    Q_INVOKABLE QString voiceDisplayName(const QString &shortName) const;

    // Script import / parsing
    Q_INVOKABLE QVariantMap parseScriptPreview(const QString &rawText, bool detectSpeakers,
                                               const QString &fallbackSpeakerId = {}) const;
    Q_INVOKABLE void importScriptText(const QString &rawText, bool detectSpeakers,
                                      const QString &fallbackSpeakerId, bool replaceExisting,
                                      bool randomizeVoices = false);
    Q_INVOKABLE void importDocumentDialog();
    bool extractDocumentText(const QString &path, QString *textOut, QString *errorOut) const;

    // Playback & synthesis
    Q_INVOKABLE void playLine(int index);
    Q_INVOKABLE void playAll();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void seekPlayback(qint64 positionMs);
    Q_INVOKABLE void cancelWork();

    // Export
    Q_INVOKABLE void exportCombinedDialog(int bitrateKbps = 192, bool normalize = false);
    Q_INVOKABLE void exportClipsDialog(int bitrateKbps = 192);
    Q_INVOKABLE void exportBothDialog(int bitrateKbps = 192, bool normalize = false);

    // TTS environment
    Q_INVOKABLE void checkEnvironment();
    Q_INVOKABLE void installTts();

    // Pure helpers used by CLI and unit tests
    static QString formatRate(int ratePercent);
    static QString formatPitch(int pitchHz);
    static QString clipCacheKey(const QString &text, const QString &voice, int rate, int pitch);
    static QString sanitizeFilePart(const QString &name);
    static QString buildFfmpegFilterScript(const QVector<int> &pauseMsPerClip, bool normalize);
    Q_INVOKABLE static QString formatDuration(qint64 ms);

    QString clipPathForLine(const ScriptLine &line) const;
    bool exportCombinedSync(const QString &outputPath, int bitrateKbps, bool normalize, QString *errorOut);
    bool exportClipsSync(const QString &dirPath, int bitrateKbps, QString *errorOut);
    bool synthesizeAllSync(QString *errorOut);
    bool fetchVoicesSync(QString *errorOut);

signals:
    void projectChanged();
    void selectionChanged();
    void selectedSpeakerChanged();
    void selectedLineChanged();
    void linesChanged();
    void historyChanged();
    void ttsStatusChanged();
    void voicesChanged();
    void workChanged();
    void playbackChanged();
    void bannerChanged();
    void exportCompleted(bool success, const QString &message);
    void documentTextExtracted(const QString &text, const QString &fileName);

private:
    struct SynthTask {
        int lineIndex = -1;
        QString cacheKey;
        QString text;
        QString voice;
        QString rate;
        QString pitch;
        QString outPath;
    };

    enum class PendingAction {
        None,
        PlayAll,
        ExportCombined,
        ExportClips,
        ExportBoth
    };

    struct PlaybackSegment {
        int lineIndex = -1;
        qint64 startMs = 0;
        qint64 endMs = 0;
    };

    void initAudio();
    void ensureBridgeExtracted();
    QString resolveTtsPython() const;
    void loadCachedVoices();
    void saveCachedVoices(const QJsonArray &arr);
    void loadFavorites();
    void saveFavorites();
    void updateClipStatesFromCache();
    void pushUndoSnapshot();
    ProjectSnapshot captureSnapshot() const;
    void restoreSnapshot(const ProjectSnapshot &snap);
    void markDirty();
    void setBanner(const QString &msg, bool isError = false);

    const Speaker *speakerById(const QString &id) const;
    Speaker *mutableSpeakerById(const QString &id);
    QString pickVoiceForSpeakerName(const QString &name, int existingCount) const;
    QString pickRandomVoice(const QString &speakerName, const QSet<QString> &usedVoices,
                            const QSet<QString> &usedLocales, const QString &avoidVoice = {}) const;
    QString voiceLocale(const QString &shortName) const;
    QString nextSpeakerColor(int index) const;
    QString newId(const QString &prefix);

    void startBatchSynthesis(const QVector<int> &lineIndices, PendingAction nextAction);
    void pumpSynthesisQueue();
    void finishBatchSynthesis();
    void runStitchForPreview();
    void runExportCombined(const QString &outPath);
    void runExportClips(const QString &dirPath);
    qint64 probeDurationMs(const QString &filePath) const;
    bool runFfmpegStitch(const QStringList &clipPaths, const QVector<int> &pausesMs,
                         const QString &outPath, int bitrateKbps, bool normalize, QString *errorOut) const;

    FilePicker *m_picker = nullptr;
    bool m_ownsPicker = false;
    QString m_cacheDir;
    QString m_clipsDir;
    QString m_bridgeScriptPath;
    QString m_pythonOverride;
    QString m_ttsPython;
    QString m_ffmpegBin;
    QString m_ffprobeBin;

    SpeakerModel m_speakerModel;
    ScriptLineModel m_lineModel;
    VoiceModel m_voiceModel;

    QString m_title = "Untitled Script";
    QString m_projectPath;
    bool m_dirty = false;
    int m_defaultPauseMs = 600;
    int m_selectedSpeakerIndex = 0;
    int m_selectedLineIndex = 0;
    int m_idCounter = 1;

    QVector<ProjectSnapshot> m_undoStack;
    QVector<ProjectSnapshot> m_redoStack;

    bool m_ttsAvailable = false;
    bool m_ttsChecking = false;
    bool m_ffmpegAvailable = false;
    QString m_ttsStatusMessage;
    bool m_voicesLoading = false;

    bool m_busy = false;
    int m_workCompleted = 0;
    int m_workTotal = 0;
    QString m_workMessage;

    QVector<SynthTask> m_synthQueue;
    QList<QProcess *> m_activeSynthProcs;
    QPointer<QProcess> m_exportProc;
    QPointer<QProcess> m_voicesProc;
    PendingAction m_pendingAction = PendingAction::None;
    QString m_pendingCombinedPath;
    QString m_pendingClipsDir;
    int m_pendingBitrate = 192;
    bool m_pendingNormalize = false;
    QString m_pendingSinglePlayKey;
    int m_pendingSinglePlayLine = -1;

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    bool m_playing = false;
    bool m_playingFullScript = false;
    int m_playingLineIndex = -1;
    qint64 m_playbackPositionMs = 0;
    qint64 m_playbackDurationMs = 0;
    QVector<PlaybackSegment> m_playbackSegments;

    QString m_statusBanner;
    bool m_statusBannerIsError = false;
};
