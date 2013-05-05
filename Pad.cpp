#include "Pad.h"

#include <QtGlobal>

Pad::Pad(const QString & clientName, QObject *parent)
    : QObject(parent), Logger(clientName), m_rev(0) {
}

void Pad::setInitialText(int rev, const QString & text, const QString & attribstr, QList<Attribute> apool) {
    m_rev = rev;
    // attribstr is basically a changeset with some parts left out.
    // Add them back in and it becomes the changeset from the empty document
    // to the current rev.
    m_base.parse("Z:0>" + QString::number(text.length(), 36)
                 + attribstr + "$" + text, apool);
    Q_FOREACH(const QString & err, m_base.errors()) {
        log(Error, err + ": " + attribstr);
    }
    m_base.clearErrors();

    m_changes.addKeep(text, QList<Attribute>());
    m_text = text;
}

QString Pad::toChangeset() const {
    QString changeset = m_changes.toString();
    Q_FOREACH(const QString & err, m_changes.errors()) {
        log(Error, err + ": " + changeset);
    }
    m_changes.clearErrors();

    // Re-parse the changeset to catch client side errors
    Changeset test;
    test.parse(changeset, m_changes.attributes());
    Q_FOREACH(const QString & err, test.errors()) {
        log(Error, err + ": " + changeset);
    }

    return changeset;
}

QList<Attribute> Pad::attributes() const {
    return m_changes.attributes();
}

int Pad::getNewLen() const {
    return m_text.length();
}

void Pad::insertAt(int pos, const QString & text,
                   const QList<Attribute> & attributes) {
    Changeset changes;
    changes.addKeep(m_text.left(pos), QList<Attribute>());
    changes.addInsert(text, attributes);
    changes.addKeep(m_text.mid(pos), QList<Attribute>());
    m_changes.apply(&changes);
    m_text.insert(pos, text);

    if (m_text.length() != m_changes.newLen())
        log(Error, "changeset and local text length do not match after insert");
    Q_FOREACH(const QString & err, m_changes.errors()) {
        log(Error, err);
    }
    m_changes.clearErrors();
}

void Pad::deleteAt(int pos, int len) {
    QString deleted = m_text.mid(pos, len);
    Changeset changes;
    changes.addKeep(m_text.left(pos), QList<Attribute>());
    changes.addDelete(deleted);
    changes.addKeep(m_text.mid(pos + len), QList<Attribute>());
    m_changes.apply(&changes);
    m_text.remove(pos, len);

    if (m_text.length() != m_changes.newLen())
        log(Error, "changeset and local text length do not match after delete");
    Q_FOREACH(const QString & err, m_changes.errors()) {
        log(Error, err);
    }
    m_changes.clearErrors();
}
