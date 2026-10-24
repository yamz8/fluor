#include "spectraplot.h"

#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#include "backend.h"

namespace {

// The window shows the part of the spectrum cytometry and microscopy use.
constexpr double ShownFirst = 340;
constexpr double ShownLast = 850;

const QColor Text(0xc8, 0xc8, 0xd0);
const QColor Faint(0x8a, 0x8a, 0x94);

QColor withAlpha(QColor color, double alpha) {
    color.setAlphaF(std::clamp(alpha, 0.0, 1.0));
    return color;
}

}  // namespace

SpectraPlot::SpectraPlot(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
}

void SpectraPlot::setBackend(Backend *backend) {
    if (backend == m_backend)
        return;
    if (m_backend)
        disconnect(m_backend, nullptr, this, nullptr);
    m_backend = backend;
    if (m_backend)
        connect(m_backend, &Backend::changed, this, [this] { update(); });
    emit backendChanged();
    update();
}

#define SETTER(name, member, type, signal) \
    void SpectraPlot::name(type value) {   \
        if (member == value)               \
            return;                        \
        member = value;                    \
        emit signal();                     \
        update();                          \
    }
SETTER(setLaser, m_laser, int, laserChanged)
SETTER(setSelected, m_selected, int, selectedChanged)
SETTER(setExcitation, m_excitation, bool, excitationChanged)
SETTER(setHoverX, m_hoverX, double, hoverXChanged)
#undef SETTER

void SpectraPlot::setEmptyText(const QString &text) {
    if (text == m_emptyText)
        return;
    m_emptyText = text;
    emit emptyTextChanged();
    update();
}

QRectF SpectraPlot::plotArea() const {
    // Room above for laser and filter labels, below for the wavelength axis.
    return QRectF(12, 34, std::max(10.0, width() - 24), std::max(10.0, height() - 34 - 28));
}

double SpectraPlot::xFor(double nm) const {
    const QRectF area = plotArea();
    const double t = (nm - ShownFirst) / (ShownLast - ShownFirst);
    // Right-to-left layouts keep wavelength increasing to the right, as
    // every spectrum chart does.
    return area.left() + t * area.width();
}

double SpectraPlot::nmFor(double x) const {
    const QRectF area = plotArea();
    return ShownFirst + (x - area.left()) / area.width() * (ShownLast - ShownFirst);
}

double SpectraPlot::emissionScale(int d) const {
    const auto &panel = m_backend->panel();
    if (m_laser < 0 || m_laser >= panel.lasers.size())
        return 1;
    double brightest = 0;
    for (const auto &entry : panel.dyes)
        brightest = std::max(brightest, entry.dye.brightness);
    const double nm = panel.lasers[m_laser].nm;
    return brightest > 0 ? panel.dyes[d].dye.brightness * m_backend->curves()[d].excitationAt(nm) / brightest : 0;
}

int SpectraPlot::dyeAt(double x, double y) const {
    if (!m_backend)
        return -1;
    const QRectF area = plotArea();
    const int i = int(std::round(nmFor(x))) - spectra::First;
    if (i < 0 || i >= spectra::Samples || y > area.bottom() + 4)
        return -1;
    int found = -1;
    double closest = 1e9;
    for (int d = 0; d < m_backend->curves().size(); ++d) {
        const double top = area.bottom() - m_backend->curves()[d].emission[i] * emissionScale(d) * area.height();
        if (y + 6 >= top && y - top < closest && area.bottom() - top > 4) {
            closest = std::abs(y - top);
            found = d;
        }
    }
    return found;
}

void SpectraPlot::paint(QPainter *painter) {
    if (!m_backend)
        return;
    const auto &panel = m_backend->panel();
    const auto &analysis = m_backend->analysis();
    const auto &curves = m_backend->curves();
    const QRectF area = plotArea();
    const QLocale locale;
    painter->setRenderHint(QPainter::Antialiasing);
    QFont small = painter->font();
    small.setPixelSize(11);
    QFont bold = small;
    bold.setBold(true);
    painter->setFont(small);
    const QFontMetricsF metrics(small);

    // Wavelength grid.
    const double step = area.width() < 520 ? 100 : 50;
    for (double nm = std::ceil(ShownFirst / step) * step; nm <= ShownLast; nm += step) {
        const double x = xFor(nm);
        painter->setPen(QPen(QColor(255, 255, 255, 14), 1));
        painter->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        painter->setPen(Faint);
        const QString label = locale.toString(int(nm));
        painter->drawText(QPointF(x - metrics.horizontalAdvance(label) / 2, area.bottom() + 18), label);
    }
    painter->setPen(QPen(QColor(255, 255, 255, 40), 1));
    painter->drawLine(area.bottomLeft(), area.bottomRight());

    const int selectedChannel = m_selected >= 0 && m_selected < analysis.assigned.size() ? analysis.assigned[m_selected] : -1;

    // Filter bands: the chosen laser's, or every one when none is chosen.
    for (int c = 0; c < analysis.channels.size(); ++c) {
        const Channel &channel = analysis.channels[c];
        const bool shown = m_laser < 0 || channel.laser == m_laser;
        const bool highlighted = c == selectedChannel;
        if (!shown && !highlighted)
            continue;
        const double left = xFor(channel.band.center - channel.band.width / 2.0);
        const double right = xFor(channel.band.center + channel.band.width / 2.0);
        const QColor color = spectra::wavelengthColor(channel.band.center);
        const QRectF band(left, area.top(), right - left, area.height());
        painter->fillRect(band, withAlpha(color, highlighted ? 0.20 : m_laser < 0 ? 0.06 : 0.12));
        if (highlighted) {
            painter->setPen(QPen(withAlpha(color, 0.7), 1, Qt::DashLine));
            painter->drawLine(band.topLeft(), band.bottomLeft());
            painter->drawLine(band.topRight(), band.bottomRight());
        }
        if (m_laser >= 0 || highlighted) {
            const QString label = channel.band.name();
            painter->setPen(withAlpha(color, highlighted ? 1 : 0.8));
            painter->drawText(QPointF((left + right) / 2 - metrics.horizontalAdvance(label) / 2, area.top() - 4), label);
        }
    }

    // Laser lines.
    for (int l = 0; l < panel.lasers.size(); ++l) {
        const double x = xFor(panel.lasers[l].nm);
        const bool active = m_laser < 0 || m_laser == l;
        const QColor color = spectra::wavelengthColor(panel.lasers[l].nm);
        painter->setPen(QPen(withAlpha(color, active ? 0.95 : 0.3), active ? 2 : 1));
        painter->drawLine(QPointF(x, area.top() - 2), QPointF(x, area.bottom()));
        painter->setFont(bold);
        const QString label = locale.toString(panel.lasers[l].nm);
        painter->setPen(withAlpha(color, active ? 1 : 0.4));
        painter->drawText(QPointF(x - QFontMetricsF(bold).horizontalAdvance(label) / 2, area.top() - 18), label);
        painter->setFont(small);
    }

    // Dye curves, the selected one last so it sits on top.
    QList<int> order;
    for (int d = 0; d < curves.size(); ++d)
        if (d != m_selected)
            order << d;
    if (m_selected >= 0 && m_selected < curves.size())
        order << m_selected;
    const int first = int(ShownFirst) - spectra::First, last = int(ShownLast) - spectra::First;
    struct NameTag { int dye; QPointF at; };
    QList<NameTag> tags;
    for (const int d : order) {
        const auto &curve = curves[d];
        const QColor color = spectra::wavelengthColor(panel.dyes[d].dye.em);
        const bool focus = d == m_selected;
        const bool dimmed = m_selected >= 0 && !focus;
        const double scale = emissionScale(d);

        // Excitation is drawn for the selected dye, and for all with excitation on.
        if (m_excitation || focus) {
            QPainterPath excitation;
            for (int i = first; i <= last; ++i) {
                const QPointF point(xFor(spectra::First + i), area.bottom() - curve.excitation[i] * area.height());
                i == first ? excitation.moveTo(point) : excitation.lineTo(point);
            }
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(withAlpha(color, dimmed ? 0.25 : 0.7), focus ? 1.6 : 1.1, Qt::DashLine));
            painter->drawPath(excitation);
        }

        QPainterPath emission(QPointF(xFor(ShownFirst), area.bottom()));
        double peakY = area.bottom(), peakX = 0;
        for (int i = first; i <= last; ++i) {
            const QPointF point(xFor(spectra::First + i), area.bottom() - curve.emission[i] * scale * area.height());
            emission.lineTo(point);
            if (point.y() < peakY) {
                peakY = point.y();
                peakX = point.x();
            }
        }
        emission.lineTo(QPointF(xFor(ShownLast), area.bottom()));
        emission.closeSubpath();
        painter->setBrush(withAlpha(color, focus ? 0.38 : dimmed ? 0.07 : 0.2));
        painter->setPen(QPen(withAlpha(color, dimmed ? 0.35 : 1), focus ? 2.4 : 1.5));
        painter->drawPath(emission);

        if (scale > 0.04 && peakX > 0)
            tags << NameTag{d, QPointF(peakX, peakY)};
    }

    // Names sit over their emission peaks, the selected dye's first; one that
    // would overlap a name already placed is left out rather than smeared.
    std::stable_sort(tags.begin(), tags.end(), [this](const NameTag &x, const NameTag &y) {
        return (x.dye == m_selected) > (y.dye == m_selected);
    });
    QList<QRectF> placed;
    for (const auto &tag : tags) {
        const bool focus = tag.dye == m_selected;
        painter->setFont(focus ? bold : small);
        const QString name = panel.dyes[tag.dye].dye.name;
        const QFontMetricsF fm(painter->font());
        const double w = fm.horizontalAdvance(name);
        const double x = std::clamp(tag.at.x() - w / 2, area.left(), area.right() - w);
        const double baseline = std::max(area.top() + 12, tag.at.y() - 6);
        const QRectF box(x - 3, baseline - fm.ascent(), w + 6, fm.height());
        if (std::any_of(placed.cbegin(), placed.cend(), [&](const QRectF &r) { return r.intersects(box); }))
            continue;
        placed << box;
        const bool dimmed = m_selected >= 0 && !focus;
        painter->setPen(withAlpha(spectra::wavelengthColor(panel.dyes[tag.dye].dye.em), dimmed ? 0.45 : 1));
        painter->drawText(QPointF(x, baseline), name);
    }
    painter->setFont(small);

    // The wavelength under the pointer.
    if (m_hoverX >= area.left() && m_hoverX <= area.right()) {
        const double nm = nmFor(m_hoverX);
        painter->setPen(QPen(QColor(255, 255, 255, 70), 1));
        painter->drawLine(QPointF(m_hoverX, area.top()), QPointF(m_hoverX, area.bottom()));
        const QString label = QString("%1 nm").arg(locale.toString(int(std::round(nm))));
        const double w = metrics.horizontalAdvance(label) + 10;
        const QRectF tag(std::clamp(m_hoverX - w / 2, area.left(), area.right() - w), area.bottom() + 4, w, 18);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0x26, 0x26, 0x2c));
        painter->drawRect(tag);
        painter->setPen(Text);
        painter->drawText(tag, Qt::AlignCenter, label);
    }

    if (curves.isEmpty() && !m_emptyText.isEmpty()) {
        QFont hint = painter->font();
        hint.setPixelSize(15);
        painter->setFont(hint);
        painter->setPen(Faint);
        painter->drawText(area, Qt::AlignCenter, m_emptyText);
    }
}
