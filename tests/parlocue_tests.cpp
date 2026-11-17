#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "theme.h"
#include "translator.h"

class FakeFilePicker : public FilePicker {
public:
    QString nextOpenPath;
    QString nextSavePath;
    QString nextDirPath;

    void openFile(const QString &, const QList<FileFilter> &, Callback cb) override {
        if (cb) cb(nextOpenPath);
    }
    void saveFile(const QString &, const QString &, const QList<FileFilter> &, Callback cb) override {
        if (cb) cb(nextSavePath);
    }
    void openDirectory(const QString &, Callback cb) override {
        if (cb) cb(nextDirPath);
    }
};

class ParloCueTests : public QObject {
    Q_OBJECT

private slots:
    void parseMultiSpeakerDialogueAndPauseTags()
    {
        QTemporaryDir cacheDir;
        FakeFilePicker picker;
        Backend backend(&picker, cacheDir.path(), "/nonexistent/python");

        const QString script =
            "Narrator: Listen to a conversation between a student and an advisor.\n"
            "[pause 1.5s]\n"
            "Student: Hi! Do you have a minute?\n"
            "Man: Sure, come on in.\n"
            "Woman: Thanks so much!\n";

        const QVariantMap preview = backend.parseScriptPreview(script, true);
        QCOMPARE(preview.value("lineCount").toInt(), 4);
        QCOMPARE(preview.value("speakerCount").toInt(), 4);

        backend.importScriptText(script, true, {}, true);
        QCOMPARE(backend.lineCount(), 4);
        QCOMPARE(backend.speakerModel()->rowCount(), 4);
        QCOMPARE(backend.lineModel()->items().at(0).pauseMs, 1500);
        QCOMPARE(backend.lineModel()->items().at(1).text, QString("Hi! Do you have a minute?"));
    }

    void parseSingleSpeakerLectureParagraphs()
    {
        QTemporaryDir cacheDir;
        FakeFilePicker picker;
        Backend backend(&picker, cacheDir.path(), "/nonexistent/python");

        const QString lecture =
            "Today we are going to examine how glaciers shape mountain valleys.\n"
            "Let us begin with accumulation zones.\n\n"
            "As snow compresses over centuries, it forms dense glacial ice.\n";

        backend.importScriptText(lecture, false, {}, true);
        QCOMPARE(backend.lineCount(), 2);
        QCOMPARE(backend.speakerModel()->rowCount(), 1);
        QVERIFY(backend.wordCount() >= 20);
    }

    void randomizeVoicesAcrossDialectsAndImport()
    {
        QTemporaryDir cacheDir;
        FakeFilePicker picker;
        Backend backend(&picker, cacheDir.path(), "/nonexistent/python");

        const QString script =
            "Narrator: Welcome.\n"
            "Man: Hello from the first speaker.\n"
            "Woman: Hello from the second speaker.\n"
            "Guest: Hello from the third speaker.\n";

        backend.importScriptText(script, true, {}, true, true);
        QCOMPARE(backend.speakerModel()->rowCount(), 4);

        QSet<QString> initialVoices;
        QSet<QString> initialLocales;
        const auto &speakers = backend.speakerModel()->items();
        for (const auto &sp : speakers) {
            QVERIFY(sp.voice.startsWith("en-"));
            initialVoices.insert(sp.voice);
            initialLocales.insert(sp.voice.section('-', 0, 1));
        }
        // 4 speakers should get 4 distinct voices across all 4 built-in English locales.
        QCOMPARE(initialVoices.size(), 4);
        QCOMPARE(initialLocales.size(), 4);

        // Gender hints should be respected for Man / Woman.
        QString manVoice;
        QString womanVoice;
        for (const auto &sp : speakers) {
            if (sp.name == "Man") manVoice = sp.voice;
            if (sp.name == "Woman") womanVoice = sp.voice;
        }
        for (const auto &v : backend.voiceModel()->allVoices()) {
            if (v.shortName == manVoice) QCOMPARE(v.gender, QString("Male"));
            if (v.shortName == womanVoice) QCOMPARE(v.gender, QString("Female"));
        }

        // Randomizing all speakers updates voices and supports undo.
        const QString firstBefore = speakers.first().voice;
        backend.randomizeSpeakerVoices();
        const auto &after = backend.speakerModel()->items();
        QSet<QString> rerolledVoices;
        for (const auto &sp : after)
            rerolledVoices.insert(sp.voice);
        QCOMPARE(rerolledVoices.size(), 4);
        QVERIFY(after.first().voice != firstBefore);

        backend.undo();
        QCOMPARE(backend.speakerModel()->items().first().voice, firstBefore);
    }

    void undoRedoAndProjectSaveLoad()
    {
        QTemporaryDir cacheDir;
        QTemporaryDir projectDir;
        FakeFilePicker picker;
        Backend backend(&picker, cacheDir.path(), "/nonexistent/python");

        backend.setLineText(0, "First sentence.");
        backend.addLine(0, {}, "Second sentence.");
        QCOMPARE(backend.lineCount(), 2);

        backend.undo();
        QCOMPARE(backend.lineCount(), 1);
        backend.redo();
        QCOMPARE(backend.lineCount(), 2);

        const QString projectPath = projectDir.filePath("lesson1.parlocue");
        QVERIFY(backend.saveToFile(projectPath));
        QCOMPARE(backend.dirty(), false);

        Backend reloaded(&picker, cacheDir.path(), "/nonexistent/python");
        QVERIFY(reloaded.loadFromFile(projectPath));
        QCOMPARE(reloaded.lineCount(), 2);
        QCOMPARE(reloaded.lineModel()->items().at(1).text, QString("Second sentence."));

        // Legacy .recite project files remain readable.
        const QString legacyPath = projectDir.filePath("legacy.recite");
        QVERIFY(QFile::copy(projectPath, legacyPath));
        Backend legacyReloaded(&picker, cacheDir.path(), "/nonexistent/python");
        QVERIFY(legacyReloaded.loadFromFile(legacyPath));
        QCOMPARE(legacyReloaded.lineCount(), 2);
    }

    void pureFormattingAndFilterScript()
    {
        QCOMPARE(Backend::formatRate(0), QString("+0%"));
        QCOMPARE(Backend::formatRate(-15), QString("-15%"));
        QCOMPARE(Backend::formatRate(25), QString("+25%"));
        QCOMPARE(Backend::formatPitch(-10), QString("-10Hz"));
        QCOMPARE(Backend::formatPitch(8), QString("+8Hz"));
        QCOMPARE(Backend::sanitizeFilePart("Dr. Smith / Lab!"), QString("Dr_Smith_Lab"));

        const QString k1 = Backend::clipCacheKey("Hello", "en-US-JennyNeural", 0, 0);
        const QString k2 = Backend::clipCacheKey("Hello", "en-US-JennyNeural", -15, 0);
        QVERIFY(k1 != k2);

        const QString filter = Backend::buildFfmpegFilterScript({600, 1000}, true);
        QVERIFY(filter.contains("apad=pad_dur=0.600"));
        QVERIFY(filter.contains("concat=n=2:v=0:a=1"));
        QVERIFY(filter.contains("loudnorm=I=-14:"));
    }

    void endToEndFakeTtsExportCombinedAndClips()
    {
        const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty())
            QSKIP("ffmpeg not installed");

        QTemporaryDir tmp;
        // Create a fake python executable that handles `check` and `synthesize`
        // by generating a valid 0.3s MP3 with ffmpeg.
        const QString fakePython = tmp.filePath("fake_tts.sh");
        const QByteArray script =
            "#!/bin/sh\n"
            "cmd=\"$2\"\n"
            "if [ \"$cmd\" = \"check\" ]; then\n"
            "  echo '{\"ok\":true,\"version\":\"test\"}'\n"
            "  exit 0\n"
            "fi\n"
            "if [ \"$cmd\" = \"synthesize\" ]; then\n"
            "  out=\"\"\n"
            "  while [ $# -gt 0 ]; do\n"
            "    if [ \"$1\" = \"--out\" ]; then out=\"$2\"; fi\n"
            "    shift\n"
            "  done\n"
            "  ffmpeg -loglevel error -f lavfi -i \"sine=frequency=440:duration=0.3\" -c:a libmp3lame -y \"$out\"\n"
            "  exit $?\n"
            "fi\n"
            "exit 1\n";
        write(fakePython, script);
        QVERIFY(QFile::setPermissions(fakePython, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        FakeFilePicker picker;
        Backend backend(&picker, tmp.filePath("cache"), fakePython);
        QVERIFY(backend.ttsAvailable());
        QVERIFY(backend.ffmpegAvailable());

        backend.importScriptText("Student: Hello.\n[pause 0.5s]\nAdvisor: Welcome.", true, {}, true);
        QCOMPARE(backend.lineCount(), 2);

        const QString outMp3 = tmp.filePath("combined.mp3");
        QString err;
        QVERIFY2(backend.exportCombinedSync(outMp3, 192, false, &err), qPrintable(err));
        QVERIFY(QFileInfo::exists(outMp3));
        QVERIFY(QFileInfo(outMp3).size() > 1000);

        const QString clipsDir = tmp.filePath("clips_out");
        QVERIFY2(backend.exportClipsSync(clipsDir, 192, &err), qPrintable(err));
        QVERIFY(QFileInfo::exists(clipsDir + "/001_Student.mp3"));
        QVERIFY(QFileInfo::exists(clipsDir + "/002_Advisor.mp3"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeReadsFullRistrettoPalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "mode = \"dark\"\n"
              "accent = \"#f38d70\"\n"
              "background = \"#2c2525\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#f38d70"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QCOMPARE(theme.isDark(), false);
        QCOMPARE(theme.bgTop(), QColor("#F5EFEA"));
        QCOMPARE(theme.bgMid(), QColor("#E8DFDA"));
        QCOMPARE(theme.bgBottom(), QColor("#DAD0C9"));
        QCOMPARE(theme.textPrimary(), QColor("#1E1717"));
        QCOMPARE(theme.accentControlForeground(), QColor("#FFFFFF"));
        // Bright peach accent is darkened on light surfaces for button & text contrast.
        QVERIFY(theme.accentControl().lightnessF() < QColor("#f38d70").lightnessF());
        QVERIFY(theme.accentText().lightnessF() < QColor("#f38d70").lightnessF());
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json");
        const QString command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n lines": {"one": "%n line", "other": "%n lines"}})");
        write(dir.path() + "/ru.json", R"({"%n lines": {"one": "%n строка", "few": "%n строки", "many": "%n строк", "other": "%n строки"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n lines", nullptr, 21), QString("%n строка"));
        QCOMPARE(translator.translate("Main", "%n lines", nullptr, 22), QString("%n строки"));
        QCOMPARE(translator.translate("Main", "%n lines", nullptr, 25), QString("%n строк"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

    void importWordDocxAndErrorHandling()
    {
        QTemporaryDir dir;
        QTemporaryDir cacheDir;
        FakeFilePicker picker;
        Backend backend(&picker, cacheDir.path(), "/nonexistent/python");

        const QString docxPath = dir.filePath("dialogue.docx");
        const QString py = QStandardPaths::findExecutable("python3");
        QVERIFY(!py.isEmpty());

        QProcess zipProc;
        zipProc.start(py, {
            "-c",
            "import sys, zipfile; "
            "xml = '<?xml version=\"1.0\" encoding=\"UTF-8\"?>'"
            "'<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">'"
            "'<w:body>'"
            "'<w:p><w:r><w:t>Narrator: Welcome to the course.</w:t></w:r></w:p>'"
            "'<w:p><w:r><w:t>Student: Thanks!</w:t></w:r></w:p>'"
            "'</w:body></w:document>'; "
            "zf = zipfile.ZipFile(sys.argv[1], 'w'); "
            "zf.writestr('word/document.xml', xml); "
            "zf.close()",
            docxPath
        });
        QVERIFY(zipProc.waitForFinished(10000));
        QCOMPARE(zipProc.exitCode(), 0);

        QSignalSpy extractedSpy(&backend, &Backend::documentTextExtracted);
        picker.nextOpenPath = docxPath;
        backend.importDocumentDialog();

        QCOMPARE(extractedSpy.count(), 1);
        const QString extracted = extractedSpy.at(0).at(0).toString();
        QVERIFY(extracted.contains("Narrator: Welcome to the course."));
        QVERIFY(extracted.contains("Student: Thanks!"));
        QCOMPARE(extractedSpy.at(0).at(1).toString(), QString("dialogue"));

        const QVariantMap preview = backend.parseScriptPreview(extracted, true);
        QCOMPARE(preview.value("lineCount").toInt(), 2);
        QCOMPARE(preview.value("speakerCount").toInt(), 2);

        QString textOut;
        QString errOut;
        QVERIFY(!backend.extractDocumentText(dir.filePath("missing.docx"), &textOut, &errOut));
        QVERIFY(!errOut.isEmpty());

        const QString badExt = dir.filePath("notes.xyz");
        write(badExt, "hello");
        errOut.clear();
        QVERIFY(!backend.extractDocumentText(badExt, &textOut, &errOut));
        QVERIFY(errOut.contains("Unsupported"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), contents.size());
    }
};

QTEST_MAIN(ParloCueTests)
#include "parlocue_tests.moc"
