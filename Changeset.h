#ifndef CHANGESET_H
#define CHANGESET_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class Attribute {
  public:
    Attribute(const QString & key, const QString & value);

    QString key;
    QString value; // may be empty when used with Keep ops

    bool operator< (const Attribute & other) const;
    bool operator== (const Attribute & other) const;
};

class Changeset : public QObject {
    Q_OBJECT

  public:
    Changeset(QObject *parent = 0);

    // Create a changeset either with parse() or with a sequence of addInsert,
    // addKeep and addDelete calls. Don't mix them.
    void parse(const QString & changeset, const QList<Attribute> & apool);

    void addInsert(const QString & text, const QList<Attribute> & attributes);
    void addKeep(const QString & text, const QList<Attribute> & attributes);
    void addKeep(int lines, int chars, const QList<Attribute> & attributes);
    void addDelete(const QString & text);
    void addDelete(int lines, int chars);

    // apply() takes a changeset that's based on this one and folds it in,
    // so that this changeset applies the new changes too.
    void apply(const Changeset * other);

    // follow() takes a changeset that's based on the same revision as this one
    // and rebases this one so that it can be applied after the other one.
    void follow(const Changeset * other);

    QString toString() const;
    int origLen() const { return m_orig_len; }
    int newLen() const { return m_new_len; }
    QList<Attribute> attributes() const;

    // These are treated as const so that const methods can still report errors
    QStringList errors() const { return m_errors; }
    void clearErrors() const { m_errors.clear(); }

  protected:
    void tidyOps() const;

  private:
    enum OpType { Keep, Insert, Delete };
    class Op {
      public:
        OpType opType;
        // if lines > 0, the op must be for text that ends with a newline
        int lines;
        int chars;
        QString charbank;  // only for Insert
        // the attribute list is kept sorted
        QList<Attribute> attributes;  // not for Delete.

        QString toString(const QList<Attribute> & apool) const;
        void splitFrom(Op & other, int lines, int chars);
        void mergeAttributes(const QList<Attribute> & attributes);
    };

    int m_orig_len;
    int m_new_len;
    // These are all 'mutable' just so that tidyOps() and attributes()
    // can be const. It's a pity, but that's what you get for delaying
    // expensive operations until they're needed.
    mutable QList<Op> m_ops;
    mutable bool m_tidy; // are ops in canonical form?
    // The attribute list is recalculated from m_ops when needed.
    mutable QList<Attribute> m_attributes;
    mutable bool m_attributes_valid;
    mutable QStringList m_errors;
};

#endif
