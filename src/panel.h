#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>

#include "spectra.h"

// A dye in the panel and the detector it is read in. The detector is kept by
// value (laser and filter), not by index, so editing the instrument never
// silently moves a dye to a different detector.
struct PanelDye {
    Dye dye;
    bool custom = false;  // defined in the file rather than taken from the library
    int laser = 0;        // 0 while the dye has no detector
    Filter band;

    bool assigned() const { return laser > 0; }
};

struct Panel {
    QString title;
    QList<Laser> lasers;
    QList<PanelDye> dyes;

    QList<Channel> channels() const;
    int channelOf(int dye) const;  // index into channels(), or -1
    void assign(int dye, const Channel &channel);
    void unassign(int dye);
    int indexOf(const QString &name) const;

    QJsonObject toJson() const;
    // Fills *error and returns an empty panel when the JSON isn't a panel.
    static Panel fromJson(const QJsonObject &object, QString *error);
    bool save(const QString &path, QString *error) const;
    static Panel load(const QString &path, QString *error);
};

struct Preset {
    QString id;
    QString name;
    QList<Laser> lasers;
};

const QList<Preset> &presets();
const Preset *findPreset(const QString &id);
Panel emptyPanel(const QString &preset = {});

// Gives every dye without a detector the free one that sees it best, taking
// the dyes whose best detector is clearest first. With reassignAll, every dye
// is placed again from scratch; with only given, just those dyes are placed.
void autoAssign(Panel &panel, bool reassignAll, const QList<int> &only = {});

struct Issue {
    enum Kind {
        Unassigned,  // a: the dye has no detector
        Unseen,      // a: no detector on the instrument sees the dye at all
        Shared,      // a and b sit in one detector
        Dim,         // a's laser barely excites it; value is the excitation
        Spill,       // a spills value (of its own signal) into b's detector
        Similar,     // a and b have signatures this similar (cosine)
        Blind,       // a sits on a detector that sees almost none of it; value is its signal there
    };
    enum Severity { Error, Warning, Note };

    Kind kind;
    Severity severity;
    int a = -1;
    int b = -1;
    double value = 0;
};

constexpr double HighSpill = 0.25;
constexpr double ModerateSpill = 0.10;
constexpr double HighSimilarity = 0.90;
constexpr double DimExcitation = 0.25;
// Below this a detector effectively doesn't see a dye (a typical dye on its
// own detector gives about 0.5: brightness, times excitation, times share passed).
constexpr double MinSignal = 0.02;

struct Analysis {
    QList<Channel> channels;
    QVector<int> assigned;               // channel per dye, or -1
    QVector<QVector<double>> signal;     // dye × channel, absolute
    QVector<QVector<double>> spill;      // dye × dye: a's signal in b's detector, over a's own; -1 when unknown
    QVector<QVector<double>> similarity; // dye × dye cosine of signatures across every detector
    QList<Issue> issues;
    double worstSpill = 0;

    int errors() const;
};

Analysis analyze(const Panel &panel);
