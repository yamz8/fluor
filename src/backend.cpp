#include "backend.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>

#include "filepicker.h"

namespace {

constexpr int UndoLimit = 200;

QString hex(const QColor &color) { return color.name(QColor::HexRgb); }

QString compact(const QString &text) {
    static const QRegularExpression punctuation("[^A-Za-z0-9]");
    return QString(text).remove(punctuation).toLower();
}

bool isSubsequence(const QString &needle, const QString &haystack) {
    int at = 0;
    for (const QChar c : haystack)
        if (at < needle.size() && needle[at] == c)
            ++at;
    return at == needle.size();
}

QString localPath(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

}  // namespace

Backend::Backend(FilePicker *picker, QObject *parent)
    : QObject(parent), m_picker(picker), m_panel(emptyPanel()) {
    m_saved = m_panel.toJson();
    if (m_picker) {
        connect(m_picker, &FilePicker::openSelected, this, [this](const QUrl &url) { load(url.toString()); });
        connect(m_picker, &FilePicker::saveSelected, this, [this](const QUrl &url) {
            QString path = url.toLocalFile();
            if (QFileInfo(path).suffix().isEmpty())
                path += ".fluor";
            if (writeTo(path))
                emit saved();
        });
        connect(m_picker, &FilePicker::failed, this, &Backend::setError);
    }
    refresh();
}

QString Backend::fileName() const { return m_path.isEmpty() ? QString() : QFileInfo(m_path).fileName(); }

bool Backend::dirty() const { return m_panel.toJson() != m_saved; }

void Backend::refresh() {
    m_analysis = analyze(m_panel);
    m_curves.clear();
    for (const auto &entry : m_panel.dyes)
        m_curves << spectra::spectrum(entry.dye);
    emit changed();
}

void Backend::setError(const QString &message) {
    m_error = message;
    emit errorChanged();
}

void Backend::clearError() { setError({}); }

void Backend::edit(const std::function<void(Panel &)> &change, const QString &key) {
    const Panel before = m_panel;
    change(m_panel);
    if (m_panel.toJson() == before.toJson())
        return;
    const bool coalesce = !key.isEmpty() && key == m_lastKey && m_lastEdit.isValid() && m_lastEdit.elapsed() < 1500;
    if (!coalesce) {
        m_undo << before;
        if (m_undo.size() > UndoLimit)
            m_undo.removeFirst();
    }
    m_redo.clear();
    m_lastKey = key;
    m_lastEdit.restart();
    refresh();
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo << m_panel;
    m_panel = m_undo.takeLast();
    m_lastKey.clear();
    refresh();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo << m_panel;
    m_panel = m_redo.takeLast();
    m_lastKey.clear();
    refresh();
}

QVariantList Backend::dyes() const {
    QVariantList list;
    for (int d = 0; d < m_panel.dyes.size(); ++d) {
        const auto &entry = m_panel.dyes[d];
        const int channel = m_analysis.assigned[d];
        QVariantMap item{
            {"name", entry.dye.name},
            {"ex", entry.dye.ex},
            {"em", entry.dye.em},
            {"brightness", entry.dye.brightness},
            {"custom", entry.custom},
            {"color", hex(spectra::wavelengthColor(entry.dye.em))},
            {"channel", channel},
            {"channelName", channel >= 0 ? m_analysis.channels[channel].name() : QString()},
            {"channelColor", channel >= 0 ? hex(spectra::wavelengthColor(m_analysis.channels[channel].band.center))
                                          : QString()},
            {"excitation", channel >= 0 ? m_curves[d].excitationAt(m_analysis.channels[channel].nm) : 0.0},
            {"captured", channel >= 0 ? spectra::captured(m_curves[d], m_analysis.channels[channel].band) : 0.0},
        };
        list << item;
    }
    return list;
}

QVariantList Backend::lasers() const {
    QVariantList list;
    for (const auto &laser : m_panel.lasers) {
        QVariantList filters;
        for (const auto &filter : laser.filters)
            filters << QVariantMap{{"center", filter.center}, {"width", filter.width},
                                   {"name", filter.name()}, {"color", hex(spectra::wavelengthColor(filter.center))}};
        list << QVariantMap{{"nm", laser.nm}, {"color", hex(spectra::wavelengthColor(laser.nm))}, {"filters", filters}};
    }
    return list;
}

QVariantList Backend::channels() const {
    QVariantList list;
    for (const auto &channel : m_analysis.channels)
        list << QVariantMap{{"name", channel.name()}, {"nm", channel.nm}, {"laser", channel.laser},
                            {"filter", channel.filter}, {"band", channel.band.name()},
                            {"color", hex(spectra::wavelengthColor(channel.band.center))}};
    return list;
}

QVariantList Backend::spill() const {
    QVariantList rows;
    for (int a = 0; a < m_panel.dyes.size(); ++a) {
        QVariantList cells;
        for (int b = 0; b < m_panel.dyes.size(); ++b)
            cells << m_analysis.spill[a][b];
        rows << QVariantMap{{"name", m_panel.dyes[a].dye.name},
                            {"color", hex(spectra::wavelengthColor(m_panel.dyes[a].dye.em))},
                            {"cells", cells}};
    }
    return rows;
}

QVariantList Backend::issues() const {
    static const char *kinds[] = {"unassigned", "unseen", "shared", "dim", "spill", "similar", "blind"};
    static const char *severities[] = {"error", "warning", "note"};
    QVariantList list;
    for (const auto &issue : m_analysis.issues) {
        auto name = [this](int d) { return d >= 0 ? m_panel.dyes[d].dye.name : QString(); };
        auto channel = [this](int d) {
            const int c = d >= 0 ? m_analysis.assigned[d] : -1;
            return c >= 0 ? m_analysis.channels[c].name() : QString();
        };
        list << QVariantMap{{"kind", kinds[issue.kind]}, {"severity", severities[issue.severity]},
                            {"a", name(issue.a)}, {"b", name(issue.b)},
                            {"aIndex", issue.a}, {"bIndex", issue.b},
                            {"aChannel", channel(issue.a)}, {"bChannel", channel(issue.b)},
                            {"laser", issue.a >= 0 && m_analysis.assigned[issue.a] >= 0
                                          ? m_analysis.channels[m_analysis.assigned[issue.a]].nm : 0},
                            {"value", issue.value}};
    }
    return list;
}

QVariantList Backend::presets() const {
    QVariantList list;
    for (const auto &preset : ::presets()) {
        QStringList lasers;
        for (const auto &laser : preset.lasers)
            lasers << QString::number(laser.nm);
        list << QVariantMap{{"id", preset.id}, {"name", preset.name}, {"lasers", lasers.join(" · ")}};
    }
    return list;
}

QVariantList Backend::searchDyes(const QString &query) const {
    const QString text = query.trimmed();
    bool numeric = false;
    const int nm = text.toInt(&numeric);
    struct Hit { const Dye *dye; double rank; };
    QList<Hit> hits;
    for (const auto &dye : spectra::library()) {
        double rank = -1;
        if (text.isEmpty())
            rank = 0;
        else if (numeric)
            // A number finds dyes excited near that laser line first, then
            // those emitting near that band.
            rank = std::abs(dye.ex - nm) <= 30   ? std::abs(dye.ex - nm)
                   : std::abs(dye.em - nm) <= 15 ? 100 + std::abs(dye.em - nm)
                                                 : -1;
        else if (dye.name.startsWith(text, Qt::CaseInsensitive))
            rank = 0;
        else if (dye.name.contains(text, Qt::CaseInsensitive))
            rank = 1;
        else if (isSubsequence(compact(text), compact(dye.name)))
            // "af488" finds Alexa Fluor 488, "pecy7" finds PE-Cy7.
            rank = 2;
        if (rank >= 0)
            hits << Hit{&dye, rank};
    }
    std::stable_sort(hits.begin(), hits.end(), [&](const Hit &x, const Hit &y) {
        if (x.rank != y.rank)
            return x.rank < y.rank;
        return text.isEmpty() ? x.dye->ex < y.dye->ex : false;
    });
    QVariantList list;
    for (const auto &hit : hits)
        list << QVariantMap{{"name", hit.dye->name}, {"ex", hit.dye->ex}, {"em", hit.dye->em},
                            {"brightness", hit.dye->brightness},
                            {"color", hex(spectra::wavelengthColor(hit.dye->em))},
                            {"inPanel", m_panel.indexOf(hit.dye->name) >= 0}};
    return list;
}

QVariantList Backend::channelOptions(int dye) const {
    QVariantList list;
    if (dye < 0 || dye >= m_panel.dyes.size())
        return list;
    const auto &signal = m_analysis.signal[dye];
    const double best = signal.isEmpty() ? 0 : *std::max_element(signal.cbegin(), signal.cend());
    for (int c = 0; c < m_analysis.channels.size(); ++c) {
        QString occupant;
        for (int other = 0; other < m_panel.dyes.size(); ++other)
            if (other != dye && m_analysis.assigned[other] == c)
                occupant = m_panel.dyes[other].dye.name;
        list << QVariantMap{{"index", c}, {"name", m_analysis.channels[c].name()},
                            {"color", hex(spectra::wavelengthColor(m_analysis.channels[c].band.center))},
                            {"relative", best > 0 ? signal[c] / best : 0.0},
                            {"occupant", occupant},
                            {"current", m_analysis.assigned[dye] == c}};
    }
    return list;
}

bool Backend::addDye(const QString &name) {
    const Dye *dye = spectra::findInLibrary(name);
    if (!dye || m_panel.indexOf(dye->name) >= 0)
        return false;
    edit([dye](Panel &panel) {
        panel.dyes << PanelDye{*dye, false, 0, {}};
        // Only the new dye is placed; anything else left without a detector
        // stays that way until the user asks for an auto-assign.
        ::autoAssign(panel, false, {int(panel.dyes.size()) - 1});
    });
    return true;
}

void Backend::removeDye(int index) {
    if (index < 0 || index >= m_panel.dyes.size())
        return;
    edit([index](Panel &panel) { panel.dyes.removeAt(index); });
}

void Backend::setChannel(int dye, int channel) {
    if (dye < 0 || dye >= m_panel.dyes.size())
        return;
    const auto all = m_panel.channels();
    edit([&](Panel &panel) {
        if (channel >= 0 && channel < all.size())
            panel.assign(dye, all[channel]);
        else
            panel.unassign(dye);
    });
}

void Backend::stepChannel(int dye, int step) {
    if (dye < 0 || dye >= m_panel.dyes.size() || m_analysis.channels.isEmpty())
        return;
    const int count = m_analysis.channels.size();
    const int current = m_analysis.assigned[dye];
    const int next = current < 0 ? (step > 0 ? 0 : count - 1) : ((current + step) % count + count) % count;
    setChannel(dye, next);
}

void Backend::autoAssign() {
    edit([](Panel &panel) { ::autoAssign(panel, true); });
}

void Backend::setTitle(const QString &title) {
    edit([&](Panel &panel) { panel.title = title.trimmed(); }, "title");
}

void Backend::applyPreset(const QString &id) {
    const Preset *preset = findPreset(id);
    if (!preset)
        return;
    edit([preset](Panel &panel) {
        panel.lasers = preset->lasers;
        // Only dyes whose detector the new instrument lacks are placed again;
        // a dye deliberately left without one stays that way.
        QList<int> lost;
        for (int d = 0; d < panel.dyes.size(); ++d)
            if (panel.dyes[d].assigned() && panel.channelOf(d) < 0) {
                panel.unassign(d);
                lost << d;
            }
        if (!lost.isEmpty())
            ::autoAssign(panel, false, lost);
    });
}

void Backend::addLaser(int nm) {
    edit([nm](Panel &panel) {
        panel.lasers << Laser{std::clamp(nm, spectra::First, spectra::Last), {Filter{std::min(nm + 30, spectra::Last), 30}}};
    });
}

void Backend::removeLaser(int laser) {
    if (laser < 0 || laser >= m_panel.lasers.size())
        return;
    edit([laser](Panel &panel) {
        const int nm = panel.lasers[laser].nm;
        panel.lasers.removeAt(laser);
        for (int d = 0; d < panel.dyes.size(); ++d)
            if (panel.dyes[d].laser == nm && panel.channelOf(d) < 0)
                panel.unassign(d);
    });
}

// Retuning a laser or a filter carries its dyes along with it.
void Backend::setLaser(int laser, int nm) {
    if (laser < 0 || laser >= m_panel.lasers.size())
        return;
    nm = std::clamp(nm, spectra::First, spectra::Last);
    edit([&](Panel &panel) {
        const int old = panel.lasers[laser].nm;
        panel.lasers[laser].nm = nm;
        for (auto &entry : panel.dyes)
            if (entry.laser == old)
                entry.laser = nm;
    }, QString("laser:%1").arg(laser));
}

void Backend::addFilter(int laser) {
    if (laser < 0 || laser >= m_panel.lasers.size())
        return;
    edit([laser](Panel &panel) {
        auto &filters = panel.lasers[laser].filters;
        const int center = filters.isEmpty() ? panel.lasers[laser].nm + 30 : filters.last().center + 50;
        filters << Filter{std::min(center, spectra::Last - 20), 30};
    });
}

void Backend::removeFilter(int laser, int filter) {
    if (laser < 0 || laser >= m_panel.lasers.size() || filter < 0 || filter >= m_panel.lasers[laser].filters.size())
        return;
    edit([&](Panel &panel) {
        const Filter old = panel.lasers[laser].filters.takeAt(filter);
        for (int d = 0; d < panel.dyes.size(); ++d)
            if (panel.dyes[d].laser == panel.lasers[laser].nm && panel.dyes[d].band == old)
                panel.unassign(d);
    });
}

void Backend::setFilter(int laser, int filter, int center, int width) {
    if (laser < 0 || laser >= m_panel.lasers.size() || filter < 0 || filter >= m_panel.lasers[laser].filters.size())
        return;
    const Filter tuned{std::clamp(center, spectra::First, spectra::Last), std::clamp(width, 1, 300)};
    edit([&](Panel &panel) {
        Filter &band = panel.lasers[laser].filters[filter];
        for (auto &entry : panel.dyes)
            if (entry.laser == panel.lasers[laser].nm && entry.band == band)
                entry.band = tuned;
        band = tuned;
    }, QString("filter:%1:%2").arg(laser).arg(filter));
}

void Backend::newPanel() {
    // A new panel keeps the instrument: the next panel is usually for the same cytometer.
    Panel fresh;
    fresh.lasers = m_panel.lasers;
    m_panel = fresh;
    m_path.clear();
    m_saved = m_panel.toJson();
    m_undo.clear();
    m_redo.clear();
    m_lastKey.clear();
    clearError();
    refresh();
}

bool Backend::load(const QString &pathOrUrl) {
    const QString path = localPath(pathOrUrl);
    QString message;
    Panel loaded = Panel::load(path, &message);
    if (!message.isEmpty()) {
        setError(tr("Could not open %1: %2").arg(QFileInfo(path).fileName(), message));
        return false;
    }
    m_panel = loaded;
    m_path = QFileInfo(path).absoluteFilePath();
    m_saved = m_panel.toJson();
    m_undo.clear();
    m_redo.clear();
    m_lastKey.clear();
    clearError();
    refresh();
    return true;
}

void Backend::open() {
    if (m_picker)
        m_picker->openPanel();
}

bool Backend::writeTo(const QString &path) {
    QString message;
    if (!m_panel.save(path, &message)) {
        setError(tr("Could not save %1: %2").arg(QFileInfo(path).fileName(), message));
        return false;
    }
    m_path = QFileInfo(path).absoluteFilePath();
    m_saved = m_panel.toJson();
    clearError();
    emit changed();
    return true;
}

void Backend::save() {
    if (m_path.isEmpty()) {
        saveAs();
        return;
    }
    if (writeTo(m_path))
        emit saved();
}

void Backend::saveAs() {
    if (!m_picker)
        return;
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!QDir(folder).exists())
        folder = QDir::homePath();
    QString name = m_panel.title.isEmpty() ? "panel" : m_panel.title;
    name.replace(QRegularExpression(R"([/\\:*?"<>|]+)"), "-");
    const QString suggested = m_path.isEmpty() ? QDir(folder).filePath(name + ".fluor") : m_path;
    m_picker->savePanel(QUrl::fromLocalFile(suggested));
}
