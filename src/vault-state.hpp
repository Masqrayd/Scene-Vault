#pragma once
#include <QSet>
#include <QString>

// Scene UUIDs, rather than names, survive renaming and collection exports.
struct VaultState {
    QSet<QString> hidden;
    QString editing;
    QString returnPreview;
    bool suspended = false;

    bool shouldHide(const QString &id, const QSet<QString> &active) const
    {
        return !suspended && hidden.contains(id) && editing != id && !active.contains(id);
    }
    bool mayHide(const QString &id, const QSet<QString> &existing,
                 const QSet<QString> &active) const
    {
        if (!existing.contains(id) || active.contains(id)) return false;
        if (hidden.contains(id)) return true;
        auto visible = existing;
        visible.subtract(hidden);
        return visible.size() > 1;
    }
    void resetSession()
    {
        editing.clear();
        returnPreview.clear();
        suspended = false;
    }
};
