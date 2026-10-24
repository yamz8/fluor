#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QVector>

// A fluorophore, described by its peaks rather than measured spectra. The
// curves drawn and integrated are modeled from these numbers (see spectrum()),
// which is close enough to plan a panel and spot the overlaps that matter.
struct Dye {
    QString name;
    double ex = 0;          // excitation peak, nm
    double em = 0;          // emission peak, nm
    double brightness = 3;  // relative, 1 (dim) to 5 (very bright)
    double ex2 = 0;         // second excitation band, as PE's at 496 nm
    double ex2Amount = 0;
    double em2 = 0;         // residual donor emission of a tandem dye
    double em2Amount = 0;
};

struct Filter {
    int center = 0;
    int width = 0;

    QString name() const { return QString("%1/%2").arg(center).arg(width); }
    bool operator==(const Filter &other) const { return center == other.center && width == other.width; }
};

struct Laser {
    int nm = 0;
    QList<Filter> filters;
};

// One detector: a filter behind a laser.
struct Channel {
    int laser = 0;   // index into the lasers
    int filter = 0;  // index into that laser's filters
    int nm = 0;
    Filter band;

    QString name() const { return QString("%1 · %2").arg(nm).arg(band.name()); }
};

namespace spectra {

constexpr int First = 300;
constexpr int Last = 900;
constexpr int Samples = Last - First + 1;

// Both curves on a 1 nm grid from First to Last, each peaking at 1.
struct Spectrum {
    QVector<double> excitation;
    QVector<double> emission;

    double excitationAt(double nm) const;
};

Spectrum spectrum(const Dye &dye);
double transmission(const Filter &filter, double nm);
// The share of a dye's emission a filter passes, from 0 to 1.
double captured(const Spectrum &spectrum, const Filter &filter);
// What a detector sees of a dye: brightness, times how well its laser
// excites it, times the share of emission its filter passes.
double signal(const Dye &dye, const Spectrum &spectrum, const Channel &channel);

QColor wavelengthColor(double nm);

const QList<Dye> &library();
const Dye *findInLibrary(const QString &name);

}  // namespace spectra
