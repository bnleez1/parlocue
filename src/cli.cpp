#include "cli.h"
#include "backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cstdio>

namespace {

void printErr(const QString &msg, bool json) {
    if (json) {
        QJsonObject o;
        o.insert("ok", false);
        o.insert("error", msg);
        std::fprintf(stderr, "%s\n", QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
    } else {
        std::fprintf(stderr, "%s\n", msg.toUtf8().constData());
    }
}

void printOut(const QJsonObject &o) {
    std::fprintf(stdout, "%s\n", QJsonDocument(o).toJson(QJsonDocument::Indented).constData());
}

void printUsage() {
    std::fprintf(stdout,
        "parlocue — Turn audio scripts into multi-voice MP3s\n\n"
        "Usage:\n"
        "  parlocue [open] [file.parlocue|file.recite|script.txt]  Open the desktop app\n"
        "  parlocue check [file] [--json]                          Verify edge-tts, ffmpeg, and optional script\n"
        "  parlocue voices [--locale PREFIX] [--json]              List available neural voices\n"
        "  parlocue export <file> [--output out.mp3] [--clips-dir dir] [--bitrate 192] [--normalize] [--json]\n"
        "  parlocue skill install                                  Install the parlocue agent skill\n"
        "  parlocue --help                                         Show this help\n");
}

int cmdSkillInstall(bool json) {
    QFile src(":/skill/SKILL.md");
    if (!src.open(QIODevice::ReadOnly)) {
        printErr("Bundled skill resource missing.", json);
        return 1;
    }
    const QByteArray data = src.readAll();
    const QString destDir = QDir::homePath() + "/.agents/skills/parlocue";
    QDir().mkpath(destDir);
    const QString destPath = destDir + "/SKILL.md";
    QSaveFile out(destPath);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
        printErr("Failed to write skill to " + destPath, json);
        return 1;
    }
    if (json) {
        printOut({{"ok", true}, {"path", destPath}});
    } else {
        std::fprintf(stdout, "Installed skill to %s\n", destPath.toUtf8().constData());
    }
    return 0;
}

} // namespace

bool isCliInvocation(const QStringList &args) {
    if (args.size() < 2)
        return false;
    const QString cmd = args.at(1);
    return cmd == "check" || cmd == "voices" || cmd == "export" || cmd == "skill"
        || cmd == "--help" || cmd == "-h" || cmd == "--version" || cmd == "-v";
}

int runCli(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qunsetenv("QT_QPA_PLATFORMTHEME");
    QGuiApplication app(argc, argv);
    app.setApplicationName("parlocue");
    app.setApplicationVersion("0.1.0");

    const QStringList args = app.arguments();
    const QString cmd = args.value(1);
    const bool json = args.contains("--json");

    if (cmd == "--help" || cmd == "-h") {
        printUsage();
        return 0;
    }
    if (cmd == "--version" || cmd == "-v") {
        std::fprintf(stdout, "parlocue 0.1.0\n");
        return 0;
    }
    if (cmd == "skill") {
        if (args.value(2) == "install")
            return cmdSkillInstall(json);
        printErr("Usage: parlocue skill install", json);
        return 1;
    }

    Backend backend;

    if (cmd == "check") {
        QString fileArg;
        for (int i = 2; i < args.size(); ++i) {
            if (!args.at(i).startsWith('-')) {
                fileArg = args.at(i);
                break;
            }
        }
        bool fileOk = true;
        QString fileErr;
        if (!fileArg.isEmpty())
            fileOk = backend.loadFromFile(fileArg, &fileErr);

        const bool ok = backend.ttsAvailable() && backend.ffmpegAvailable() && fileOk;
        if (json) {
            QJsonObject o;
            o.insert("ok", ok);
            o.insert("ttsAvailable", backend.ttsAvailable());
            o.insert("ffmpegAvailable", backend.ffmpegAvailable());
            o.insert("status", backend.ttsStatusMessage());
            if (!fileArg.isEmpty()) {
                o.insert("file", fileArg);
                o.insert("lines", backend.lineCount());
                o.insert("words", backend.wordCount());
                if (!fileOk)
                    o.insert("fileError", fileErr);
            }
            printOut(o);
        } else if (ok) {
            std::fprintf(stdout, "OK: %s\n", backend.ttsStatusMessage().toUtf8().constData());
            if (!fileArg.isEmpty())
                std::fprintf(stdout, "Script: %d lines, %d words\n", backend.lineCount(), backend.wordCount());
        } else {
            printErr(!fileOk ? fileErr : backend.ttsStatusMessage(), false);
        }
        return ok ? 0 : 1;
    }

    if (cmd == "voices") {
        QString localePrefix;
        for (int i = 2; i < args.size(); ++i) {
            if (args.at(i) == "--locale" && i + 1 < args.size())
                localePrefix = args.at(++i);
        }
        if (backend.ttsAvailable()) {
            QString err;
            backend.fetchVoicesSync(&err);
        }
        QJsonArray arr;
        for (const auto &v : backend.voiceModel()->allVoices()) {
            if (!localePrefix.isEmpty() && !v.locale.startsWith(localePrefix, Qt::CaseInsensitive))
                continue;
            QJsonObject o;
            o.insert("shortName", v.shortName);
            o.insert("displayName", v.displayName);
            o.insert("locale", v.locale);
            o.insert("gender", v.gender);
            arr.append(o);
        }
        if (json) {
            printOut({{"ok", true}, {"count", arr.size()}, {"voices", arr}});
        } else {
            for (const auto &val : arr) {
                const auto o = val.toObject();
                std::fprintf(stdout, "%-28s %-8s %-8s %s\n",
                             o.value("shortName").toString().toUtf8().constData(),
                             o.value("locale").toString().toUtf8().constData(),
                             o.value("gender").toString().toUtf8().constData(),
                             o.value("displayName").toString().toUtf8().constData());
            }
        }
        return 0;
    }

    if (cmd == "export") {
        QString fileArg;
        QString outMp3;
        QString clipsDir;
        int bitrate = 192;
        bool normalize = false;

        for (int i = 2; i < args.size(); ++i) {
            const QString a = args.at(i);
            if (a == "--output" && i + 1 < args.size())
                outMp3 = args.at(++i);
            else if (a == "--clips-dir" && i + 1 < args.size())
                clipsDir = args.at(++i);
            else if (a == "--bitrate" && i + 1 < args.size())
                bitrate = args.at(++i).toInt();
            else if (a == "--normalize")
                normalize = true;
            else if (a == "--json")
                continue;
            else if (!a.startsWith('-') && fileArg.isEmpty())
                fileArg = a;
        }

        if (fileArg.isEmpty() || (outMp3.isEmpty() && clipsDir.isEmpty())) {
            printErr("Usage: parlocue export <file> [--output out.mp3] [--clips-dir dir]", json);
            return 1;
        }

        QString err;
        if (!backend.loadFromFile(fileArg, &err)) {
            printErr(err, json);
            return 1;
        }
        if (!outMp3.isEmpty() && !backend.exportCombinedSync(outMp3, bitrate, normalize, &err)) {
            printErr(err, json);
            return 1;
        }
        if (!clipsDir.isEmpty() && !backend.exportClipsSync(clipsDir, bitrate, &err)) {
            printErr(err, json);
            return 1;
        }

        if (json) {
            QJsonObject o;
            o.insert("ok", true);
            o.insert("lines", backend.lineCount());
            if (!outMp3.isEmpty()) o.insert("output", outMp3);
            if (!clipsDir.isEmpty()) o.insert("clipsDir", clipsDir);
            printOut(o);
        } else {
            if (!outMp3.isEmpty())
                std::fprintf(stdout, "Exported %s\n", outMp3.toUtf8().constData());
            if (!clipsDir.isEmpty())
                std::fprintf(stdout, "Exported clips to %s\n", clipsDir.toUtf8().constData());
        }
        return 0;
    }

    printErr("Unknown command: " + cmd, json);
    return 1;
}
