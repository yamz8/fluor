#include "spectra.h"

#include <algorithm>
#include <cmath>

namespace spectra {

namespace {

// A band that rises slowly on one side and falls fast on the other, which is
// how absorption and emission bands look in wavelength: excitation trails off
// toward the blue, emission toward the red.
double band(double nm, double peak, double left, double right) {
    const double sigma = nm < peak ? left : right;
    const double z = (nm - peak) / sigma;
    return std::exp(-0.5 * z * z);
}

void normalize(QVector<double> &curve) {
    const double peak = *std::max_element(curve.cbegin(), curve.cend());
    if (peak > 0)
        for (double &value : curve)
            value /= peak;
}

}  // namespace

double Spectrum::excitationAt(double nm) const {
    const double position = nm - First;
    if (position < 0 || position > Samples - 1)
        return 0;
    const int below = int(std::floor(position));
    const int above = std::min(below + 1, Samples - 1);
    const double t = position - below;
    return excitation[below] * (1 - t) + excitation[above] * t;
}

Spectrum spectrum(const Dye &dye) {
    Spectrum result;
    result.excitation.resize(Samples);
    result.emission.resize(Samples);
    for (int i = 0; i < Samples; ++i) {
        const double nm = First + i;
        double ex = band(nm, dye.ex, 0.085 * dye.ex, 0.03 * dye.ex);
        if (dye.ex2 > 0)
            ex += dye.ex2Amount * band(nm, dye.ex2, 0.05 * dye.ex2, 0.03 * dye.ex2);
        double em = band(nm, dye.em, 0.02 * dye.em, 0.045 * dye.em);
        if (dye.em2 > 0)
            em += dye.em2Amount * band(nm, dye.em2, 0.02 * dye.em2, 0.045 * dye.em2);
        result.excitation[i] = ex;
        result.emission[i] = em;
    }
    normalize(result.excitation);
    normalize(result.emission);
    return result;
}

// A hard-coated bandpass: about 92% inside the band, with 3 nm edges.
double transmission(const Filter &filter, double nm) {
    const double half = filter.width / 2.0;
    const double distance = std::abs(nm - filter.center);
    constexpr double edge = 3, peak = 0.92;
    if (distance <= half - edge / 2)
        return peak;
    if (distance >= half + edge / 2)
        return 0;
    return peak * (half + edge / 2 - distance) / edge;
}

double captured(const Spectrum &spectrum, const Filter &filter) {
    double total = 0, passed = 0;
    for (int i = 0; i < Samples; ++i) {
        total += spectrum.emission[i];
        passed += spectrum.emission[i] * transmission(filter, First + i);
    }
    return total > 0 ? passed / total : 0;
}

double signal(const Dye &dye, const Spectrum &spectrum, const Channel &channel) {
    return dye.brightness * spectrum.excitationAt(channel.nm) * captured(spectrum, channel.band);
}

// Visible wavelengths after Dan Bruton's approximation, lifted a little so
// they read on a dark window. Ultraviolet stays a pale violet and the near
// infrared turns toward a deep rose, so neither disappears into the dark.
QColor wavelengthColor(double nm) {
    double r = 0, g = 0, b = 0;
    if (nm < 380) {
        r = 0.55; g = 0.4; b = 1;
    } else if (nm < 440) {
        r = 0.55 * (440 - nm) / 60; b = 1;
    } else if (nm < 490) {
        g = (nm - 440) / 50; b = 1;
    } else if (nm < 510) {
        g = 1; b = (510 - nm) / 20;
    } else if (nm < 580) {
        r = (nm - 510) / 70; g = 1;
    } else if (nm < 645) {
        r = 1; g = (645 - nm) / 65;
    } else if (nm < 700) {
        r = 1;
    } else {
        r = 1; g = 0.15; b = std::min(0.6, (nm - 700) / 150 * 0.6);
    }
    auto lift = [](double v) { return int(std::round(255 * (0.18 + 0.82 * v))); };
    return QColor(lift(r), lift(g), lift(b));
}

// Peaks and relative brightness gathered from vendors' published spectra,
// rounded. PE and its tandems absorb at both 496 and 565 nm, and tandems keep
// a little of their donor's emission.
const QList<Dye> &library() {
    static const QList<Dye> dyes = [] {
        auto dye = [](const char *name, double ex, double em, double brightness) {
            return Dye{name, ex, em, brightness};
        };
        auto tandem = [](const char *name, double ex, double em, double brightness, double donor, double leak) {
            return Dye{name, ex, em, brightness, 0, 0, donor, leak};
        };
        auto pe = [](const char *name, double em, double brightness, double leak) {
            return Dye{name, 565, em, brightness, 496, 0.75, leak > 0 ? 578.0 : 0.0, leak};
        };
        return QList<Dye>{
            // Ultraviolet
            dye("BUV395", 348, 395, 3),
            tandem("BUV496", 348, 496, 3, 395, 0.05),
            tandem("BUV563", 348, 563, 3, 395, 0.05),
            tandem("BUV615", 348, 615, 3, 395, 0.05),
            tandem("BUV661", 348, 661, 3, 395, 0.05),
            tandem("BUV737", 348, 737, 3, 395, 0.05),
            tandem("BUV805", 348, 805, 2, 395, 0.05),
            dye("DAPI", 358, 461, 2),
            dye("Hoechst 33342", 350, 461, 2),
            dye("Alexa Fluor 350", 346, 442, 1),
            dye("EBFP2", 383, 448, 1),
            // Violet
            dye("BV421", 407, 421, 5),
            dye("Super Bright 436", 414, 436, 4),
            dye("Alexa Fluor 405", 401, 421, 2),
            dye("Pacific Blue", 401, 452, 2),
            dye("eFluor 450", 405, 450, 2),
            dye("BV480", 436, 478, 3),
            dye("ECFP", 434, 477, 1),
            tandem("BV510", 405, 510, 3, 421, 0.05),
            dye("Zombie Aqua", 405, 516, 2),
            dye("Pacific Orange", 400, 551, 1),
            tandem("BV570", 407, 570, 2, 421, 0.05),
            tandem("BV605", 407, 603, 4, 421, 0.05),
            tandem("BV650", 407, 645, 4, 421, 0.05),
            tandem("BV711", 407, 711, 4, 421, 0.05),
            tandem("BV750", 407, 750, 3, 421, 0.05),
            tandem("BV786", 407, 786, 3, 421, 0.05),
            // Blue
            dye("FITC", 495, 519, 2),
            dye("Alexa Fluor 488", 495, 519, 3),
            dye("BB515", 490, 515, 4),
            dye("EGFP", 488, 507, 2),
            dye("mNeonGreen", 506, 517, 3),
            dye("CFSE", 492, 517, 3),
            dye("SYTOX Green", 504, 523, 3),
            dye("EYFP", 513, 527, 2),
            dye("PerCP", 482, 678, 2),
            tandem("PerCP-Cy5.5", 482, 695, 3, 678, 0.1),
            dye("PerCP-eFluor 710", 482, 710, 3),
            dye("BB700", 485, 695, 4),
            // Yellow-green
            pe("PE", 578, 5, 0),
            pe("PE-CF594", 612, 4, 0.06),
            pe("PE-Dazzle 594", 610, 4, 0.06),
            pe("PE-Texas Red", 615, 3, 0.06),
            pe("PE-Cy5", 667, 4, 0.05),
            pe("PE-Cy5.5", 695, 3, 0.05),
            pe("PE-Cy7", 785, 3, 0.05),
            dye("Propidium Iodide", 535, 617, 2),
            dye("7-AAD", 546, 647, 1),
            dye("tdTomato", 554, 581, 4),
            dye("DsRed", 558, 583, 2),
            dye("mScarlet", 569, 594, 3),
            dye("mCherry", 587, 610, 1),
            dye("Cy3", 550, 570, 3),
            dye("TRITC", 557, 576, 2),
            dye("Alexa Fluor 532", 532, 554, 3),
            dye("Alexa Fluor 546", 556, 573, 3),
            dye("Alexa Fluor 555", 555, 565, 3),
            dye("Alexa Fluor 568", 578, 603, 3),
            dye("Alexa Fluor 594", 590, 617, 3),
            dye("Texas Red", 595, 615, 2),
            // Red
            dye("APC", 650, 660, 4),
            dye("Alexa Fluor 633", 632, 647, 3),
            dye("eFluor 660", 633, 658, 3),
            dye("Alexa Fluor 647", 650, 668, 3),
            dye("Cy5", 649, 666, 3),
            dye("iRFP670", 643, 670, 1),
            dye("Alexa Fluor 680", 679, 702, 3),
            dye("Cy5.5", 675, 694, 3),
            tandem("APC-R700", 652, 704, 3, 660, 0.08),
            dye("Alexa Fluor 700", 702, 723, 2),
            tandem("APC-Cy7", 650, 785, 3, 660, 0.06),
            tandem("APC-H7", 650, 785, 3, 660, 0.06),
            tandem("APC-Fire 750", 650, 787, 3, 660, 0.04),
            dye("Zombie NIR", 718, 746, 2),
            dye("Alexa Fluor 750", 749, 775, 2),
            dye("Cy7", 750, 773, 2),
        };
    }();
    return dyes;
}

const Dye *findInLibrary(const QString &name) {
    for (const auto &dye : library())
        if (dye.name.compare(name, Qt::CaseInsensitive) == 0)
            return &dye;
    return nullptr;
}

}  // namespace spectra
