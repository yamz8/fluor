#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "panel.h"
#include "spectra.h"
#include "theme.h"
#include "translator.h"

// Answers save dialogs with a fixed path, as a user would.
class FakePicker : public FilePicker {
public:
    QString savePath;
    int saves = 0;
    void openPanel() override {}
    void savePanel(const QUrl &) override {
        ++saves;
        emit saveSelected(QUrl::fromLocalFile(savePath));
    }
};

class FluorTests : public QObject {
    Q_OBJECT

private slots:
    void spectraPeakWhereTheDyeSays()
    {
        const Dye *fitc = spectra::findInLibrary("fitc");
        QVERIFY(fitc);
        QCOMPARE(fitc->name, QString("FITC"));
        const auto curve = spectra::spectrum(*fitc);
        const int peak = int(std::max_element(curve.emission.cbegin(), curve.emission.cend()) - curve.emission.cbegin());
        QCOMPARE(spectra::First + peak, 519);
        QVERIFY(curve.excitationAt(495) > 0.99);
        QVERIFY(curve.excitationAt(640) < 0.01);
    }

    void filtersPassTheirBand()
    {
        const Filter filter{530, 30};
        QCOMPARE(filter.name(), QString("530/30"));
        QVERIFY(spectra::transmission(filter, 530) > 0.9);
        QCOMPARE(spectra::transmission(filter, 560), 0.0);
        QVERIFY(spectra::transmission(filter, 545) > 0 && spectra::transmission(filter, 545) < 0.9);
    }

    void tandemsCarryBothExcitationBands()
    {
        const auto curve = spectra::spectrum(*spectra::findInLibrary("PE-Cy7"));
        QVERIFY(curve.excitationAt(561) > 0.8);
        QVERIFY(curve.excitationAt(488) > 0.5);
    }

    void autoAssignPutsDyesOnTheirDetectors()
    {
        Panel panel = emptyPanel("vbr");
        for (const char *name : {"BV421", "FITC", "APC"})
            panel.dyes << PanelDye{*spectra::findInLibrary(name), false, 0, {}};
        autoAssign(panel, true);
        const auto analysis = analyze(panel);
        QCOMPARE(analysis.channels[analysis.assigned[0]].name(), QString("405 · 450/50"));
        QCOMPARE(analysis.channels[analysis.assigned[1]].name(), QString("488 · 530/30"));
        QCOMPARE(analysis.channels[analysis.assigned[2]].name(), QString("640 · 670/30"));
        QCOMPARE(analysis.errors(), 0);
    }

    void autoAssignNeverDoublesUp()
    {
        Panel panel = emptyPanel("confocal");
        for (const char *name : {"FITC", "Alexa Fluor 488", "EGFP"})
            panel.dyes << PanelDye{*spectra::findInLibrary(name), false, 0, {}};
        autoAssign(panel, true);
        // One green detector: one dye gets it, and the others are left off
        // and reported rather than crammed onto a detector that can't see them.
        QSet<int> used;
        int placed = 0;
        for (int d = 0; d < panel.dyes.size(); ++d) {
            const int channel = panel.channelOf(d);
            if (channel < 0)
                continue;
            QVERIFY(!used.contains(channel));
            used << channel;
            ++placed;
        }
        QCOMPARE(placed, 1);
        QCOMPARE(analyze(panel).errors(), 2);
    }

    void aDyeNoLaserExcitesStaysOff()
    {
        Panel panel = emptyPanel("vbr");
        panel.dyes << PanelDye{*spectra::findInLibrary("Alexa Fluor 350"), false, 0, {}};
        autoAssign(panel, true);
        QVERIFY(!panel.dyes[0].assigned());
        const auto analysis = analyze(panel);
        QCOMPARE(analysis.issues.size(), 1);
        QCOMPARE(analysis.issues[0].kind, Issue::Unseen);
    }

    void analysisFindsSpillAndSharing()
    {
        Panel panel = emptyPanel("vbyr");
        panel.dyes << PanelDye{*spectra::findInLibrary("PE"), false, 561, {586, 15}};
        panel.dyes << PanelDye{*spectra::findInLibrary("PE-CF594"), false, 561, {610, 20}};
        panel.dyes << PanelDye{*spectra::findInLibrary("FITC"), false, 488, {530, 30}};
        panel.dyes << PanelDye{*spectra::findInLibrary("BB515"), false, 488, {530, 30}};
        const auto analysis = analyze(panel);
        QVERIFY(analysis.spill[0][1] >= HighSpill);
        QVERIFY(analysis.spill[2][0] < 0.01);
        QCOMPARE(analysis.errors(), 1);
        QCOMPARE(analysis.issues[0].kind, Issue::Shared);
        bool spill = false;
        for (const auto &issue : analysis.issues)
            spill = spill || (issue.kind == Issue::Spill && issue.a == 0 && issue.b == 1);
        QVERIFY(spill);
    }

    void panelsRoundTripThroughJson()
    {
        QTemporaryDir dir;
        Panel panel = emptyPanel("confocal");
        panel.title = "Mouse brain";
        panel.dyes << PanelDye{*spectra::findInLibrary("DAPI"), false, 405, {450, 50}};
        panel.dyes << PanelDye{Dye{"Glow", 500, 540, 4}, true, 0, {}};
        QString error;
        QVERIFY(panel.save(dir.filePath("a.fluor"), &error));
        const Panel loaded = Panel::load(dir.filePath("a.fluor"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.toJson(), panel.toJson());
        QCOMPARE(loaded.dyes[1].dye.em, 540.0);
        QVERIFY(loaded.dyes[1].custom);
        QCOMPARE(loaded.channelOf(0), 0);
    }

    void badPanelsSayWhatsWrong()
    {
        QString error;
        Panel::fromJson(QJsonDocument::fromJson(R"({"lasers": [], "dyes": [{"name": "Unobtainium"}]})").object(), &error);
        QVERIFY(error.contains("Unknown dye Unobtainium"));
        Panel::fromJson(QJsonDocument::fromJson(R"({"lasers": [{"nm": 488, "filters": ["wide"]}]})").object(), &error);
        QVERIFY(error.contains("center/width"));
        Panel::fromJson(QJsonObject{}, &error);
        QVERIFY(error.contains("Not a Fluor panel"));
    }

    void editsUndoAndTrackUnsavedWork()
    {
        Backend backend;
        QVERIFY(!backend.dirty());
        QVERIFY(backend.addDye("bv421"));
        QVERIFY(!backend.addDye("BV421"));
        QCOMPARE(backend.dyes().size(), 1);
        QCOMPARE(backend.dyes()[0].toMap()["channelName"].toString(), QString("405 · 450/50"));
        QVERIFY(backend.dirty());
        backend.undo();
        QCOMPARE(backend.dyes().size(), 0);
        QVERIFY(!backend.dirty());
        backend.redo();
        QCOMPARE(backend.dyes().size(), 1);
    }

    void retuningAFilterCarriesItsDye()
    {
        Backend backend;
        backend.addDye("FITC");
        const auto channel = backend.panel().dyes[0];
        int laser = -1, filter = -1;
        for (int l = 0; l < backend.panel().lasers.size(); ++l)
            for (int f = 0; f < backend.panel().lasers[l].filters.size(); ++f)
                if (backend.panel().lasers[l].nm == channel.laser && backend.panel().lasers[l].filters[f] == channel.band) {
                    laser = l;
                    filter = f;
                }
        QVERIFY(laser >= 0);
        backend.setFilter(laser, filter, 531, 30);
        backend.setFilter(laser, filter, 532, 30);
        QCOMPARE(backend.panel().dyes[0].band.center, 532);
        QVERIFY(backend.panel().dyes[0].assigned());
        // Stepping a spin box is one undo step.
        backend.undo();
        QCOMPARE(backend.panel().dyes[0].band.center, 530);
    }

    void savingAsWritesThePickedFile()
    {
        QTemporaryDir dir;
        FakePicker picker;
        picker.savePath = dir.filePath("panel");
        Backend backend(&picker);
        QSignalSpy saved(&backend, &Backend::saved);
        backend.addDye("APC");
        backend.save();
        QCOMPARE(picker.saves, 1);
        QCOMPARE(saved.count(), 1);
        QVERIFY(QFile::exists(dir.filePath("panel.fluor")));
        QVERIFY(!backend.dirty());
        QCOMPARE(backend.fileName(), QString("panel.fluor"));
        backend.addDye("FITC");
        backend.save();
        QCOMPARE(picker.saves, 1);
        Backend reopened;
        QVERIFY(reopened.load(dir.filePath("panel.fluor")));
        QCOMPARE(reopened.dyes().size(), 2);
    }

    void searchFindsByNameAndWavelength()
    {
        Backend backend;
        QCOMPARE(backend.searchDyes("af488").value(0).toMap()["name"].toString(), QString("Alexa Fluor 488"));
        QCOMPARE(backend.searchDyes("pecy7").value(0).toMap()["name"].toString(), QString("PE-Cy7"));
        const auto red = backend.searchDyes("640");
        QVERIFY(!red.isEmpty());
        for (const auto &hit : red)
            QVERIFY(std::abs(hit.toMap()["ex"].toDouble() - 640) <= 30 || std::abs(hit.toMap()["em"].toDouble() - 640) <= 15);
    }

    void aDetectorThatMissesItsDyeIsAnError()
    {
        // FITC under a red filter: the laser excites it, the filter passes none of it.
        Panel panel = emptyPanel("vbr");
        panel.dyes << PanelDye{*spectra::findInLibrary("FITC"), false, 488, {780, 60}};
        const auto analysis = analyze(panel);
        QCOMPARE(analysis.errors(), 1);
        QCOMPARE(analysis.issues[0].kind, Issue::Blind);
    }

    void anUnseenPinnedDyeDoesntStopPlacement()
    {
        // BUV395 pinned under the red laser, which can't see it, must not keep
        // the others from being placed well.
        Panel panel = emptyPanel("vbyr");
        panel.dyes << PanelDye{*spectra::findInLibrary("BUV395"), false, 640, {670, 30}};
        for (const char *name : {"PE", "PE-Cy5", "PerCP-Cy5.5"})
            panel.dyes << PanelDye{*spectra::findInLibrary(name), false, 0, {}};
        Panel free = panel;
        free.dyes.removeFirst();
        autoAssign(panel, false);
        autoAssign(free, false);
        for (int d = 1; d < panel.dyes.size(); ++d) {
            QCOMPARE(panel.dyes[d].laser, free.dyes[d - 1].laser);
            QCOMPARE(panel.dyes[d].band, free.dyes[d - 1].band);
        }
        QCOMPARE(panel.dyes[0].laser, 640);
    }

    void presetsLeaveDeliberatelyUnplacedDyesAlone()
    {
        Backend backend;
        backend.addDye("FITC");
        backend.addDye("APC");
        backend.setChannel(1, -1);
        backend.applyPreset("vbyr");
        QVERIFY(backend.panel().dyes[0].assigned());
        QVERIFY(!backend.panel().dyes[1].assigned());
    }

    void loadingRejectsDetectorsTheInstrumentLacks()
    {
        QString error;
        Panel::fromJson(QJsonDocument::fromJson(
            R"({"lasers": [{"nm": 488, "filters": ["530/30"]}], "dyes": [{"name": "FITC", "laser": 488, "filter": "530/31"}]})").object(), &error);
        QVERIFY2(error.contains("doesn't have"), qPrintable(error));
        Panel::fromJson(QJsonDocument::fromJson(
            R"({"lasers": [], "dyes": [{"name": "Glow", "ex": 500, "em": 540, "brightness": 0}]})").object(), &error);
        QVERIFY2(error.contains("brightness"), qPrintable(error));
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
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
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
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(FluorTests)
#include "fluor_tests.moc"
