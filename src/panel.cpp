#include "panel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

QList<Channel> Panel::channels() const {
    QList<Channel> result;
    for (int l = 0; l < lasers.size(); ++l)
        for (int f = 0; f < lasers[l].filters.size(); ++f)
            result << Channel{l, f, lasers[l].nm, lasers[l].filters[f]};
    return result;
}

int Panel::channelOf(int dye) const {
    const PanelDye &entry = dyes[dye];
    if (!entry.assigned())
        return -1;
    const auto all = channels();
    for (int c = 0; c < all.size(); ++c)
        if (all[c].nm == entry.laser && all[c].band == entry.band)
            return c;
    return -1;
}

void Panel::assign(int dye, const Channel &channel) {
    dyes[dye].laser = channel.nm;
    dyes[dye].band = channel.band;
}

void Panel::unassign(int dye) {
    dyes[dye].laser = 0;
    dyes[dye].band = {};
}

int Panel::indexOf(const QString &name) const {
    for (int i = 0; i < dyes.size(); ++i)
        if (dyes[i].dye.name.compare(name, Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

QJsonObject Panel::toJson() const {
    QJsonArray laserList;
    for (const auto &laser : lasers) {
        QJsonArray filters;
        for (const auto &filter : laser.filters)
            filters << filter.name();
        laserList << QJsonObject{{"nm", laser.nm}, {"filters", filters}};
    }
    QJsonArray dyeList;
    for (const auto &entry : dyes) {
        QJsonObject object{{"name", entry.dye.name}};
        if (entry.custom) {
            object["ex"] = entry.dye.ex;
            object["em"] = entry.dye.em;
            object["brightness"] = entry.dye.brightness;
        }
        if (entry.assigned()) {
            object["laser"] = entry.laser;
            object["filter"] = entry.band.name();
        }
        dyeList << object;
    }
    return {{"fluor", 1}, {"title", title}, {"lasers", laserList}, {"dyes", dyeList}};
}

namespace {

bool parseFilter(const QString &text, Filter *filter) {
    static const QRegularExpression pattern(R"(^\s*(\d{3})\s*/\s*(\d{1,3})\s*$)");
    const auto match = pattern.match(text);
    if (!match.hasMatch())
        return false;
    *filter = {match.captured(1).toInt(), match.captured(2).toInt()};
    return filter->width > 0;
}

Panel failWith(QString *error, const QString &message) {
    if (error)
        *error = message;
    return {};
}

}  // namespace

Panel Panel::fromJson(const QJsonObject &object, QString *error) {
    if (!object.contains("lasers") || !object.value("lasers").isArray())
        return failWith(error, "Not a Fluor panel: it has no \"lasers\" list.");
    Panel panel;
    panel.title = object.value("title").toString();
    for (const auto &value : object.value("lasers").toArray()) {
        const QJsonObject item = value.toObject();
        Laser laser{item.value("nm").toInt(), {}};
        if (laser.nm < spectra::First || laser.nm > spectra::Last)
            return failWith(error, QString("Laser %1 nm is outside %2 to %3 nm.")
                                       .arg(item.value("nm").toVariant().toString())
                                       .arg(spectra::First).arg(spectra::Last));
        for (const auto &text : item.value("filters").toArray()) {
            Filter filter;
            if (!parseFilter(text.toString(), &filter))
                return failWith(error, QString("Filter \"%1\" on the %2 nm laser is not center/width, as 530/30.")
                                           .arg(text.toString()).arg(laser.nm));
            laser.filters << filter;
        }
        panel.lasers << laser;
    }
    for (const auto &value : object.value("dyes").toArray()) {
        const QJsonObject item = value.toObject();
        const QString name = item.value("name").toString().trimmed();
        if (name.isEmpty())
            return failWith(error, "A dye has no name.");
        PanelDye entry;
        if (item.contains("ex") || item.contains("em")) {
            entry.custom = true;
            entry.dye = Dye{name, item.value("ex").toDouble(), item.value("em").toDouble(),
                            item.value("brightness").toDouble(3)};
            if (entry.dye.ex < spectra::First || entry.dye.em <= entry.dye.ex || entry.dye.em > spectra::Last)
                return failWith(error, QString("Dye %1 needs an excitation peak (ex) below its emission peak (em), "
                                               "both between %2 and %3 nm.")
                                           .arg(name).arg(spectra::First).arg(spectra::Last));
        } else if (const Dye *known = spectra::findInLibrary(name)) {
            entry.dye = *known;
        } else {
            return failWith(error, QString("Unknown dye %1. List the known ones with fluor dyes, "
                                           "or give it \"ex\" and \"em\" peaks.").arg(name));
        }
        if (panel.indexOf(entry.dye.name) >= 0)
            return failWith(error, QString("Dye %1 is in the panel twice.").arg(entry.dye.name));
        if (item.contains("laser") || item.contains("filter")) {
            if (!parseFilter(item.value("filter").toString(), &entry.band))
                return failWith(error, QString("Dye %1's filter is not center/width, as 530/30.").arg(name));
            entry.laser = item.value("laser").toInt();
        }
        panel.dyes << entry;
    }
    return panel;
}

bool Panel::save(const QString &path, QString *error) const {
    // QSaveFile writes beside the target and renames over it, so a failed
    // save never leaves half a panel behind.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(toJson()).toJson()) < 0 || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

Panel Panel::load(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return failWith(error, file.errorString());
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError)
        return failWith(error, QString("Not valid JSON: %1 at byte %2.").arg(parse.errorString()).arg(parse.offset));
    return fromJson(document.object(), error);
}

const QList<Preset> &presets() {
    static const QList<Preset> list = [] {
        auto filters = [](std::initializer_list<std::pair<int, int>> bands) {
            QList<Filter> result;
            for (const auto &[center, width] : bands)
                result << Filter{center, width};
            return result;
        };
        const Laser violet{405, filters({{450, 50}, {525, 50}, {610, 20}, {660, 20}, {710, 50}, {780, 60}})};
        const Laser red{640, filters({{670, 30}, {730, 45}, {780, 60}})};
        return QList<Preset>{
            {"vbr", "Violet, blue, red analyzer",
             {violet, {488, filters({{530, 30}, {575, 26}, {610, 20}, {695, 40}, {780, 60}})}, red}},
            {"vbyr", "Violet, blue, yellow-green, red analyzer",
             {violet,
              {488, filters({{530, 30}, {695, 40}})},
              {561, filters({{586, 15}, {610, 20}, {670, 14}, {710, 50}, {780, 60}})},
              red}},
            {"uvbyr", "Five-laser analyzer",
             {{355, filters({{379, 28}, {450, 50}, {515, 30}, {740, 35}, {810, 40}})},
              violet,
              {488, filters({{530, 30}, {695, 40}})},
              {561, filters({{586, 15}, {610, 20}, {670, 14}, {710, 50}, {780, 60}})},
              red}},
            {"confocal", "Confocal microscope",
             {{405, filters({{450, 50}})},
              {488, filters({{525, 50}})},
              {561, filters({{600, 50}})},
              {640, filters({{690, 50}})}}},
        };
    }();
    return list;
}

const Preset *findPreset(const QString &id) {
    for (const auto &preset : presets())
        if (preset.id == id)
            return &preset;
    return nullptr;
}

Panel emptyPanel(const QString &preset) {
    const Preset *found = findPreset(preset);
    Panel panel;
    panel.lasers = (found ? found : &presets().first())->lasers;
    return panel;
}

namespace {

// How bad a placement is: every dye's spill into every other dye's detector,
// squared so one bad overlap outweighs several small ones, plus a smaller
// charge for each dye's signal lost against its best detector.
double placementCost(const QVector<QVector<double>> &signal, const QVector<double> &best, const QVector<int> &seat) {
    double cost = 0;
    for (int a = 0; a < seat.size(); ++a) {
        if (seat[a] < 0)
            continue;
        const double own = signal[a][seat[a]];
        if (own <= 0)
            return 1e9;
        cost += 0.5 * (1 - own / best[a]);
        for (int b = 0; b < seat.size(); ++b)
            if (b != a && seat[b] >= 0) {
                const double spill = signal[a][seat[b]] / own;
                cost += spill * spill;
            }
    }
    return cost;
}

}  // namespace

void autoAssign(Panel &panel, bool reassignAll, int only) {
    const auto channels = panel.channels();
    const int n = panel.dyes.size();
    QVector<bool> taken(channels.size(), false), movable(n, false);
    QVector<int> seat(n, -1);
    QList<int> waiting;
    for (int d = 0; d < n; ++d) {
        const int channel = reassignAll ? -1 : panel.channelOf(d);
        if (channel >= 0 && !taken[channel]) {
            taken[channel] = true;
            seat[d] = channel;
        } else if (only < 0 || d == only) {
            waiting << d;
            movable[d] = true;
        }
    }
    QVector<QVector<double>> signal(n);
    QVector<double> best(n, 0);
    for (int d = 0; d < n; ++d) {
        const auto spectrum = spectra::spectrum(panel.dyes[d].dye);
        for (const auto &channel : channels)
            signal[d] << spectra::signal(panel.dyes[d].dye, spectrum, channel);
        if (!signal[d].isEmpty())
            best[d] = *std::max_element(signal[d].cbegin(), signal[d].cend());
    }

    // Each round places the dye that would lose the most by not getting its
    // best free detector, so a dye with one good option isn't crowded out by
    // one that has several.
    while (!waiting.isEmpty()) {
        int pick = -1, pickChannel = -1;
        double pickRegret = -1;
        for (const int d : waiting) {
            double first = 0, second = 0;
            int firstChannel = -1;
            for (int c = 0; c < channels.size(); ++c) {
                if (taken[c])
                    continue;
                const double s = signal[d][c];
                if (s < MinSignal)
                    continue;
                if (s > first) {
                    second = first;
                    first = s;
                    firstChannel = c;
                } else if (s > second) {
                    second = s;
                }
            }
            if (firstChannel < 0)
                continue;
            const double regret = (first - second) / first;
            if (regret > pickRegret) {
                pick = d;
                pickChannel = firstChannel;
                pickRegret = regret;
            }
        }
        if (pick < 0)
            break;  // No free detector sees any of the remaining dyes.
        seat[pick] = pickChannel;
        taken[pickChannel] = true;
        waiting.removeOne(pick);
    }

    // The brightest detector isn't always the best seat: one with a little
    // less signal and far less overlap usually is. Move and swap the dyes
    // being placed while that lowers the overlap, never leaving a dye below
    // a quarter of its best signal.
    auto usable = [&](int d, int c) { return c >= 0 && signal[d][c] >= 0.25 * best[d]; };
    double cost = placementCost(signal, best, seat);
    for (int round = 0; round < 200; ++round) {
        QVector<int> bestSeat;
        double bestCost = cost - 1e-6;
        for (int d = 0; d < n; ++d) {
            if (!movable[d] || seat[d] < 0)
                continue;
            for (int c = 0; c < channels.size(); ++c) {
                if (taken[c] || !usable(d, c))
                    continue;
                QVector<int> trial = seat;
                trial[d] = c;
                const double trialCost = placementCost(signal, best, trial);
                if (trialCost < bestCost) {
                    bestCost = trialCost;
                    bestSeat = trial;
                }
            }
            for (int e = d + 1; e < n; ++e) {
                if (!movable[e] || seat[e] < 0 || !usable(d, seat[e]) || !usable(e, seat[d]))
                    continue;
                QVector<int> trial = seat;
                std::swap(trial[d], trial[e]);
                const double trialCost = placementCost(signal, best, trial);
                if (trialCost < bestCost) {
                    bestCost = trialCost;
                    bestSeat = trial;
                }
            }
        }
        if (bestSeat.isEmpty())
            break;
        seat = bestSeat;
        cost = bestCost;
        taken.fill(false);
        for (const int c : seat)
            if (c >= 0)
                taken[c] = true;
    }

    for (int d = 0; d < n; ++d) {
        if (!movable[d])
            continue;
        if (seat[d] >= 0)
            panel.assign(d, channels[seat[d]]);
        else
            panel.unassign(d);
    }
}

int Analysis::errors() const {
    return int(std::count_if(issues.cbegin(), issues.cend(), [](const Issue &issue) {
        return issue.severity == Issue::Error;
    }));
}

Analysis analyze(const Panel &panel) {
    Analysis result;
    result.channels = panel.channels();
    const int n = panel.dyes.size(), channels = result.channels.size();
    result.assigned.resize(n);
    result.signal.resize(n);
    QVector<spectra::Spectrum> curves;
    for (int d = 0; d < n; ++d) {
        result.assigned[d] = panel.channelOf(d);
        curves << spectra::spectrum(panel.dyes[d].dye);
        for (const auto &channel : result.channels)
            result.signal[d] << spectra::signal(panel.dyes[d].dye, curves[d], channel);
    }

    result.spill.fill(QVector<double>(n, -1), n);
    result.similarity.fill(QVector<double>(n, 0), n);
    for (int a = 0; a < n; ++a) {
        for (int b = 0; b < n; ++b) {
            double dot = 0, na = 0, nb = 0;
            for (int c = 0; c < channels; ++c) {
                dot += result.signal[a][c] * result.signal[b][c];
                na += result.signal[a][c] * result.signal[a][c];
                nb += result.signal[b][c] * result.signal[b][c];
            }
            result.similarity[a][b] = na > 0 && nb > 0 ? dot / std::sqrt(na * nb) : 0;

            const int own = result.assigned[a], other = result.assigned[b];
            if (own < 0 || other < 0 || result.signal[a][own] < MinSignal)
                continue;
            result.spill[a][b] = result.signal[a][other] / result.signal[a][own];
            if (a != b)
                result.worstSpill = std::max(result.worstSpill, result.spill[a][b]);
        }
    }

    auto add = [&](Issue::Kind kind, Issue::Severity severity, int a, int b, double value) {
        result.issues << Issue{kind, severity, a, b, value};
    };
    for (int a = 0; a < n; ++a) {
        const int own = result.assigned[a];
        if (own < 0) {
            const bool seen = std::any_of(result.signal[a].cbegin(), result.signal[a].cend(),
                                          [](double s) { return s >= MinSignal; });
            add(seen ? Issue::Unassigned : Issue::Unseen, Issue::Error, a, -1, 0);
            continue;
        }
        const double excitation = curves[a].excitationAt(result.channels[own].nm);
        if (excitation < DimExcitation)
            add(Issue::Dim, Issue::Warning, a, -1, excitation);
        for (int b = 0; b < n; ++b) {
            if (b == a || result.assigned[b] < 0)
                continue;
            if (result.assigned[b] == own) {
                if (a < b)
                    add(Issue::Shared, Issue::Error, a, b, 1);
                continue;
            }
            const double spill = result.spill[a][b];
            if (spill >= HighSpill)
                add(Issue::Spill, Issue::Warning, a, b, spill);
            else if (spill >= ModerateSpill)
                add(Issue::Spill, Issue::Note, a, b, spill);
            if (a < b && result.similarity[a][b] >= HighSimilarity)
                add(Issue::Similar, Issue::Warning, a, b, result.similarity[a][b]);
        }
    }
    // Errors first, then the worst overlaps.
    std::stable_sort(result.issues.begin(), result.issues.end(), [](const Issue &x, const Issue &y) {
        // A dim dye is worse the less it is excited; everything else the higher its value.
        auto weight = [](const Issue &issue) { return issue.kind == Issue::Dim ? 1 - issue.value : issue.value; };
        if (x.severity != y.severity)
            return x.severity < y.severity;
        return weight(x) > weight(y);
    });
    return result;
}
