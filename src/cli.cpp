// The command line follows Hype's (MIT, David Heinemeier Hansson).
#include "cli.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>
#include <cstdio>

#include "backend.h"
#include "panel.h"

namespace {

void print(FILE *stream, const QString &text) {
    // Errors go to stderr between lines of stdout; flush so they stay in order.
    fflush(stdout);
    fprintf(stream, "%s\n", qPrintable(text));
}
void print(const QJsonObject &object) { fprintf(stdout, "%s", QJsonDocument(object).toJson().constData()); }
int fail(const QString &message) {
    print(stderr, message);
    return 1;
}

QString count(int n, const QString &noun) { return QString("%1 %2%3").arg(n).arg(noun, n == 1 ? "" : "s"); }

QString percent(double value) { return QString::number(int(std::round(value * 100))) + "%"; }

const char *severityName(Issue::Severity severity) {
    return severity == Issue::Error ? "error" : severity == Issue::Warning ? "warning" : "note";
}

const char *kindName(Issue::Kind kind) {
    static const char *names[] = {"unassigned", "unseen", "shared", "dim", "spill", "similar", "blind"};
    return names[kind];
}

QString channelName(const Analysis &analysis, int dye) {
    const int c = analysis.assigned.value(dye, -1);
    return c >= 0 ? analysis.channels[c].name() : QString("none");
}

QString message(const Panel &panel, const Analysis &analysis, const Issue &issue) {
    const QString a = issue.a >= 0 ? panel.dyes[issue.a].dye.name : QString();
    const QString b = issue.b >= 0 ? panel.dyes[issue.b].dye.name : QString();
    switch (issue.kind) {
    case Issue::Unassigned:
        return a + " has no detector.";
    case Issue::Unseen:
        return "No laser and filter on this instrument sees " + a + ".";
    case Issue::Shared:
        return a + " and " + b + " share the " + channelName(analysis, issue.a) + " detector.";
    case Issue::Dim:
        return QString("The %1 nm laser excites %2 at only %3 of its peak.")
            .arg(analysis.channels[analysis.assigned[issue.a]].nm).arg(a, percent(issue.value));
    case Issue::Spill:
        return QString("%1 spills %2 of its signal into %3's detector (%4).")
            .arg(a, percent(issue.value), b, channelName(analysis, issue.b));
    case Issue::Blind:
        return a + "'s detector (" + channelName(analysis, issue.a) + ") sees almost none of its light.";
    case Issue::Similar:
        return QString("%1 and %2 look alike across every detector (similarity %3), so they are hard to unmix.")
            .arg(a, b, QString::number(issue.value, 'f', 2));
    }
    return {};
}

struct Command {
    QCommandLineParser parser;
    Command(const QString &name, const QString &description, bool panel = true) {
        parser.setApplicationDescription(description);
        parser.addHelpOption();
        parser.addPositionalArgument(name, description, name);
        if (panel)
            parser.addPositionalArgument("panel", "Fluor panel file", "<panel.fluor>");
    }
    void json() { parser.addOption({"json", "Print the result as JSON"}); }
    bool wantsJson() const { return parser.isSet("json"); }
    QString argument(int index) const { return parser.positionalArguments().value(index); }
    bool load(Panel &panel) const {
        if (argument(1).isEmpty()) {
            print(stderr, "Name a panel file.");
            return false;
        }
        QString error;
        panel = Panel::load(argument(1), &error);
        if (!error.isEmpty()) {
            print(stderr, argument(1) + ": " + error);
            return false;
        }
        return true;
    }
};

QJsonArray assignments(const Panel &panel, const Analysis &analysis) {
    QJsonArray list;
    for (int d = 0; d < panel.dyes.size(); ++d) {
        const int c = analysis.assigned[d];
        const auto curve = spectra::spectrum(panel.dyes[d].dye);
        QJsonObject item{{"name", panel.dyes[d].dye.name}, {"ex", panel.dyes[d].dye.ex},
                         {"em", panel.dyes[d].dye.em}, {"brightness", panel.dyes[d].dye.brightness}};
        if (c >= 0) {
            item["laser"] = analysis.channels[c].nm;
            item["filter"] = analysis.channels[c].band.name();
            item["excitation"] = std::round(curve.excitationAt(analysis.channels[c].nm) * 1000) / 1000;
            item["captured"] = std::round(spectra::captured(curve, analysis.channels[c].band) * 1000) / 1000;
        }
        list << item;
    }
    return list;
}

void printAssignments(const Panel &panel, const Analysis &analysis) {
    print(stdout, QString("  %1 %2 %3 %4").arg("Dye", -18).arg("Detector", -16).arg("Excited", 8).arg("Captured", 9));
    for (int d = 0; d < panel.dyes.size(); ++d) {
        const int c = analysis.assigned[d];
        const auto curve = spectra::spectrum(panel.dyes[d].dye);
        print(stdout, QString("  %1 %2 %3 %4")
                          .arg(panel.dyes[d].dye.name, -18)
                          .arg(channelName(analysis, d), -16)
                          .arg(c >= 0 ? percent(curve.excitationAt(analysis.channels[c].nm)) : "-", 8)
                          .arg(c >= 0 ? percent(spectra::captured(curve, analysis.channels[c].band)) : "-", 9));
    }
}

int check(const QStringList &arguments) {
    Command command("check", "Predict how the panel's dyes bleed into each other's detectors.");
    command.json();
    command.parser.process(arguments);
    Panel panel;
    if (!command.load(panel))
        return 1;
    const Analysis analysis = analyze(panel);
    const int errors = analysis.errors();
    const int n = panel.dyes.size();

    if (command.wantsJson()) {
        QJsonArray names, matrix, issues;
        for (int a = 0; a < n; ++a) {
            names << panel.dyes[a].dye.name;
            QJsonArray row;
            for (int b = 0; b < n; ++b)
                row << (analysis.spill[a][b] < 0 ? QJsonValue() : QJsonValue(std::round(analysis.spill[a][b] * 1000) / 1000));
            matrix << row;
        }
        for (const auto &issue : analysis.issues)
            issues << QJsonObject{{"severity", severityName(issue.severity)}, {"kind", kindName(issue.kind)},
                                  {"a", issue.a >= 0 ? QJsonValue(panel.dyes[issue.a].dye.name) : QJsonValue()},
                                  {"b", issue.b >= 0 ? QJsonValue(panel.dyes[issue.b].dye.name) : QJsonValue()},
                                  {"value", std::round(issue.value * 1000) / 1000},
                                  {"message", message(panel, analysis, issue)}};
        print({{"ok", errors == 0}, {"title", panel.title}, {"dyes", assignments(panel, analysis)},
               {"spillover", QJsonObject{{"dyes", names}, {"matrix", matrix},
                                         {"note", "matrix[a][b]: a's signal in b's detector, over a's signal in its own"}}},
               {"worstSpill", std::round(analysis.worstSpill * 1000) / 1000}, {"issues", issues}});
        return errors ? 1 : 0;
    }

    print(stdout, QString("%1%2, %3")
                      .arg(panel.title.isEmpty() ? QString() : panel.title + ": ",
                           count(n, "dye"), count(analysis.channels.size(), "detector")));
    printAssignments(panel, analysis);
    if (n > 1) {
        print(stdout, "\nSpillover: each row's signal in each column's detector, as a share of its own");
        QString header = QString("  %1").arg("", -18);
        for (int b = 0; b < n; ++b)
            header += QString(" %1").arg(panel.dyes[b].dye.name.left(6), 6);
        print(stdout, header);
        for (int a = 0; a < n; ++a) {
            QString row = QString("  %1").arg(panel.dyes[a].dye.name, -18);
            for (int b = 0; b < n; ++b) {
                const double value = analysis.spill[a][b];
                row += QString(" %1").arg(a == b ? "-" : value < 0 ? "?" : percent(value), 6);
            }
            print(stdout, row);
        }
    }
    if (!analysis.issues.isEmpty())
        print(stdout, "");
    for (const auto &issue : analysis.issues)
        print(issue.severity == Issue::Error ? stderr : stdout,
              QString("%1: %2").arg(severityName(issue.severity), message(panel, analysis, issue)));
    const int warnings = int(std::count_if(analysis.issues.cbegin(), analysis.issues.cend(),
                                           [](const Issue &i) { return i.severity == Issue::Warning; }));
    print(stdout, QString("\nWorst spillover %1; %2, %3")
                      .arg(percent(analysis.worstSpill), count(errors, "error"), count(warnings, "warning")));
    return errors ? 1 : 0;
}

bool saveOrFail(const Panel &panel, const QString &path) {
    QString error;
    if (panel.save(path, &error))
        return true;
    print(stderr, path + ": " + error);
    return false;
}

int assign(const QStringList &arguments) {
    Command command("assign", "Give dyes without a detector the best free one, and save the panel.");
    command.parser.addOption({"all", "Place every dye again from scratch"});
    command.json();
    command.parser.process(arguments);
    Panel panel;
    if (!command.load(panel))
        return 1;
    autoAssign(panel, command.parser.isSet("all"));
    if (!saveOrFail(panel, command.argument(1)))
        return 1;
    const Analysis analysis = analyze(panel);
    if (command.wantsJson())
        print({{"panel", QFileInfo(command.argument(1)).absoluteFilePath()}, {"dyes", assignments(panel, analysis)}});
    else
        printAssignments(panel, analysis);
    return 0;
}

int create(const QStringList &arguments) {
    Command command("new", "Start a panel on an instrument preset, with the dyes given placed on detectors.");
    command.parser.addOption({"preset", "Instrument; see fluor presets (default: vbr)", "id", "vbr"});
    command.parser.addOption({"title", "Panel title", "title"});
    command.parser.addOption({"dye", "A dye to add; repeat for more", "name"});
    command.json();
    command.parser.process(arguments);
    const QString path = command.argument(1);
    if (path.isEmpty())
        return fail("Name the panel file to create.");
    if (QFileInfo::exists(path))
        return fail(path + " already exists.");
    if (!findPreset(command.parser.value("preset")))
        return fail("No preset " + command.parser.value("preset") + "; see fluor presets.");
    Panel panel = emptyPanel(command.parser.value("preset"));
    panel.title = command.parser.value("title");
    for (const auto &name : command.parser.values("dye")) {
        const Dye *dye = spectra::findInLibrary(name);
        if (!dye)
            return fail("Unknown dye " + name + "; see fluor dyes.");
        if (panel.indexOf(dye->name) < 0)
            panel.dyes << PanelDye{*dye, false, 0, {}};
    }
    autoAssign(panel, true);
    if (!saveOrFail(panel, path))
        return 1;
    if (command.wantsJson())
        print({{"panel", QFileInfo(path).absoluteFilePath()}, {"dyes", assignments(panel, analyze(panel))}});
    else
        print(stdout, "Created " + path);
    return 0;
}

int dyes(const QStringList &arguments) {
    Command command("dyes", "List the dyes Fluor knows, or those matching a name or a wavelength.", false);
    command.parser.addPositionalArgument("query", "Part of a name, or a laser or emission wavelength", "[query]");
    command.json();
    command.parser.process(arguments);
    const Backend backend;
    const auto hits = backend.searchDyes(command.argument(1));
    QJsonArray list;
    for (const auto &hit : hits) {
        const auto item = hit.toMap();
        list << QJsonObject{{"name", item["name"].toString()}, {"ex", item["ex"].toDouble()},
                            {"em", item["em"].toDouble()}, {"brightness", item["brightness"].toDouble()}};
        if (!command.wantsJson())
            print(stdout, QString("%1  ex %2  em %3  brightness %4")
                              .arg(item["name"].toString(), -18)
                              .arg(item["ex"].toInt(), 3).arg(item["em"].toInt(), 3)
                              .arg(item["brightness"].toInt()));
    }
    if (command.wantsJson())
        print({{"dyes", list}});
    return hits.isEmpty() ? 1 : 0;
}

int presetList(const QStringList &arguments) {
    Command command("presets", "List the instrument presets: lasers and their filters.", false);
    command.json();
    command.parser.process(arguments);
    QJsonArray list;
    for (const auto &preset : presets()) {
        QJsonArray lasers;
        QStringList text;
        for (const auto &laser : preset.lasers) {
            QJsonArray filters;
            QStringList names;
            for (const auto &filter : laser.filters) {
                filters << filter.name();
                names << filter.name();
            }
            lasers << QJsonObject{{"nm", laser.nm}, {"filters", filters}};
            text << QString("%1: %2").arg(laser.nm).arg(names.join(" "));
        }
        list << QJsonObject{{"id", preset.id}, {"name", preset.name}, {"lasers", lasers}};
        if (!command.wantsJson())
            print(stdout, QString("%1  %2\n      %3").arg(preset.id, -9).arg(preset.name, text.join("\n      ")));
    }
    if (command.wantsJson())
        print({{"presets", list}});
    return 0;
}

// As Hype does: one shared copy, linked into Claude Code, found directly by Codex.
int skill(const QStringList &arguments) {
    Command command("skill", "Print the agent skill, or install it for your coding agents with: skill install.", false);
    command.parser.addPositionalArgument("install", "Copy to ~/.agents/skills/fluor and link into ~/.claude/skills", "[install]");
    command.parser.process(arguments);
    QFile bundled(":/skill.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return 1;
    const QByteArray text = bundled.readAll();
    if (command.argument(1).isEmpty()) {
        fputs(text.constData(), stdout);
        return 0;
    }
    if (command.argument(1) != "install")
        return fail("Use fluor skill to print the skill, or fluor skill install.");
    const QDir home = QDir::home();
    const QString directory = home.filePath(".agents/skills/fluor");
    QSaveFile file(directory + "/SKILL.md");
    if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) || file.write(text) < 0 || !file.commit())
        return fail("Could not write " + file.fileName());
    print(stdout, "Installed " + file.fileName());
    if (!home.exists(".claude"))
        return 0;
    const QString link = home.filePath(".claude/skills/fluor");
    const QFileInfo existing(link);
    if (existing.isSymLink())
        QFile::remove(link);
    else if (existing.exists())
        return fail(link + " exists and is not a link; leaving it alone.");
    if (!home.mkpath(".claude/skills") || !QFile::link("../../.agents/skills/fluor", link))
        return fail("Could not link " + link);
    print(stdout, "Linked " + link);
    return 0;
}

int help(const QStringList &arguments) {
    const QString topic = arguments.value(2);
    if (topic == "format") {
        QFile guide(":/format.md");
        if (!guide.open(QIODevice::ReadOnly))
            return 1;
        fputs(guide.readAll().constData(), stdout);
        return 0;
    }
    if (isCliCommand(topic) && topic != "help")
        return runCli({arguments[0], topic, "--help"});
    print(stdout, "Usage: fluor <command> [options]\n\n" + cliSummary());
    return 0;
}

}  // namespace

QString cliSummary() {
    return "Planner:\n"
           "  open [panel]              Open the panel planner\n\n"
           "Commands that need no display (fluor help <command> for options):\n"
           "  new <panel>               Start a panel from a preset and dyes\n"
           "  check <panel>             Predict spillover and report problems\n"
           "  assign <panel>            Place dyes on their best free detectors\n"
           "  dyes [query]              List known dyes, by name or wavelength\n"
           "  presets                   List instrument presets\n"
           "  help format               How a panel file is written\n"
           "  skill [install]           Print the skill for coding agents, or install it";
}

bool isCliCommand(const QString &word) {
    return QStringList{"new", "check", "assign", "dyes", "presets", "skill", "help"}.contains(word);
}

int runCli(const QStringList &arguments) {
    const QString command = arguments.value(1);
    return command == "new"       ? create(arguments)
           : command == "check"   ? check(arguments)
           : command == "assign"  ? assign(arguments)
           : command == "dyes"    ? dyes(arguments)
           : command == "presets" ? presetList(arguments)
           : command == "skill"   ? skill(arguments)
                                  : help(arguments);
}
