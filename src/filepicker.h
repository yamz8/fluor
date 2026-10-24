#pragma once

#include <QObject>
#include <QUrl>

// Open and save dialogs, behind an interface so tests can answer them.
class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    virtual void openPanel() = 0;
    virtual void savePanel(const QUrl &suggestedUrl) = 0;

signals:
    void openSelected(const QUrl &url);
    void saveSelected(const QUrl &url);
    void failed(const QString &message);
};
