#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <functional>

#include "panel.h"

class FilePicker;

// The panel being planned, its analysis, its file, and its undo history.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
    Q_PROPERTY(QString filePath READ filePath NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QVariantList dyes READ dyes NOTIFY changed)
    Q_PROPERTY(QVariantList lasers READ lasers NOTIFY changed)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
    Q_PROPERTY(QVariantList spill READ spill NOTIFY changed)
    Q_PROPERTY(QVariantList issues READ issues NOTIFY changed)
    Q_PROPERTY(double worstSpill READ worstSpill NOTIFY changed)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY changed)
    Q_PROPERTY(QVariantList presets READ presets CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit Backend(FilePicker *picker = nullptr, QObject *parent = nullptr);

    const Panel &panel() const { return m_panel; }
    const Analysis &analysis() const { return m_analysis; }
    const QList<spectra::Spectrum> &curves() const { return m_curves; }

    QString title() const { return m_panel.title; }
    QString fileName() const;
    QString filePath() const { return m_path; }
    bool dirty() const;
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QVariantList dyes() const;
    QVariantList lasers() const;
    QVariantList channels() const;
    QVariantList spill() const;
    QVariantList issues() const;
    double worstSpill() const { return m_analysis.worstSpill; }
    int errorCount() const { return m_analysis.errors(); }
    QVariantList presets() const;
    QString error() const { return m_error; }

    Q_INVOKABLE QVariantList searchDyes(const QString &query) const;
    // Every detector, with how well it sees the dye (0 to 1 of its best) and
    // which other dye already sits there.
    Q_INVOKABLE QVariantList channelOptions(int dye) const;

    Q_INVOKABLE bool addDye(const QString &name);
    Q_INVOKABLE void removeDye(int index);
    Q_INVOKABLE void setChannel(int dye, int channel);
    Q_INVOKABLE void stepChannel(int dye, int step);
    Q_INVOKABLE void autoAssign();
    Q_INVOKABLE void setTitle(const QString &title);

    Q_INVOKABLE void applyPreset(const QString &id);
    Q_INVOKABLE void addLaser(int nm);
    Q_INVOKABLE void removeLaser(int laser);
    Q_INVOKABLE void setLaser(int laser, int nm);
    Q_INVOKABLE void addFilter(int laser);
    Q_INVOKABLE void removeFilter(int laser, int filter);
    Q_INVOKABLE void setFilter(int laser, int filter, int center, int width);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    Q_INVOKABLE void newPanel();
    Q_INVOKABLE bool load(const QString &pathOrUrl);
    Q_INVOKABLE void open();
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs();
    Q_INVOKABLE void clearError();

signals:
    void changed();
    void errorChanged();
    void saved();

private:
    // Applies one edit as an undoable step. Edits sharing a key within a
    // moment (a spin box being stepped) collapse into one step.
    void edit(const std::function<void(Panel &)> &change, const QString &key = {});
    void refresh();
    void setError(const QString &message);
    bool writeTo(const QString &path);

    FilePicker *m_picker;
    Panel m_panel;
    Analysis m_analysis;
    QList<spectra::Spectrum> m_curves;
    QString m_path;
    QJsonObject m_saved;
    QList<Panel> m_undo, m_redo;
    QString m_lastKey;
    QElapsedTimer m_lastEdit;
    QString m_error;
};
