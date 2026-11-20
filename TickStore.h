#pragma once

#include <QDate>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Holds the set of ticked days in memory and loads/saves it from/to an
// SQLite database file chosen by the user.
class TickStore : public QObject {
    Q_OBJECT
    QML_ELEMENT

    // Bumped whenever the set of ticked days changes, so QML bindings that
    // call isTicked() can depend on it and re-evaluate.
    Q_PROPERTY(int revision READ revision NOTIFY ticksChanged)
    Q_PROPERTY(bool modified READ isModified NOTIFY modifiedChanged)
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileUrlChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileUrlChanged)

public:
    explicit TickStore(QObject *parent = nullptr);

    [[nodiscard]] int revision() const { return m_revision; }
    [[nodiscard]] bool isModified() const { return m_modified; }
    [[nodiscard]] QUrl fileUrl() const { return m_fileUrl; }
    [[nodiscard]] QString fileName() const;

    // month is 1-based (January == 1).
    Q_INVOKABLE [[nodiscard]] bool isTicked(int year, int month, int day) const;
    Q_INVOKABLE void toggle(int year, int month, int day);

    Q_INVOKABLE void newFile();
    Q_INVOKABLE bool open(const QUrl &url);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QUrl &url);

    // The ticked days of the given month (1-based), earliest first, as
    // dd.MM.yyyy. Each date is listed twice; all lines are separated by '\n'.
    Q_INVOKABLE [[nodiscard]] QString monthAsText(int year, int month) const;
    // Puts monthAsText() on the clipboard.
    Q_INVOKABLE void copyMonth(int year, int month) const;

    // Reopens the file that was open when the app last closed, if it still
    // exists. Returns true if a file was opened.
    Q_INVOKABLE bool restoreLastFile();

signals:
    void ticksChanged();
    void modifiedChanged();
    void fileUrlChanged();
    void errorOccurred(const QString &message);

private:
    void setModified(bool modified);
    void setFileUrl(const QUrl &url);
    bool writeTo(const QString &path);

    QSet<QDate> m_ticks;
    int m_revision = 0;
    bool m_modified = false;
    QUrl m_fileUrl;
};
