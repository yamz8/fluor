#pragma once

#include <QQuickPaintedItem>

#include "backend.h"

// The spectra chart: every dye's emission (and, optionally, excitation) over
// the instrument's laser lines and filter bands. With one laser chosen, each
// emission is scaled by how brightly that laser drives the dye, which is what
// that laser's detectors actually see.
class SpectraPlot : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Backend *source READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(int laser READ laser WRITE setLaser NOTIFY laserChanged)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(bool excitation READ excitation WRITE setExcitation NOTIFY excitationChanged)
    Q_PROPERTY(double hoverX READ hoverX WRITE setHoverX NOTIFY hoverXChanged)
    Q_PROPERTY(QString emptyText READ emptyText WRITE setEmptyText NOTIFY emptyTextChanged)

public:
    explicit SpectraPlot(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    Backend *backend() const { return m_backend; }
    void setBackend(Backend *backend);
    int laser() const { return m_laser; }
    void setLaser(int laser);
    int selected() const { return m_selected; }
    void setSelected(int selected);
    bool excitation() const { return m_excitation; }
    void setExcitation(bool excitation);
    double hoverX() const { return m_hoverX; }
    void setHoverX(double x);
    QString emptyText() const { return m_emptyText; }
    void setEmptyText(const QString &text);

    // The dye whose curve is tallest under a point, or -1.
    Q_INVOKABLE int dyeAt(double x, double y) const;

signals:
    void backendChanged();
    void laserChanged();
    void selectedChanged();
    void excitationChanged();
    void hoverXChanged();
    void emptyTextChanged();

private:
    QRectF plotArea() const;
    double xFor(double nm) const;
    double nmFor(double x) const;
    // How tall dye d's emission is drawn, as a share of the plot height.
    double emissionScale(int d) const;

    Backend *m_backend = nullptr;
    int m_laser = -1;
    int m_selected = -1;
    bool m_excitation = true;
    double m_hoverX = -1;
    QString m_emptyText;
};
