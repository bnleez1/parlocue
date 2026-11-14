#include "backend.h"
#include "portalfilepicker.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <algorithm>

namespace {

const QStringList kSpeakerPalette = {
    "#4ECDC4", "#FFD60A", "#FF6B6B", "#A78BFA", "#34D399", "#F59E0B", "#60A5FA", "#F472B6"
};

const QStringList kFemaleVoices = {
    "en-US-JennyNeural", "en-US-AriaNeural", "en-GB-SoniaNeural", "en-US-EmmaNeural",
    "en-US-AvaNeural", "en-AU-NatashaNeural", "en-CA-ClaraNeural", "en-US-MichelleNeural"
};

const QStringList kMaleVoices = {
    "en-US-GuyNeural", "en-US-DavisNeural", "en-GB-RyanNeural", "en-US-BrianNeural",
    "en-US-AndrewNeural", "en-AU-WilliamNeural", "en-CA-LiamNeural", "en-US-ChristopherNeural"
};

struct BuiltinVoice {
    const char *shortName;
    const char *display;
    const char *locale;
    const char *gender;
    const char *personalities;
};

const BuiltinVoice kDefaultVoices[] = {
    {"en-US-JennyNeural", "Jenny", "en-US", "Female", "Friendly, Considerate, Comfort"},
    {"en-US-GuyNeural", "Guy", "en-US", "Male", "Passion"},
    {"en-US-AriaNeural", "Aria", "en-US", "Female", "Positive, Confident"},
    {"en-US-DavisNeural", "Davis", "en-US", "Male", "Soothing, Calm"},
    {"en-US-EmmaNeural", "Emma", "en-US", "Female", "Cheerful, Clear, Conversational"},
    {"en-US-BrianNeural", "Brian", "en-US", "Male", "Approachable, Casual, Sincere"},
    {"en-US-AvaNeural", "Ava", "en-US", "Female", "Expressive, Caring, Pleasant"},
    {"en-US-AndrewNeural", "Andrew", "en-US", "Male", "Warm, Confident, Authentic"},
    {"en-US-ChristopherNeural", "Christopher", "en-US", "Male", "Reliable, Authority"},
    {"en-US-MichelleNeural", "Michelle", "en-US", "Female", "Friendly, Pleasant"},
    {"en-GB-SoniaNeural", "Sonia", "en-GB", "Female", "Friendly, Positive"},
    {"en-GB-RyanNeural", "Ryan", "en-GB", "Male", "Friendly, Positive"},
    {"en-GB-LibbyNeural", "Libby", "en-GB", "Female", "Friendly, Positive"},
    {"en-GB-ThomasNeural", "Thomas", "en-GB", "Male", "Friendly, Positive"},
    {"en-AU-NatashaNeural", "Natasha", "en-AU", "Female", "Friendly, Positive"},
    {"en-AU-WilliamNeural", "William", "en-AU", "Male", "Friendly, Positive"},
    {"en-CA-ClaraNeural", "Clara", "en-CA", "Female", "Friendly, Positive"},
    {"en-CA-LiamNeural", "Liam", "en-CA", "Male", "Friendly, Positive"},
};

QString cleanDisplayName(const QString &shortName, const QString &friendlyName) {
    // FriendlyName looks like: "Microsoft Jenny Online (Natural) - English (United States)"
    if (!friendlyName.isEmpty()) {
        static const QRegularExpression re(R"(^Microsoft\s+(.+?)\s+(?:Online|Multilingual))");
        const auto m = re.match(friendlyName);
        if (m.hasMatch())
            return m.captured(1).trimmed();
    }
    // Fallback from shortName: en-US-JennyNeural -> Jenny
    const QStringList parts = shortName.split('-');
    if (parts.size() >= 3) {
        QString name = parts.mid(2).join(' ');
        name.remove("Neural");
        return name.trimmed();
    }
    return shortName;
}

int parsePauseTagMs(const QString &line) {
    static const QRegularExpression re(
        R"(^\s*[\[\(]\s*pause\s*[:=]?\s*(\d+(?:\.\d+)?)\s*(ms|s|sec|secs|seconds?)?\s*[\]\)]\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(line);
    if (!m.hasMatch())
        return -1;
    const double val = m.captured(1).toDouble();
    const QString unit = m.captured(2).toLower();
    if (unit == "ms")
        return qBound(0, static_cast<int>(qRound(val)), 30000);
    return qBound(0, static_cast<int>(qRound(val * 1000.0)), 30000);
}

QPair<QString, QString> matchSpeakerPrefix(const QString &line) {
    // Match "Speaker Name: spoken text" or "**Speaker Name:** spoken text"
    static const QRegularExpression re(
        R"(^\s*\*{0,2}([A-Za-z][A-Za-z0-9 ._-]{0,24}?)\*{0,2}\s*:\s*\*{0,2}\s*(.+)$)");
    const auto m = re.match(line);
    if (!m.hasMatch())
        return {};
    const QString speaker = m.captured(1).trimmed();
    const QString body = m.captured(2).trimmed();
    const QString lower = speaker.toLower();
    if (lower == "http" || lower == "https" || lower == "ftp" || lower == "note")
        return {};
    if (body.isEmpty())
        return {};
    return {speaker, body};
}

} // namespace

// ---------------- SpeakerModel ----------------

int SpeakerModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_speakers.size();
}

QVariant SpeakerModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_speakers.size())
        return {};
    const auto &s = m_speakers.at(index.row());
    switch (role) {
    case IdRole: return s.id;
    case NameRole: return s.name;
    case VoiceRole: return s.voice;
    case VoiceDisplayRole: return m_voiceDisplays.value(s.voice, cleanDisplayName(s.voice, {}));
    case RateRole: return s.rate;
    case PitchRole: return s.pitch;
    case DefaultPauseMsRole: return s.defaultPauseMs;
    case ColorRole: return s.color;
    default: return {};
    }
}

QHash<int, QByteArray> SpeakerModel::roleNames() const {
    return {
        {IdRole, "speakerId"},
        {NameRole, "name"},
        {VoiceRole, "voice"},
        {VoiceDisplayRole, "voiceDisplay"},
        {RateRole, "rate"},
        {PitchRole, "pitch"},
        {DefaultPauseMsRole, "defaultPauseMs"},
        {ColorRole, "color"}
    };
}

void SpeakerModel::setSpeakers(const QVector<Speaker> &speakers) {
    beginResetModel();
    m_speakers = speakers;
    endResetModel();
}

void SpeakerModel::setVoiceDisplayLookup(const QHash<QString, QString> &lookup) {
    m_voiceDisplays = lookup;
    if (!m_speakers.isEmpty())
        emit dataChanged(index(0), index(m_speakers.size() - 1), {VoiceDisplayRole});
}

void SpeakerModel::notifyUpdated(int row) {
    if (row >= 0 && row < m_speakers.size())
        emit dataChanged(index(row), index(row));
}

// ---------------- ScriptLineModel ----------------

int ScriptLineModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_lines.size();
}

const Speaker *ScriptLineModel::findSpeaker(const QString &id) const {
    if (!m_speakers)
        return nullptr;
    for (const auto &s : *m_speakers) {
        if (s.id == id)
            return &s;
    }
    return m_speakers->isEmpty() ? nullptr : &m_speakers->first();
}

QVariant ScriptLineModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return {};
    const auto &l = m_lines.at(index.row());
    const Speaker *sp = findSpeaker(l.speakerId);
    switch (role) {
    case IdRole: return l.id;
    case SpeakerIdRole: return l.speakerId;
    case SpeakerNameRole: return sp ? sp->name : QStringLiteral("Speaker");
    case SpeakerColorRole: return sp ? sp->color : QStringLiteral("#FFD60A");
    case TextRole: return l.text;
    case PauseMsRole: return l.pauseMs;
    case HasRateOverrideRole: return l.hasRateOverride;
    case RateOverrideRole: return l.rateOverride;
    case HasPitchOverrideRole: return l.hasPitchOverride;
    case PitchOverrideRole: return l.pitchOverride;
    case ClipStateRole: return l.clipState;
    case ClipErrorRole: return l.clipError;
    case DurationMsRole: return l.durationMs;
    default: return {};
    }
}

QHash<int, QByteArray> ScriptLineModel::roleNames() const {
    return {
        {IdRole, "lineId"},
        {SpeakerIdRole, "speakerId"},
        {SpeakerNameRole, "speakerName"},
        {SpeakerColorRole, "speakerColor"},
        {TextRole, "text"},
        {PauseMsRole, "pauseMs"},
        {HasRateOverrideRole, "hasRateOverride"},
        {RateOverrideRole, "rateOverride"},
        {HasPitchOverrideRole, "hasPitchOverride"},
        {PitchOverrideRole, "pitchOverride"},
        {ClipStateRole, "clipState"},
        {ClipErrorRole, "clipError"},
        {DurationMsRole, "durationMs"}
    };
}

void ScriptLineModel::setLines(const QVector<ScriptLine> &lines) {
    beginResetModel();
    m_lines = lines;
    endResetModel();
}

void ScriptLineModel::setSpeakers(const QVector<Speaker> *speakers) {
    m_speakers = speakers;
}

void ScriptLineModel::notifyUpdated(int row) {
    if (row >= 0 && row < m_lines.size())
        emit dataChanged(index(row), index(row));
}

void ScriptLineModel::notifySpeakerChanged() {
    if (!m_lines.isEmpty())
        emit dataChanged(index(0), index(m_lines.size() - 1), {SpeakerNameRole, SpeakerColorRole});
}

// ---------------- VoiceModel ----------------

int VoiceModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_filtered.size();
}

QVariant VoiceModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filtered.size())
        return {};
    const auto &v = m_filtered.at(index.row());
    switch (role) {
    case ShortNameRole: return v.shortName;
    case DisplayNameRole: return v.displayName;
    case FriendlyNameRole: return v.friendlyName;
    case LocaleRole: return v.locale;
    case GenderRole: return v.gender;
    case PersonalitiesRole: return v.personalities.join(", ");
    case CategoriesRole: return v.categories.join(", ");
    case IsFavoriteRole: return v.isFavorite;
    default: return {};
    }
}

QHash<int, QByteArray> VoiceModel::roleNames() const {
    return {
        {ShortNameRole, "shortName"},
        {DisplayNameRole, "displayName"},
        {FriendlyNameRole, "friendlyName"},
        {LocaleRole, "locale"},
        {GenderRole, "gender"},
        {PersonalitiesRole, "personalities"},
        {CategoriesRole, "categories"},
        {IsFavoriteRole, "isFavorite"}
    };
}

void VoiceModel::setAllVoices(const QVector<VoiceEntry> &voices) {
    m_allVoices = voices;
    for (auto &v : m_allVoices)
        v.isFavorite = m_favorites.contains(v.shortName);
    rebuildFiltered();
}

void VoiceModel::setFavorites(const QStringList &favorites) {
    m_favorites = favorites;
    for (auto &v : m_allVoices)
        v.isFavorite = m_favorites.contains(v.shortName);
    rebuildFiltered();
}

void VoiceModel::setSearch(const QString &s) {
    if (m_search == s) return;
    m_search = s;
    rebuildFiltered();
    emit filterChanged();
}

void VoiceModel::setLocaleFilter(const QString &l) {
    if (m_localeFilter == l) return;
    m_localeFilter = l;
    rebuildFiltered();
    emit filterChanged();
}

void VoiceModel::setGenderFilter(const QString &g) {
    if (m_genderFilter == g) return;
    m_genderFilter = g;
    rebuildFiltered();
    emit filterChanged();
}

void VoiceModel::setFavoritesOnly(bool f) {
    if (m_favoritesOnly == f) return;
    m_favoritesOnly = f;
    rebuildFiltered();
    emit filterChanged();
}

void VoiceModel::rebuildFiltered() {
    beginResetModel();
    m_filtered.clear();
    const QString q = m_search.trimmed().toLower();
    const QString loc = m_localeFilter.trimmed().toLower();
    const QString gen = m_genderFilter.trimmed().toLower();

    for (const auto &v : m_allVoices) {
        if (m_favoritesOnly && !v.isFavorite)
            continue;
        if (!loc.isEmpty() && loc != "all") {
            if (!v.locale.toLower().startsWith(loc))
                continue;
        }
        if (!gen.isEmpty() && gen != "all") {
            if (v.gender.toLower() != gen)
                continue;
        }
        if (!q.isEmpty()) {
            const bool match = v.shortName.toLower().contains(q)
                || v.displayName.toLower().contains(q)
                || v.friendlyName.toLower().contains(q)
                || v.locale.toLower().contains(q)
                || v.personalities.join(' ').toLower().contains(q)
                || v.categories.join(' ').toLower().contains(q);
            if (!match)
                continue;
        }
        m_filtered.append(v);
    }

    // Pin favorite voices to the top, then sort by locale and display name.
    std::stable_sort(m_filtered.begin(), m_filtered.end(), [](const VoiceEntry &a, const VoiceEntry &b) {
        if (a.isFavorite != b.isFavorite)
            return a.isFavorite;
        if (a.locale != b.locale) {
            // Put en-US, en-GB, en-AU, en-CA ahead of other locales.
            auto rank = [](const QString &l) {
                if (l == "en-US") return 0;
                if (l == "en-GB") return 1;
                if (l == "en-AU") return 2;
                if (l == "en-CA") return 3;
                if (l.startsWith("en-")) return 4;
                return 5;
            };
            const int ra = rank(a.locale), rb = rank(b.locale);
            if (ra != rb) return ra < rb;
            return a.locale < b.locale;
        }
        return a.displayName < b.displayName;
    });
    endResetModel();
}

// ---------------- Backend ----------------

Backend::Backend(FilePicker *picker, const QString &cacheDir, const QString &pythonOverride, QObject *parent)
    : QObject(parent),
      m_picker(picker),
      m_pythonOverride(pythonOverride) {
    if (!m_picker) {
        m_picker = new PortalFilePicker(this);
        m_ownsPicker = true;
    }

    m_cacheDir = cacheDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        : cacheDir;
    if (m_cacheDir.isEmpty())
        m_cacheDir = QDir::homePath() + "/.cache/recite";
    m_clipsDir = m_cacheDir + "/clips";
    QDir().mkpath(m_clipsDir);

    m_ffmpegBin = QStandardPaths::findExecutable("ffmpeg");
    m_ffprobeBin = QStandardPaths::findExecutable("ffprobe");
    m_ffmpegAvailable = !m_ffmpegBin.isEmpty() && !m_ffprobeBin.isEmpty();

    m_lineModel.setSpeakers(&m_speakerModel.items());

    connect(this, &Backend::selectionChanged, this, &Backend::selectedSpeakerChanged);
    connect(this, &Backend::selectionChanged, this, &Backend::selectedLineChanged);
    connect(&m_speakerModel, &QAbstractItemModel::dataChanged, this, &Backend::selectedSpeakerChanged);
    connect(&m_speakerModel, &QAbstractItemModel::modelReset, this, &Backend::selectedSpeakerChanged);
    connect(&m_lineModel, &QAbstractItemModel::dataChanged, this, &Backend::selectedLineChanged);
    connect(&m_lineModel, &QAbstractItemModel::modelReset, this, &Backend::selectedLineChanged);

    ensureBridgeExtracted();
    loadFavorites();
    loadCachedVoices();
    newProject();

    if (m_pythonOverride.isEmpty())
        m_pythonOverride = qEnvironmentVariable("RECITE_TTS_PYTHON");

    // Defer environment check cleanly unless in tests/CLI that call it synchronously.
    checkEnvironment();
}

Backend::~Backend() {
    cancelWork();
}

void Backend::initAudio() {
    if (m_player)
        return;
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(1.0f);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        const bool nowPlaying = (state == QMediaPlayer::PlayingState);
        if (m_playing != nowPlaying) {
            m_playing = nowPlaying;
            if (!nowPlaying && state == QMediaPlayer::StoppedState) {
                m_playingFullScript = false;
                m_playingLineIndex = -1;
            }
            emit playbackChanged();
        }
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
        m_playbackDurationMs = dur;
        emit playbackChanged();
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        m_playbackPositionMs = pos;
        if (m_playingFullScript) {
            int foundLine = -1;
            for (const auto &seg : m_playbackSegments) {
                if (pos >= seg.startMs && pos < seg.endMs) {
                    foundLine = seg.lineIndex;
                    break;
                }
            }
            if (foundLine != m_playingLineIndex)
                m_playingLineIndex = foundLine;
        }
        emit playbackChanged();
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &err) {
        setBanner(tr("Playback error: %1").arg(err), true);
    });
}

void Backend::ensureBridgeExtracted() {
    m_bridgeScriptPath = m_cacheDir + "/tts_bridge.py";
    QFile res(":/tts_bridge.py");
    if (res.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = res.readAll();
        QSaveFile out(m_bridgeScriptPath);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(bytes);
            out.commit();
        }
    }
}

QString Backend::resolveTtsPython() const {
    if (!m_pythonOverride.isEmpty() && QFileInfo::exists(m_pythonOverride))
        return m_pythonOverride;

    const QStringList candidates = {
        QDir::homePath() + "/.local/share/recite/venv/bin/python",
        QDir::homePath() + "/.local/share/pipx/venvs/edge-tts/bin/python",
        QStandardPaths::findExecutable("python3")
    };
    for (const auto &py : candidates) {
        if (py.isEmpty() || !QFileInfo::exists(py))
            continue;
        QProcess probe;
        probe.start(py, {m_bridgeScriptPath, "check"});
        if (probe.waitForFinished(3000) && probe.exitCode() == 0)
            return py;
    }
    return {};
}

void Backend::checkEnvironment() {
    m_ttsChecking = true;
    emit ttsStatusChanged();

    m_ffmpegBin = QStandardPaths::findExecutable("ffmpeg");
    m_ffprobeBin = QStandardPaths::findExecutable("ffprobe");
    m_ffmpegAvailable = !m_ffmpegBin.isEmpty() && !m_ffprobeBin.isEmpty();

    m_ttsPython = resolveTtsPython();
    m_ttsAvailable = !m_ttsPython.isEmpty();

    if (!m_ffmpegAvailable) {
        m_ttsStatusMessage = tr("ffmpeg / ffprobe not found. Install ffmpeg to combine and export MP3 audio.");
    } else if (!m_ttsAvailable) {
        m_ttsStatusMessage = tr("edge-tts is not installed yet. Click Install Voices Engine to set up Clipchamp neural voices.");
    } else {
        m_ttsStatusMessage = tr("Ready — Microsoft Neural Voices");
    }

    m_ttsChecking = false;
    emit ttsStatusChanged();

    if (m_ttsAvailable && m_voiceModel.totalCount() <= 20)
        refreshVoices();
}

void Backend::installTts() {
    if (m_busy)
        return;

    const QString python3 = QStandardPaths::findExecutable("python3");
    if (python3.isEmpty()) {
        setBanner(tr("python3 was not found on PATH."), true);
        return;
    }

    m_busy = true;
    m_workCompleted = 0;
    m_workTotal = 0;
    m_workMessage = tr("Installing edge-tts voice engine…");
    emit workChanged();

    const QString venvDir = QDir::homePath() + "/.local/share/recite/venv";
    auto *proc = new QProcess(this);
    m_exportProc = proc;
    const QString cmd = QStringLiteral("python3 -m venv '%1' && '%1/bin/pip' install --upgrade edge-tts")
                            .arg(venvDir);
    connect(proc, &QProcess::finished, this, [this, proc](int code, QProcess::ExitStatus status) {
        const QString errText = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        proc->deleteLater();
        m_busy = false;
        m_workMessage.clear();
        emit workChanged();
        if (code == 0 && status == QProcess::NormalExit) {
            checkEnvironment();
            setBanner(tr("edge-tts installed. Fetching voice catalog…"), false);
            refreshVoices();
        } else {
            setBanner(tr("Could not install edge-tts: %1").arg(errText.left(200)), true);
        }
    });
    proc->start("sh", {"-c", cmd});
}

void Backend::loadCachedVoices() {
    QVector<VoiceEntry> entries;
    const QString cacheFile = m_cacheDir + "/voices.json";
    QFile file(cacheFile);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
        for (const auto &val : arr) {
            const QJsonObject o = val.toObject();
            VoiceEntry v;
            v.shortName = o.value("shortName").toString();
            v.friendlyName = o.value("friendlyName").toString();
            v.displayName = cleanDisplayName(v.shortName, v.friendlyName);
            v.locale = o.value("locale").toString();
            v.gender = o.value("gender").toString();
            for (const auto &c : o.value("categories").toArray())
                v.categories.append(c.toString());
            for (const auto &p : o.value("personalities").toArray())
                v.personalities.append(p.toString());
            if (!v.shortName.isEmpty())
                entries.append(v);
        }
    }

    if (entries.isEmpty()) {
        for (const auto &b : kDefaultVoices) {
            VoiceEntry v;
            v.shortName = QString::fromUtf8(b.shortName);
            v.displayName = QString::fromUtf8(b.display);
            v.locale = QString::fromUtf8(b.locale);
            v.gender = QString::fromUtf8(b.gender);
            v.personalities = QString::fromUtf8(b.personalities).split(", ", Qt::SkipEmptyParts);
            entries.append(v);
        }
    }

    m_voiceModel.setAllVoices(entries);
    QHash<QString, QString> lookup;
    for (const auto &v : entries)
        lookup.insert(v.shortName, QStringLiteral("%1 (%2)").arg(v.displayName, v.locale));
    m_speakerModel.setVoiceDisplayLookup(lookup);
    emit voicesChanged();
}

void Backend::saveCachedVoices(const QJsonArray &arr) {
    QSaveFile file(m_cacheDir + "/voices.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void Backend::loadFavorites() {
    QFile file(m_cacheDir + "/favorites.json");
    QStringList favs;
    if (file.open(QIODevice::ReadOnly)) {
        for (const auto &v : QJsonDocument::fromJson(file.readAll()).array())
            favs.append(v.toString());
    } else {
        favs = {"en-US-JennyNeural", "en-US-GuyNeural", "en-US-AriaNeural", "en-GB-SoniaNeural", "en-GB-RyanNeural"};
    }
    m_voiceModel.setFavorites(favs);
}

void Backend::saveFavorites() {
    QSaveFile file(m_cacheDir + "/favorites.json");
    if (file.open(QIODevice::WriteOnly)) {
        QJsonArray arr;
        for (const auto &f : m_voiceModel.favorites())
            arr.append(f);
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void Backend::refreshVoices() {
    if (!m_ttsAvailable || m_voicesLoading)
        return;

    m_voicesLoading = true;
    emit voicesChanged();

    auto *proc = new QProcess(this);
    m_voicesProc = proc;
    connect(proc, &QProcess::finished, this, [this, proc](int code, QProcess::ExitStatus status) {
        const QByteArray out = proc->readAllStandardOutput();
        const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        proc->deleteLater();
        m_voicesLoading = false;

        if (code == 0 && status == QProcess::NormalExit) {
            const QJsonDocument doc = QJsonDocument::fromJson(out);
            if (doc.isArray() && !doc.array().isEmpty()) {
                saveCachedVoices(doc.array());
                loadCachedVoices();
                return;
            }
        }
        if (!err.isEmpty())
            setBanner(tr("Could not fetch voice list: %1").arg(err.left(180)), true);
        emit voicesChanged();
    });
    proc->start(m_ttsPython, {m_bridgeScriptPath, "voices"});
}

bool Backend::fetchVoicesSync(QString *errorOut) {
    if (!m_ttsAvailable) {
        if (errorOut) *errorOut = m_ttsStatusMessage;
        return false;
    }
    QProcess proc;
    proc.start(m_ttsPython, {m_bridgeScriptPath, "voices"});
    if (!proc.waitForFinished(30000) || proc.exitCode() != 0) {
        if (errorOut)
            *errorOut = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
    if (!doc.isArray()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid voice JSON from tts_bridge");
        return false;
    }
    saveCachedVoices(doc.array());
    loadCachedVoices();
    return true;
}

void Backend::toggleFavoriteVoice(const QString &shortName) {
    QStringList favs = m_voiceModel.favorites();
    if (favs.contains(shortName))
        favs.removeAll(shortName);
    else
        favs.append(shortName);
    m_voiceModel.setFavorites(favs);
    saveFavorites();
}

QString Backend::voiceDisplayName(const QString &shortName) const {
    for (const auto &v : m_voiceModel.allVoices()) {
        if (v.shortName == shortName)
            return QStringLiteral("%1 · %2 · %3").arg(v.displayName, v.locale, v.gender);
    }
    return cleanDisplayName(shortName, {});
}

// ---------------- Formatting & Pure Helpers ----------------

QString Backend::formatRate(int ratePercent) {
    const int clamped = qBound(-50, ratePercent, 100);
    return clamped >= 0 ? QStringLiteral("+%1%").arg(clamped)
                        : QStringLiteral("%1%").arg(clamped);
}

QString Backend::formatPitch(int pitchHz) {
    const int clamped = qBound(-50, pitchHz, 50);
    return clamped >= 0 ? QStringLiteral("+%1Hz").arg(clamped)
                        : QStringLiteral("%1Hz").arg(clamped);
}

QString Backend::clipCacheKey(const QString &text, const QString &voice, int rate, int pitch) {
    const QByteArray payload = QStringLiteral("%1\n%2\n%3\n%4")
                                   .arg(voice, formatRate(rate), formatPitch(pitch), text.trimmed())
                                   .toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex().left(24));
}

QString Backend::sanitizeFilePart(const QString &name) {
    QString clean = name.trimmed();
    clean.replace(QRegularExpression("[^A-Za-z0-9_-]+"), "_");
    clean.remove(QRegularExpression("^_+|_+$"));
    return clean.isEmpty() ? QStringLiteral("Speaker") : clean;
}

QString Backend::buildFfmpegFilterScript(const QVector<int> &pauseMsPerClip, bool normalize) {
    if (pauseMsPerClip.isEmpty())
        return {};
    QStringList parts;
    QStringList concatInputs;
    for (int i = 0; i < pauseMsPerClip.size(); ++i) {
        const int pause = qMax(0, pauseMsPerClip.at(i));
        QString chain = QStringLiteral("[%1:a]aresample=44100,aformat=sample_fmts=fltp:channel_layouts=stereo")
                            .arg(i);
        if (pause > 0 && i < pauseMsPerClip.size() - 1) {
            const double sec = pause / 1000.0;
            chain += QStringLiteral(",apad=pad_dur=%1").arg(QString::number(sec, 'f', 3));
        } else if (pause > 0 && pauseMsPerClip.size() == 1) {
            const double sec = pause / 1000.0;
            chain += QStringLiteral(",apad=pad_dur=%1").arg(QString::number(sec, 'f', 3));
        }
        chain += QStringLiteral("[a%1]").arg(i);
        parts.append(chain);
        concatInputs.append(QStringLiteral("[a%1]").arg(i));
    }

    if (normalize) {
        parts.append(QStringLiteral("%1concat=n=%2:v=0:a=1[joined];[joined]loudnorm=I=-14:TP=-1.5:LRA=11[out]")
                         .arg(concatInputs.join(QString()))
                         .arg(pauseMsPerClip.size()));
    } else {
        parts.append(QStringLiteral("%1concat=n=%2:v=0:a=1[out]")
                         .arg(concatInputs.join(QString()))
                         .arg(pauseMsPerClip.size()));
    }
    return parts.join(";\n");
}

QString Backend::formatDuration(qint64 ms) {
    if (ms < 0) ms = 0;
    const qint64 totalSec = ms / 1000;
    const qint64 mins = totalSec / 60;
    const qint64 secs = totalSec % 60;
    return QStringLiteral("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'));
}

// ---------------- Undo / Redo & Snapshot ----------------

ProjectSnapshot Backend::captureSnapshot() const {
    return {m_title, m_defaultPauseMs, m_speakerModel.items(), m_lineModel.items()};
}

void Backend::restoreSnapshot(const ProjectSnapshot &snap) {
    m_title = snap.title;
    m_defaultPauseMs = snap.defaultPauseMs;
    m_speakerModel.setSpeakers(snap.speakers);
    m_lineModel.setSpeakers(&m_speakerModel.items());
    m_lineModel.setLines(snap.lines);
    if (m_selectedSpeakerIndex >= m_speakerModel.items().size())
        m_selectedSpeakerIndex = qMax(0, m_speakerModel.items().size() - 1);
    if (m_selectedLineIndex >= m_lineModel.items().size())
        m_selectedLineIndex = qMax(0, m_lineModel.items().size() - 1);
    updateClipStatesFromCache();
    emit projectChanged();
    emit selectionChanged();
    emit linesChanged();
}

void Backend::pushUndoSnapshot() {
    m_undoStack.append(captureSnapshot());
    if (m_undoStack.size() > 50)
        m_undoStack.removeFirst();
    m_redoStack.clear();
    emit historyChanged();
}

void Backend::undo() {
    if (m_undoStack.isEmpty())
        return;
    m_redoStack.append(captureSnapshot());
    const auto snap = m_undoStack.takeLast();
    restoreSnapshot(snap);
    markDirty();
    emit historyChanged();
}

void Backend::redo() {
    if (m_redoStack.isEmpty())
        return;
    m_undoStack.append(captureSnapshot());
    const auto snap = m_redoStack.takeLast();
    restoreSnapshot(snap);
    markDirty();
    emit historyChanged();
}

void Backend::saveUndoPoint() {
    pushUndoSnapshot();
}

QVariantMap Backend::selectedSpeaker() const {
    const auto &speakers = m_speakerModel.items();
    if (m_selectedSpeakerIndex < 0 || m_selectedSpeakerIndex >= speakers.size())
        return {};
    const auto &s = speakers.at(m_selectedSpeakerIndex);
    return {
        {"id", s.id},
        {"name", s.name},
        {"voice", s.voice},
        {"voiceDisplay", voiceDisplayName(s.voice)},
        {"rate", s.rate},
        {"pitch", s.pitch},
        {"defaultPauseMs", s.defaultPauseMs},
        {"color", s.color}
    };
}

QVariantMap Backend::selectedLine() const {
    const auto &lines = m_lineModel.items();
    if (m_selectedLineIndex < 0 || m_selectedLineIndex >= lines.size())
        return {};
    const auto &l = lines.at(m_selectedLineIndex);
    const Speaker *sp = speakerById(l.speakerId);
    return {
        {"id", l.id},
        {"speakerId", l.speakerId},
        {"speakerName", sp ? sp->name : QString()},
        {"text", l.text},
        {"pauseMs", l.pauseMs},
        {"hasRateOverride", l.hasRateOverride},
        {"rateOverride", l.rateOverride},
        {"hasPitchOverride", l.hasPitchOverride},
        {"pitchOverride", l.pitchOverride},
        {"speakerRate", sp ? sp->rate : 0},
        {"speakerPitch", sp ? sp->pitch : 0}
    };
}

void Backend::markDirty() {
    if (!m_dirty) {
        m_dirty = true;
        emit projectChanged();
    }
}

void Backend::setBanner(const QString &msg, bool isError) {
    m_statusBanner = msg;
    m_statusBannerIsError = isError;
    emit bannerChanged();
}

void Backend::dismissBanner() {
    m_statusBanner.clear();
    m_statusBannerIsError = false;
    emit bannerChanged();
}

QString Backend::newId(const QString &prefix) {
    return QStringLiteral("%1%2").arg(prefix).arg(++m_idCounter);
}

QString Backend::nextSpeakerColor(int index) const {
    return kSpeakerPalette.at(index % kSpeakerPalette.size());
}

QString Backend::pickVoiceForSpeakerName(const QString &name, int existingCount) const {
    const QString lower = name.toLower();
    static const QRegularExpression maleHints(
        R"(\b(man|male|boy|mr|sir|father|dad|brother|uncle|king|guy|adam|john|david|mark|alex|kevin|sam)\b)");
    static const QRegularExpression femaleHints(
        R"(\b(woman|female|girl|ms|mrs|miss|madam|mother|mom|sister|aunt|queen|sarah|emily|anna|lisa|mary|kate|emma|jenny)\b)");
    if (maleHints.match(lower).hasMatch())
        return kMaleVoices.at(existingCount % kMaleVoices.size());
    if (femaleHints.match(lower).hasMatch())
        return kFemaleVoices.at(existingCount % kFemaleVoices.size());
    if (existingCount % 2 == 0)
        return kFemaleVoices.at((existingCount / 2) % kFemaleVoices.size());
    return kMaleVoices.at((existingCount / 2) % kMaleVoices.size());
}

const Speaker *Backend::speakerById(const QString &id) const {
    for (const auto &s : m_speakerModel.items()) {
        if (s.id == id)
            return &s;
    }
    return m_speakerModel.items().isEmpty() ? nullptr : &m_speakerModel.items().first();
}

Speaker *Backend::mutableSpeakerById(const QString &id) {
    for (auto &s : m_speakerModel.items()) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

QString Backend::clipPathForLine(const ScriptLine &line) const {
    const Speaker *sp = speakerById(line.speakerId);
    const QString voice = sp ? sp->voice : QStringLiteral("en-US-JennyNeural");
    const int rate = line.hasRateOverride ? line.rateOverride : (sp ? sp->rate : 0);
    const int pitch = line.hasPitchOverride ? line.pitchOverride : (sp ? sp->pitch : 0);
    const QString key = clipCacheKey(line.text, voice, rate, pitch);
    return m_clipsDir + "/" + key + ".mp3";
}

void Backend::updateClipStatesFromCache() {
    auto &lines = m_lineModel.items();
    for (int i = 0; i < lines.size(); ++i) {
        auto &line = lines[i];
        if (line.text.trimmed().isEmpty()) {
            line.clipState = "idle";
            continue;
        }
        const QString path = clipPathForLine(line);
        const QFileInfo fi(path);
        if (fi.exists() && fi.size() > 0) {
            line.clipState = "ready";
            line.clipError.clear();
        } else if (line.clipState != "synthesizing") {
            line.clipState = "idle";
        }
        m_lineModel.notifyUpdated(i);
    }
}

int Backend::wordCount() const {
    int total = 0;
    static const QRegularExpression ws("\\s+");
    for (const auto &l : m_lineModel.items()) {
        const QString trimmed = l.text.trimmed();
        if (!trimmed.isEmpty())
            total += trimmed.split(ws, Qt::SkipEmptyParts).size();
    }
    return total;
}

// ---------------- Project Lifecycle ----------------

void Backend::newProject() {
    stopPlayback();
    m_title = tr("Untitled Script");
    m_projectPath.clear();
    m_dirty = false;
    m_defaultPauseMs = 600;
    m_undoStack.clear();
    m_redoStack.clear();

    Speaker narrator;
    narrator.id = newId("s");
    narrator.name = tr("Narrator");
    narrator.voice = "en-US-JennyNeural";
    narrator.color = nextSpeakerColor(0);

    ScriptLine line1;
    line1.id = newId("l");
    line1.speakerId = narrator.id;
    line1.text = {};
    line1.pauseMs = m_defaultPauseMs;

    m_speakerModel.setSpeakers({narrator});
    m_lineModel.setSpeakers(&m_speakerModel.items());
    m_lineModel.setLines({line1});
    m_selectedSpeakerIndex = 0;
    m_selectedLineIndex = 0;

    emit projectChanged();
    emit selectionChanged();
    emit linesChanged();
    emit historyChanged();
}

void Backend::setProjectTitle(const QString &title) {
    if (m_title == title) return;
    pushUndoSnapshot();
    m_title = title;
    markDirty();
    emit projectChanged();
}

void Backend::setDefaultPauseMs(int ms) {
    const int clamped = qBound(0, ms, 30000);
    if (m_defaultPauseMs == clamped) return;
    pushUndoSnapshot();
    m_defaultPauseMs = clamped;
    markDirty();
    emit projectChanged();
}

void Backend::setSelectedSpeakerIndex(int idx) {
    if (idx < 0 || idx >= m_speakerModel.items().size()) return;
    if (m_selectedSpeakerIndex == idx) return;
    m_selectedSpeakerIndex = idx;
    emit selectionChanged();
}

void Backend::setSelectedLineIndex(int idx) {
    if (idx < 0 || idx >= m_lineModel.items().size()) return;
    if (m_selectedLineIndex == idx) return;
    m_selectedLineIndex = idx;
    emit selectionChanged();
}

bool Backend::loadFromFile(const QString &path, QString *errorOut) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        const QString err = tr("Cannot open %1: %2").arg(path, file.errorString());
        if (errorOut) *errorOut = err;
        setBanner(err, true);
        return false;
    }

    const QByteArray raw = file.readAll();
    // Plain text scripts (.txt) can be opened straight into the parser!
    if (path.endsWith(".txt", Qt::CaseInsensitive) || path.endsWith(".md", Qt::CaseInsensitive)) {
        newProject();
        m_title = QFileInfo(path).completeBaseName();
        importScriptText(QString::fromUtf8(raw), true, {}, true);
        m_dirty = false;
        emit projectChanged();
        return true;
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString err = tr("Invalid project file: %1").arg(parseErr.errorString());
        if (errorOut) *errorOut = err;
        setBanner(err, true);
        return false;
    }

    const QJsonObject root = doc.object();
    QVector<Speaker> speakers;
    for (const auto &v : root.value("speakers").toArray()) {
        const QJsonObject o = v.toObject();
        Speaker s;
        s.id = o.value("id").toString(newId("s"));
        s.name = o.value("name").toString("Speaker");
        s.voice = o.value("voice").toString("en-US-JennyNeural");
        s.rate = qBound(-50, o.value("rate").toInt(0), 100);
        s.pitch = qBound(-50, o.value("pitch").toInt(0), 50);
        s.defaultPauseMs = qBound(0, o.value("defaultPauseMs").toInt(600), 30000);
        s.color = o.value("color").toString(nextSpeakerColor(speakers.size()));
        speakers.append(s);
    }
    if (speakers.isEmpty()) {
        Speaker s;
        s.id = newId("s");
        s.name = tr("Narrator");
        s.color = nextSpeakerColor(0);
        speakers.append(s);
    }

    QVector<ScriptLine> lines;
    for (const auto &v : root.value("lines").toArray()) {
        const QJsonObject o = v.toObject();
        ScriptLine l;
        l.id = o.value("id").toString(newId("l"));
        l.speakerId = o.value("speakerId").toString(speakers.first().id);
        l.text = o.value("text").toString();
        l.pauseMs = qBound(0, o.value("pauseMs").toInt(600), 30000);
        l.hasRateOverride = o.value("hasRateOverride").toBool(false);
        l.rateOverride = qBound(-50, o.value("rateOverride").toInt(0), 100);
        l.hasPitchOverride = o.value("hasPitchOverride").toBool(false);
        l.pitchOverride = qBound(-50, o.value("pitchOverride").toInt(0), 50);
        lines.append(l);
    }

    stopPlayback();
    m_undoStack.clear();
    m_redoStack.clear();
    m_title = root.value("title").toString(QFileInfo(path).completeBaseName());
    m_defaultPauseMs = qBound(0, root.value("defaultPauseMs").toInt(600), 30000);
    m_projectPath = path;
    m_dirty = false;
    m_speakerModel.setSpeakers(speakers);
    m_lineModel.setSpeakers(&m_speakerModel.items());
    m_lineModel.setLines(lines);
    m_selectedSpeakerIndex = 0;
    m_selectedLineIndex = 0;
    updateClipStatesFromCache();

    emit projectChanged();
    emit selectionChanged();
    emit linesChanged();
    emit historyChanged();
    return true;
}

bool Backend::saveToFile(const QString &path, QString *errorOut) {
    QJsonObject root;
    root.insert("version", 1);
    root.insert("title", m_title);
    root.insert("defaultPauseMs", m_defaultPauseMs);

    QJsonArray spArr;
    for (const auto &s : m_speakerModel.items()) {
        QJsonObject o;
        o.insert("id", s.id);
        o.insert("name", s.name);
        o.insert("voice", s.voice);
        o.insert("rate", s.rate);
        o.insert("pitch", s.pitch);
        o.insert("defaultPauseMs", s.defaultPauseMs);
        o.insert("color", s.color);
        spArr.append(o);
    }
    root.insert("speakers", spArr);

    QJsonArray lineArr;
    for (const auto &l : m_lineModel.items()) {
        QJsonObject o;
        o.insert("id", l.id);
        o.insert("speakerId", l.speakerId);
        o.insert("text", l.text);
        o.insert("pauseMs", l.pauseMs);
        o.insert("hasRateOverride", l.hasRateOverride);
        o.insert("rateOverride", l.rateOverride);
        o.insert("hasPitchOverride", l.hasPitchOverride);
        o.insert("pitchOverride", l.pitchOverride);
        lineArr.append(o);
    }
    root.insert("lines", lineArr);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        const QString err = tr("Cannot save %1: %2").arg(path, file.errorString());
        if (errorOut) *errorOut = err;
        setBanner(err, true);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        const QString err = tr("Failed to write %1: %2").arg(path, file.errorString());
        if (errorOut) *errorOut = err;
        setBanner(err, true);
        return false;
    }

    m_projectPath = path;
    if (m_title == tr("Untitled Script") || m_title.isEmpty())
        m_title = QFileInfo(path).completeBaseName();
    m_dirty = false;
    emit projectChanged();
    setBanner(tr("Saved %1").arg(QFileInfo(path).fileName()), false);
    return true;
}

void Backend::openProjectDialog() {
    const QList<FileFilter> filters = {
        {tr("Recite & Script Files"), {"*.recite", "*.txt"}},
        {tr("Recite Projects (*.recite)"), {"*.recite"}},
        {tr("Text Scripts (*.txt)"), {"*.txt"}}
    };
    m_picker->openFile(tr("Open Script Project"), filters, [this](const QString &path) {
        if (!path.isEmpty())
            loadFromFile(path);
    });
}

void Backend::saveProjectDialog() {
    const QString suggested = sanitizeFilePart(m_title) + ".recite";
    const QList<FileFilter> filters = {
        {tr("Recite Projects (*.recite)"), {"*.recite"}}
    };
    m_picker->saveFile(tr("Save Script Project"), suggested, filters, [this](const QString &path) {
        if (path.isEmpty()) return;
        QString out = path;
        if (!out.endsWith(".recite", Qt::CaseInsensitive))
            out += ".recite";
        saveToFile(out);
    });
}

void Backend::saveProject() {
    if (m_projectPath.isEmpty()) {
        saveProjectDialog();
        return;
    }
    saveToFile(m_projectPath);
}

// ---------------- Script Editing ----------------

void Backend::addLine(int afterIndex, const QString &speakerId, const QString &text) {
    pushUndoSnapshot();
    auto lines = m_lineModel.items();
    QString spId = speakerId;
    if (spId.isEmpty()) {
        if (afterIndex >= 0 && afterIndex < lines.size())
            spId = lines.at(afterIndex).speakerId;
        else if (m_selectedSpeakerIndex >= 0 && m_selectedSpeakerIndex < m_speakerModel.items().size())
            spId = m_speakerModel.items().at(m_selectedSpeakerIndex).id;
        else if (!m_speakerModel.items().isEmpty())
            spId = m_speakerModel.items().first().id;
    }
    const Speaker *sp = speakerById(spId);

    ScriptLine line;
    line.id = newId("l");
    line.speakerId = spId;
    line.text = text;
    line.pauseMs = sp ? sp->defaultPauseMs : m_defaultPauseMs;

    const int insertPos = (afterIndex >= 0 && afterIndex < lines.size()) ? afterIndex + 1 : lines.size();
    lines.insert(insertPos, line);
    m_lineModel.setLines(lines);
    m_selectedLineIndex = insertPos;
    updateClipStatesFromCache();
    markDirty();
    emit linesChanged();
    emit selectionChanged();
}

void Backend::duplicateLine(int index) {
    auto lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    pushUndoSnapshot();
    ScriptLine copy = lines.at(index);
    copy.id = newId("l");
    lines.insert(index + 1, copy);
    m_lineModel.setLines(lines);
    m_selectedLineIndex = index + 1;
    markDirty();
    emit linesChanged();
    emit selectionChanged();
}

void Backend::removeLine(int index) {
    auto lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    pushUndoSnapshot();
    lines.removeAt(index);
    if (lines.isEmpty()) {
        ScriptLine empty;
        empty.id = newId("l");
        empty.speakerId = m_speakerModel.items().isEmpty() ? QString() : m_speakerModel.items().first().id;
        empty.pauseMs = m_defaultPauseMs;
        lines.append(empty);
    }
    m_lineModel.setLines(lines);
    m_selectedLineIndex = qBound(0, index, lines.size() - 1);
    markDirty();
    emit linesChanged();
    emit selectionChanged();
}

void Backend::moveLine(int fromIndex, int toIndex) {
    auto lines = m_lineModel.items();
    if (fromIndex < 0 || fromIndex >= lines.size() || toIndex < 0 || toIndex >= lines.size() || fromIndex == toIndex)
        return;
    pushUndoSnapshot();
    lines.move(fromIndex, toIndex);
    m_lineModel.setLines(lines);
    m_selectedLineIndex = toIndex;
    markDirty();
    emit linesChanged();
    emit selectionChanged();
}

void Backend::setLineText(int index, const QString &text) {
    auto &lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    if (lines[index].text == text) return;
    lines[index].text = text;
    // Update cache status without resetting the model so the TextArea keeps focus.
    const QString path = clipPathForLine(lines[index]);
    const QFileInfo fi(path);
    lines[index].clipState = (!text.trimmed().isEmpty() && fi.exists() && fi.size() > 0) ? "ready" : "idle";
    lines[index].clipError.clear();
    m_lineModel.notifyUpdated(index);
    markDirty();
    emit linesChanged();
}

void Backend::commitLineText(int index, const QString &text) {
    auto &lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    if (lines[index].text == text) return;
    pushUndoSnapshot();
    setLineText(index, text);
}

void Backend::setLineSpeaker(int index, const QString &speakerId) {
    auto &lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    if (lines[index].speakerId == speakerId) return;
    pushUndoSnapshot();
    lines[index].speakerId = speakerId;
    m_lineModel.notifyUpdated(index);
    updateClipStatesFromCache();
    markDirty();
}

void Backend::cycleLineSpeaker(int index) {
    const auto &speakers = m_speakerModel.items();
    auto &lines = m_lineModel.items();
    if (index < 0 || index >= lines.size() || speakers.size() <= 1) return;
    int cur = 0;
    for (int i = 0; i < speakers.size(); ++i) {
        if (speakers.at(i).id == lines.at(index).speakerId) {
            cur = i;
            break;
        }
    }
    const int next = (cur + 1) % speakers.size();
    setLineSpeaker(index, speakers.at(next).id);
}

void Backend::setLinePause(int index, int pauseMs) {
    auto &lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    const int clamped = qBound(0, pauseMs, 30000);
    if (lines[index].pauseMs == clamped) return;
    pushUndoSnapshot();
    lines[index].pauseMs = clamped;
    m_lineModel.notifyUpdated(index);
    markDirty();
}

void Backend::setLineRateOverride(int index, bool enabled, int rate) {
    auto &lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    pushUndoSnapshot();
    lines[index].hasRateOverride = enabled;
    lines[index].rateOverride = qBound(-50, rate, 100);
    m_lineModel.notifyUpdated(index);
    updateClipStatesFromCache();
    markDirty();
}

void Backend::setLinePitchOverride(int index, bool enabled, int pitch) {
    auto &lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    pushUndoSnapshot();
    lines[index].hasPitchOverride = enabled;
    lines[index].pitchOverride = qBound(-50, pitch, 50);
    m_lineModel.notifyUpdated(index);
    updateClipStatesFromCache();
    markDirty();
}

void Backend::applyDefaultPauseToAll() {
    pushUndoSnapshot();
    auto &lines = m_lineModel.items();
    for (int i = 0; i < lines.size(); ++i) {
        lines[i].pauseMs = m_defaultPauseMs;
        m_lineModel.notifyUpdated(i);
    }
    markDirty();
}

// ---------------- Speaker Editing ----------------

void Backend::addSpeaker(const QString &name) {
    pushUndoSnapshot();
    auto speakers = m_speakerModel.items();
    Speaker sp;
    sp.id = newId("s");
    sp.name = name.trimmed().isEmpty()
        ? tr("Speaker %1").arg(speakers.size() + 1)
        : name.trimmed();
    sp.voice = pickVoiceForSpeakerName(sp.name, speakers.size());
    sp.defaultPauseMs = m_defaultPauseMs;
    sp.color = nextSpeakerColor(speakers.size());
    speakers.append(sp);
    m_speakerModel.setSpeakers(speakers);
    m_lineModel.setSpeakers(&m_speakerModel.items());
    m_selectedSpeakerIndex = speakers.size() - 1;
    m_lineModel.notifySpeakerChanged();
    markDirty();
    emit selectionChanged();
}

void Backend::removeSpeaker(int index) {
    auto speakers = m_speakerModel.items();
    if (speakers.size() <= 1 || index < 0 || index >= speakers.size())
        return;
    pushUndoSnapshot();
    const QString removedId = speakers.at(index).id;
    speakers.removeAt(index);
    const QString fallbackId = speakers.first().id;
    m_speakerModel.setSpeakers(speakers);
    m_lineModel.setSpeakers(&m_speakerModel.items());

    auto &lines = m_lineModel.items();
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].speakerId == removedId)
            lines[i].speakerId = fallbackId;
    }
    m_lineModel.notifySpeakerChanged();
    updateClipStatesFromCache();
    m_selectedSpeakerIndex = qBound(0, index, speakers.size() - 1);
    markDirty();
    emit selectionChanged();
}

void Backend::setSpeakerName(int index, const QString &name) {
    auto &speakers = m_speakerModel.items();
    if (index < 0 || index >= speakers.size()) return;
    const QString clean = name.trimmed();
    if (clean.isEmpty() || speakers[index].name == clean) return;
    pushUndoSnapshot();
    speakers[index].name = clean;
    m_speakerModel.notifyUpdated(index);
    m_lineModel.notifySpeakerChanged();
    markDirty();
}

void Backend::setSpeakerVoice(int index, const QString &shortName) {
    auto &speakers = m_speakerModel.items();
    if (index < 0 || index >= speakers.size() || shortName.isEmpty()) return;
    if (speakers[index].voice == shortName) return;
    pushUndoSnapshot();
    speakers[index].voice = shortName;
    m_speakerModel.notifyUpdated(index);
    updateClipStatesFromCache();
    markDirty();
}

void Backend::setSpeakerRate(int index, int rate) {
    auto &speakers = m_speakerModel.items();
    if (index < 0 || index >= speakers.size()) return;
    const int clamped = qBound(-50, rate, 100);
    if (speakers[index].rate == clamped) return;
    speakers[index].rate = clamped;
    m_speakerModel.notifyUpdated(index);
    updateClipStatesFromCache();
    markDirty();
}

void Backend::setSpeakerPitch(int index, int pitch) {
    auto &speakers = m_speakerModel.items();
    if (index < 0 || index >= speakers.size()) return;
    const int clamped = qBound(-50, pitch, 50);
    if (speakers[index].pitch == clamped) return;
    speakers[index].pitch = clamped;
    m_speakerModel.notifyUpdated(index);
    updateClipStatesFromCache();
    markDirty();
}

void Backend::setSpeakerDefaultPause(int index, int pauseMs) {
    auto &speakers = m_speakerModel.items();
    if (index < 0 || index >= speakers.size()) return;
    const int clamped = qBound(0, pauseMs, 30000);
    if (speakers[index].defaultPauseMs == clamped) return;
    speakers[index].defaultPauseMs = clamped;
    m_speakerModel.notifyUpdated(index);
    markDirty();
}

// ---------------- Script Parser / Import ----------------

QVariantMap Backend::parseScriptPreview(const QString &rawText, bool detectSpeakers,
                                        const QString &fallbackSpeakerId) const {
    const QStringList rawLines = rawText.split(QRegularExpression("\r?\n"));

    struct ParsedLine {
        QString speakerName;
        QString text;
        int pauseMs;
    };
    QVector<ParsedLine> parsed;
    QStringList detectedNames;

    QString fallbackName = m_speakerModel.items().isEmpty()
        ? QStringLiteral("Narrator")
        : m_speakerModel.items().first().name;
    if (!fallbackSpeakerId.isEmpty()) {
        if (const Speaker *sp = speakerById(fallbackSpeakerId))
            fallbackName = sp->name;
    }

    QString currentSpeaker = fallbackName;
    QString currentPara;

    auto flushPara = [&]() {
        const QString cleaned = currentPara.trimmed();
        if (!cleaned.isEmpty()) {
            parsed.append({currentSpeaker, cleaned, m_defaultPauseMs});
            if (!detectedNames.contains(currentSpeaker, Qt::CaseInsensitive))
                detectedNames.append(currentSpeaker);
        }
        currentPara.clear();
    };

    for (const QString &rawLine : rawLines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) {
            flushPara();
            continue;
        }

        const int pauseTag = parsePauseTagMs(trimmed);
        if (pauseTag >= 0) {
            flushPara();
            if (!parsed.isEmpty())
                parsed.last().pauseMs = pauseTag;
            continue;
        }

        if (detectSpeakers) {
            const auto spMatch = matchSpeakerPrefix(trimmed);
            if (!spMatch.first.isEmpty()) {
                flushPara();
                currentSpeaker = spMatch.first;
                currentPara = spMatch.second;
                continue;
            }
        }

        if (!currentPara.isEmpty())
            currentPara += ' ';
        currentPara += trimmed;
    }
    flushPara();

    QVariantList lineList;
    for (const auto &p : parsed) {
        QVariantMap m;
        m.insert("speaker", p.speakerName);
        m.insert("text", p.text);
        m.insert("pauseMs", p.pauseMs);
        lineList.append(m);
    }

    QVariantMap result;
    result.insert("lines", lineList);
    result.insert("speakers", detectedNames);
    result.insert("lineCount", parsed.size());
    result.insert("speakerCount", detectedNames.size());
    return result;
}

void Backend::importScriptText(const QString &rawText, bool detectSpeakers,
                               const QString &fallbackSpeakerId, bool replaceExisting) {
    const QVariantMap preview = parseScriptPreview(rawText, detectSpeakers, fallbackSpeakerId);
    const QVariantList parsedLines = preview.value("lines").toList();
    if (parsedLines.isEmpty())
        return;

    pushUndoSnapshot();

    QVector<Speaker> speakers = m_speakerModel.items();
    auto findOrAddSpeaker = [&](const QString &name) -> QString {
        for (const auto &s : speakers) {
            if (s.name.compare(name, Qt::CaseInsensitive) == 0)
                return s.id;
        }
        Speaker sp;
        sp.id = newId("s");
        sp.name = name;
        sp.voice = pickVoiceForSpeakerName(name, speakers.size());
        sp.defaultPauseMs = m_defaultPauseMs;
        sp.color = nextSpeakerColor(speakers.size());
        speakers.append(sp);
        return sp.id;
    };

    QVector<ScriptLine> lines = replaceExisting ? QVector<ScriptLine>() : m_lineModel.items();
    // Drop a single blank starter line when appending to an untouched project.
    if (!replaceExisting && lines.size() == 1 && lines.first().text.trimmed().isEmpty())
        lines.clear();

    for (const auto &v : parsedLines) {
        const QVariantMap m = v.toMap();
        ScriptLine line;
        line.id = newId("l");
        line.speakerId = findOrAddSpeaker(m.value("speaker").toString());
        line.text = m.value("text").toString();
        line.pauseMs = m.value("pauseMs", m_defaultPauseMs).toInt();
        lines.append(line);
    }

    m_speakerModel.setSpeakers(speakers);
    m_lineModel.setSpeakers(&m_speakerModel.items());
    m_lineModel.setLines(lines);
    m_selectedSpeakerIndex = 0;
    m_selectedLineIndex = 0;
    updateClipStatesFromCache();
    markDirty();
    emit linesChanged();
    emit selectionChanged();
}

// ---------------- Playback & Synthesis ----------------

void Backend::primaryAction() {
    if (m_playing) {
        stopPlayback();
        return;
    }
    playAll();
}

void Backend::stopPlayback() {
    if (m_player)
        m_player->stop();
    m_playing = false;
    m_playingFullScript = false;
    m_playingLineIndex = -1;
    m_playbackPositionMs = 0;
    emit playbackChanged();
}

void Backend::seekPlayback(qint64 positionMs) {
    if (m_player)
        m_player->setPosition(positionMs);
}

void Backend::sampleVoice(const QString &shortName, int rate, int pitch) {
    if (!m_ttsAvailable) {
        setBanner(m_ttsStatusMessage, true);
        return;
    }
    const QString sampleText = tr("Hello! This is a preview of how I sound when reading your script.");
    const QString key = clipCacheKey(sampleText, shortName, rate, pitch);
    const QString outPath = m_clipsDir + "/" + key + ".mp3";

    if (QFileInfo::exists(outPath) && QFileInfo(outPath).size() > 0) {
        initAudio();
        stopPlayback();
        m_playingLineIndex = -1;
        m_playingFullScript = false;
        m_player->setSource(QUrl::fromLocalFile(outPath));
        m_player->play();
        return;
    }

    auto *tmpText = new QTemporaryFile(m_cacheDir + "/sample_XXXXXX.txt", this);
    if (!tmpText->open()) {
        tmpText->deleteLater();
        return;
    }
    tmpText->write(sampleText.toUtf8());
    tmpText->close();

    m_busy = true;
    m_workCompleted = 0;
    m_workTotal = 1;
    m_workMessage = tr("Generating voice sample…");
    emit workChanged();

    auto *proc = new QProcess(this);
    m_activeSynthProcs.append(proc);
    connect(proc, &QProcess::finished, this, [this, proc, tmpText, outPath](int code, QProcess::ExitStatus status) {
        m_activeSynthProcs.removeAll(proc);
        const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        proc->deleteLater();
        tmpText->deleteLater();
        m_busy = false;
        m_workMessage.clear();
        emit workChanged();

        if (code == 0 && status == QProcess::NormalExit && QFileInfo::exists(outPath)) {
            initAudio();
            stopPlayback();
            m_playingLineIndex = -1;
            m_playingFullScript = false;
            m_player->setSource(QUrl::fromLocalFile(outPath));
            m_player->play();
        } else if (!err.isEmpty()) {
            setBanner(tr("Voice sample failed: %1").arg(err.left(180)), true);
        }
    });
    proc->start(m_ttsPython, {
        m_bridgeScriptPath, "synthesize",
        "--text-file", tmpText->fileName(),
        "--voice", shortName,
        "--rate", formatRate(rate),
        "--pitch", formatPitch(pitch),
        "--out", outPath
    });
}

void Backend::playLine(int index) {
    const auto &lines = m_lineModel.items();
    if (index < 0 || index >= lines.size()) return;
    const auto &line = lines.at(index);
    if (line.text.trimmed().isEmpty()) {
        setBanner(tr("Line %1 has no text to speak.").arg(index + 1), true);
        return;
    }
    if (!m_ttsAvailable) {
        setBanner(m_ttsStatusMessage, true);
        return;
    }

    const QString clipPath = clipPathForLine(line);
    if (QFileInfo::exists(clipPath) && QFileInfo(clipPath).size() > 0) {
        initAudio();
        stopPlayback();
        m_playingLineIndex = index;
        m_playingFullScript = false;
        m_player->setSource(QUrl::fromLocalFile(clipPath));
        m_player->play();
        emit playbackChanged();
        return;
    }

    m_pendingSinglePlayKey = clipPath;
    m_pendingSinglePlayLine = index;
    startBatchSynthesis({index}, PendingAction::None);
}

void Backend::playAll() {
    if (!m_ttsAvailable) {
        setBanner(m_ttsStatusMessage, true);
        return;
    }
    if (!m_ffmpegAvailable) {
        setBanner(tr("ffmpeg is required to stitch full-script preview."), true);
        return;
    }

    QVector<int> nonEmpty;
    const auto &lines = m_lineModel.items();
    for (int i = 0; i < lines.size(); ++i) {
        if (!lines.at(i).text.trimmed().isEmpty())
            nonEmpty.append(i);
    }
    if (nonEmpty.isEmpty()) {
        setBanner(tr("Add some script text before playing."), true);
        return;
    }

    m_pendingSinglePlayLine = -1;
    m_pendingSinglePlayKey.clear();
    startBatchSynthesis(nonEmpty, PendingAction::PlayAll);
}

void Backend::startBatchSynthesis(const QVector<int> &lineIndices, PendingAction nextAction) {
    if (m_busy)
        return;

    m_synthQueue.clear();
    m_pendingAction = nextAction;
    m_workCompleted = 0;
    m_workTotal = 0;

    auto &lines = m_lineModel.items();
    for (int idx : lineIndices) {
        if (idx < 0 || idx >= lines.size()) continue;
        const auto &line = lines.at(idx);
        if (line.text.trimmed().isEmpty()) continue;

        const QString outPath = clipPathForLine(line);
        if (QFileInfo::exists(outPath) && QFileInfo(outPath).size() > 0) {
            lines[idx].clipState = "ready";
            m_lineModel.notifyUpdated(idx);
            continue;
        }

        const Speaker *sp = speakerById(line.speakerId);
        const QString voice = sp ? sp->voice : QStringLiteral("en-US-JennyNeural");
        const int rate = line.hasRateOverride ? line.rateOverride : (sp ? sp->rate : 0);
        const int pitch = line.hasPitchOverride ? line.pitchOverride : (sp ? sp->pitch : 0);

        SynthTask task;
        task.lineIndex = idx;
        task.text = line.text.trimmed();
        task.voice = voice;
        task.rate = formatRate(rate);
        task.pitch = formatPitch(pitch);
        task.outPath = outPath;
        m_synthQueue.append(task);

        lines[idx].clipState = "synthesizing";
        lines[idx].clipError.clear();
        m_lineModel.notifyUpdated(idx);
    }

    m_workTotal = m_synthQueue.size();
    if (m_synthQueue.isEmpty()) {
        finishBatchSynthesis();
        return;
    }

    m_busy = true;
    m_workMessage = tr("Generating audio (%1/%2)…").arg(0).arg(m_workTotal);
    emit workChanged();
    pumpSynthesisQueue();
}

void Backend::pumpSynthesisQueue() {
    while (m_activeSynthProcs.size() < 3 && !m_synthQueue.isEmpty()) {
        const SynthTask task = m_synthQueue.takeFirst();
        auto *tmpText = new QTemporaryFile(m_cacheDir + "/line_XXXXXX.txt", this);
        if (!tmpText->open()) {
            tmpText->deleteLater();
            continue;
        }
        tmpText->write(task.text.toUtf8());
        tmpText->close();

        auto *proc = new QProcess(this);
        m_activeSynthProcs.append(proc);
        connect(proc, &QProcess::finished, this,
                [this, proc, tmpText, task](int code, QProcess::ExitStatus status) {
            m_activeSynthProcs.removeAll(proc);
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            proc->deleteLater();
            tmpText->deleteLater();

            auto &lines = m_lineModel.items();
            if (task.lineIndex >= 0 && task.lineIndex < lines.size()) {
                if (code == 0 && status == QProcess::NormalExit && QFileInfo::exists(task.outPath)) {
                    lines[task.lineIndex].clipState = "ready";
                    lines[task.lineIndex].clipError.clear();
                } else {
                    lines[task.lineIndex].clipState = "error";
                    lines[task.lineIndex].clipError = err.isEmpty() ? tr("Synthesis failed") : err;
                }
                m_lineModel.notifyUpdated(task.lineIndex);
            }

            ++m_workCompleted;
            m_workMessage = tr("Generating audio (%1/%2)…").arg(m_workCompleted).arg(m_workTotal);
            emit workChanged();

            if (!m_synthQueue.isEmpty()) {
                pumpSynthesisQueue();
            } else if (m_activeSynthProcs.isEmpty()) {
                finishBatchSynthesis();
            }
        });

        proc->start(m_ttsPython, {
            m_bridgeScriptPath, "synthesize",
            "--text-file", tmpText->fileName(),
            "--voice", task.voice,
            "--rate", task.rate,
            "--pitch", task.pitch,
            "--out", task.outPath
        });
    }
}

void Backend::finishBatchSynthesis() {
    m_busy = false;
    m_workMessage.clear();
    emit workChanged();

    // Check if any requested lines failed.
    for (const auto &line : m_lineModel.items()) {
        if (line.clipState == "error") {
            setBanner(tr("Audio synthesis failed: %1").arg(line.clipError.left(180)), true);
            m_pendingAction = PendingAction::None;
            m_pendingSinglePlayLine = -1;
            return;
        }
    }

    if (m_pendingSinglePlayLine >= 0 && QFileInfo::exists(m_pendingSinglePlayKey)) {
        const int idx = m_pendingSinglePlayLine;
        const QString path = m_pendingSinglePlayKey;
        m_pendingSinglePlayLine = -1;
        m_pendingSinglePlayKey.clear();
        initAudio();
        stopPlayback();
        m_playingLineIndex = idx;
        m_playingFullScript = false;
        m_player->setSource(QUrl::fromLocalFile(path));
        m_player->play();
        emit playbackChanged();
        return;
    }

    const PendingAction action = m_pendingAction;
    m_pendingAction = PendingAction::None;

    if (action == PendingAction::PlayAll) {
        runStitchForPreview();
    } else if (action == PendingAction::ExportCombined) {
        runExportCombined(m_pendingCombinedPath);
    } else if (action == PendingAction::ExportClips) {
        runExportClips(m_pendingClipsDir);
    } else if (action == PendingAction::ExportBoth) {
        runExportClips(m_pendingClipsDir);
        runExportCombined(m_pendingCombinedPath);
    }
}

void Backend::cancelWork() {
    m_synthQueue.clear();
    for (QProcess *p : std::as_const(m_activeSynthProcs)) {
        if (p && p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(1000);
        }
    }
    m_activeSynthProcs.clear();
    if (m_exportProc && m_exportProc->state() != QProcess::NotRunning) {
        m_exportProc->kill();
        m_exportProc->waitForFinished(1000);
    }
    if (m_voicesProc && m_voicesProc->state() != QProcess::NotRunning) {
        m_voicesProc->kill();
        m_voicesProc->waitForFinished(1000);
    }
    m_pendingAction = PendingAction::None;
    m_pendingSinglePlayLine = -1;
    m_busy = false;
    m_workMessage.clear();
    updateClipStatesFromCache();
    emit workChanged();
}

qint64 Backend::probeDurationMs(const QString &filePath) const {
    if (m_ffprobeBin.isEmpty())
        return 0;
    QProcess proc;
    proc.start(m_ffprobeBin, {
        "-v", "error", "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1", filePath
    });
    if (!proc.waitForFinished(5000) || proc.exitCode() != 0)
        return 0;
    const double sec = QString::fromUtf8(proc.readAllStandardOutput()).trimmed().toDouble();
    return static_cast<qint64>(qRound(sec * 1000.0));
}

bool Backend::runFfmpegStitch(const QStringList &clipPaths, const QVector<int> &pausesMs,
                              const QString &outPath, int bitrateKbps, bool normalize, QString *errorOut) const {
    if (clipPaths.isEmpty()) {
        if (errorOut) *errorOut = tr("No audio clips to combine.");
        return false;
    }
    if (m_ffmpegBin.isEmpty()) {
        if (errorOut) *errorOut = tr("ffmpeg not found.");
        return false;
    }

    const QString filterScript = buildFfmpegFilterScript(pausesMs, normalize);
    QTemporaryFile scriptFile(m_cacheDir + "/ffmpeg_filter_XXXXXX.txt");
    if (!scriptFile.open()) {
        if (errorOut) *errorOut = tr("Could not create temporary filter script.");
        return false;
    }
    scriptFile.write(filterScript.toUtf8());
    scriptFile.close();

    const QString tmpOut = outPath + ".tmp.mp3";
    QFile::remove(tmpOut);

    QStringList args{"-loglevel", "error"};
    for (const auto &p : clipPaths)
        args << "-i" << p;
    args << "-/filter_complex" << scriptFile.fileName()
         << "-map" << "[out]"
         << "-c:a" << "libmp3lame"
         << "-b:a" << QStringLiteral("%1k").arg(bitrateKbps)
         << "-y" << tmpOut;

    QProcess proc;
    proc.start(m_ffmpegBin, args);
    if (!proc.waitForFinished(120000) || proc.exitCode() != 0) {
        QFile::remove(tmpOut);
        if (errorOut)
            *errorOut = QString::fromUtf8(proc.readAllStandardError()).trimmed().right(400);
        return false;
    }

    QFile::remove(outPath);
    if (!QFile::rename(tmpOut, outPath)) {
        QFile::remove(tmpOut);
        if (errorOut) *errorOut = tr("Failed to write output file %1").arg(outPath);
        return false;
    }
    return true;
}

void Backend::runStitchForPreview() {
    QStringList clips;
    QVector<int> pauses;
    QVector<PlaybackSegment> segments;
    qint64 cursor = 0;

    auto &lines = m_lineModel.items();
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].text.trimmed().isEmpty())
            continue;
        const QString path = clipPathForLine(lines[i]);
        if (!QFileInfo::exists(path))
            continue;
        if (lines[i].durationMs <= 0) {
            lines[i].durationMs = probeDurationMs(path);
            m_lineModel.notifyUpdated(i);
        }
        const qint64 dur = qMax<qint64>(100, lines[i].durationMs);
        const int pause = (i < lines.size() - 1) ? lines[i].pauseMs : 0;
        segments.append({i, cursor, cursor + dur + pause});
        cursor += dur + pause;
        clips.append(path);
        pauses.append(lines[i].pauseMs);
    }

    const QString previewPath = m_cacheDir + "/preview_full.mp3";
    QString err;
    if (!runFfmpegStitch(clips, pauses, previewPath, 192, false, &err)) {
        setBanner(tr("Could not assemble preview: %1").arg(err), true);
        return;
    }

    m_playbackSegments = segments;
    initAudio();
    stopPlayback();
    m_playingFullScript = true;
    m_playingLineIndex = segments.isEmpty() ? -1 : segments.first().lineIndex;
    m_player->setSource(QUrl::fromLocalFile(previewPath));
    m_player->play();
    emit playbackChanged();
}

// ---------------- Export ----------------

void Backend::exportCombinedDialog(int bitrateKbps, bool normalize) {
    if (!m_ttsAvailable || !m_ffmpegAvailable) {
        setBanner(m_ttsStatusMessage, true);
        return;
    }
    const QString suggested = sanitizeFilePart(m_title) + ".mp3";
    const QList<FileFilter> filters = {{tr("MP3 Audio (*.mp3)"), {"*.mp3"}}};
    m_picker->saveFile(tr("Export Combined MP3"), suggested, filters,
                       [this, bitrateKbps, normalize](const QString &path) {
        if (path.isEmpty()) return;
        QString out = path;
        if (!out.endsWith(".mp3", Qt::CaseInsensitive))
            out += ".mp3";
        m_pendingCombinedPath = out;
        m_pendingBitrate = bitrateKbps;
        m_pendingNormalize = normalize;

        QVector<int> nonEmpty;
        for (int i = 0; i < m_lineModel.items().size(); ++i)
            if (!m_lineModel.items().at(i).text.trimmed().isEmpty())
                nonEmpty.append(i);
        startBatchSynthesis(nonEmpty, PendingAction::ExportCombined);
    });
}

void Backend::exportClipsDialog(int bitrateKbps) {
    if (!m_ttsAvailable) {
        setBanner(m_ttsStatusMessage, true);
        return;
    }
    m_picker->openDirectory(tr("Choose Folder for Individual MP3 Clips"), [this, bitrateKbps](const QString &dir) {
        if (dir.isEmpty()) return;
        m_pendingClipsDir = dir;
        m_pendingBitrate = bitrateKbps;

        QVector<int> nonEmpty;
        for (int i = 0; i < m_lineModel.items().size(); ++i)
            if (!m_lineModel.items().at(i).text.trimmed().isEmpty())
                nonEmpty.append(i);
        startBatchSynthesis(nonEmpty, PendingAction::ExportClips);
    });
}

void Backend::exportBothDialog(int bitrateKbps, bool normalize) {
    if (!m_ttsAvailable || !m_ffmpegAvailable) {
        setBanner(m_ttsStatusMessage, true);
        return;
    }
    const QString suggested = sanitizeFilePart(m_title) + ".mp3";
    const QList<FileFilter> filters = {{tr("MP3 Audio (*.mp3)"), {"*.mp3"}}};
    m_picker->saveFile(tr("Export Combined MP3"), suggested, filters,
                       [this, bitrateKbps, normalize](const QString &mp3Path) {
        if (mp3Path.isEmpty()) return;
        QString out = mp3Path;
        if (!out.endsWith(".mp3", Qt::CaseInsensitive))
            out += ".mp3";
        m_picker->openDirectory(tr("Choose Folder for Individual MP3 Clips"),
                                [this, out, bitrateKbps, normalize](const QString &dir) {
            if (dir.isEmpty()) return;
            m_pendingCombinedPath = out;
            m_pendingClipsDir = dir;
            m_pendingBitrate = bitrateKbps;
            m_pendingNormalize = normalize;

            QVector<int> nonEmpty;
            for (int i = 0; i < m_lineModel.items().size(); ++i)
                if (!m_lineModel.items().at(i).text.trimmed().isEmpty())
                    nonEmpty.append(i);
            startBatchSynthesis(nonEmpty, PendingAction::ExportBoth);
        });
    });
}

void Backend::runExportCombined(const QString &outPath) {
    QStringList clips;
    QVector<int> pauses;
    for (const auto &line : m_lineModel.items()) {
        if (line.text.trimmed().isEmpty()) continue;
        clips.append(clipPathForLine(line));
        pauses.append(line.pauseMs);
    }
    QString err;
    if (runFfmpegStitch(clips, pauses, outPath, m_pendingBitrate, m_pendingNormalize, &err)) {
        const QString msg = tr("Exported %1").arg(QFileInfo(outPath).fileName());
        setBanner(msg, false);
        emit exportCompleted(true, msg);
    } else {
        setBanner(tr("Export failed: %1").arg(err), true);
        emit exportCompleted(false, err);
    }
}

void Backend::runExportClips(const QString &dirPath) {
    QString err;
    if (exportClipsSync(dirPath, m_pendingBitrate, &err)) {
        const QString msg = tr("Exported individual clips to %1").arg(dirPath);
        setBanner(msg, false);
        emit exportCompleted(true, msg);
    } else {
        setBanner(tr("Clip export failed: %1").arg(err), true);
        emit exportCompleted(false, err);
    }
}

bool Backend::synthesizeAllSync(QString *errorOut) {
    if (!m_ttsAvailable) {
        if (errorOut) *errorOut = m_ttsStatusMessage;
        return false;
    }
    auto &lines = m_lineModel.items();
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].text.trimmed().isEmpty())
            continue;
        const QString outPath = clipPathForLine(lines[i]);
        if (QFileInfo::exists(outPath) && QFileInfo(outPath).size() > 0) {
            lines[i].clipState = "ready";
            continue;
        }
        const Speaker *sp = speakerById(lines[i].speakerId);
        const QString voice = sp ? sp->voice : QStringLiteral("en-US-JennyNeural");
        const int rate = lines[i].hasRateOverride ? lines[i].rateOverride : (sp ? sp->rate : 0);
        const int pitch = lines[i].hasPitchOverride ? lines[i].pitchOverride : (sp ? sp->pitch : 0);

        QTemporaryFile tmpText(m_cacheDir + "/cli_line_XXXXXX.txt");
        if (!tmpText.open()) {
            if (errorOut) *errorOut = tr("Could not create temporary text file.");
            return false;
        }
        tmpText.write(lines[i].text.trimmed().toUtf8());
        tmpText.close();

        QProcess proc;
        proc.start(m_ttsPython, {
            m_bridgeScriptPath, "synthesize",
            "--text-file", tmpText.fileName(),
            "--voice", voice,
            "--rate", formatRate(rate),
            "--pitch", formatPitch(pitch),
            "--out", outPath
        });
        if (!proc.waitForFinished(60000) || proc.exitCode() != 0) {
            if (errorOut)
                *errorOut = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            return false;
        }
        lines[i].clipState = "ready";
    }
    return true;
}

bool Backend::exportCombinedSync(const QString &outputPath, int bitrateKbps, bool normalize, QString *errorOut) {
    if (!synthesizeAllSync(errorOut))
        return false;
    QStringList clips;
    QVector<int> pauses;
    for (const auto &line : m_lineModel.items()) {
        if (line.text.trimmed().isEmpty()) continue;
        clips.append(clipPathForLine(line));
        pauses.append(line.pauseMs);
    }
    return runFfmpegStitch(clips, pauses, outputPath, bitrateKbps, normalize, errorOut);
}

bool Backend::exportClipsSync(const QString &dirPath, int bitrateKbps, QString *errorOut) {
    if (!synthesizeAllSync(errorOut))
        return false;
    QDir().mkpath(dirPath);
    int num = 0;
    for (const auto &line : m_lineModel.items()) {
        if (line.text.trimmed().isEmpty()) continue;
        ++num;
        const Speaker *sp = speakerById(line.speakerId);
        const QString spName = sanitizeFilePart(sp ? sp->name : QStringLiteral("Speaker"));
        const QString dest = QStringLiteral("%1/%2_%3.mp3")
                                 .arg(dirPath)
                                 .arg(num, 3, 10, QChar('0'))
                                 .arg(spName);
        const QString src = clipPathForLine(line);
        if (!m_ffmpegBin.isEmpty() && bitrateKbps > 0) {
            const QString tmp = dest + ".tmp.mp3";
            QFile::remove(tmp);
            QProcess proc;
            proc.start(m_ffmpegBin, {
                "-i", src, "-ar", "44100", "-ac", "2",
                "-c:a", "libmp3lame", "-b:a", QStringLiteral("%1k").arg(bitrateKbps),
                "-y", tmp
            });
            if (proc.waitForFinished(30000) && proc.exitCode() == 0 && QFileInfo::exists(tmp)) {
                QFile::remove(dest);
                if (!QFile::rename(tmp, dest)) {
                    QFile::remove(tmp);
                    if (errorOut) *errorOut = tr("Failed to write %1").arg(dest);
                    return false;
                }
                continue;
            }
            QFile::remove(tmp);
        }
        QFile::remove(dest);
        if (!QFile::copy(src, dest)) {
            if (errorOut) *errorOut = tr("Failed to copy clip to %1").arg(dest);
            return false;
        }
    }
    return true;
}
