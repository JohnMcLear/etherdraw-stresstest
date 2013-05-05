#ifndef PAD_H
#define PAD_H

#include <QList>
#include <QObject>
#include <QString>

#include "Changeset.h"
#include "Logger.h"

class Pad : public QObject, private Logger {
    Q_OBJECT

  public:
    Pad(const QString & clientName, QObject *parent = 0);
    void setInitialText(int rev, const QString & text,
                        const QString & attribstr, QList<Attribute> apool);

    int rev() const { return m_rev; }

    QString toChangeset() const;
    QList<Attribute> attributes() const;
    int getNewLen() const;

    // For both of these, pos is an index into the new text
    void insertAt(int pos, const QString & text,
                  const QList<Attribute> & attributes);
    void deleteAt(int pos, int len);

  private:
    int m_rev;
    Changeset m_base; // changeset from empty document to m_rev
    Changeset m_changes; // local changes on top of m_rev
    QString m_text; // text after local changes
};

#endif
