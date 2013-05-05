#include "Changeset.h"

#include <QMap>
#include <QRegExp>

#include <QtAlgorithms> // for qSort

Changeset::Changeset(QObject *parent)
  : QObject(parent), m_orig_len(0), m_new_len(0), m_tidy(true),
    m_attributes_valid(false) {
}

QString Changeset::toString() const {
    if (m_ops.isEmpty())
        return "";

    tidyOps();
    QString result("Z:");
    result.append(QString::number(m_orig_len, 36));
    if (m_new_len >= m_orig_len)
        result.append(">" + QString::number(m_new_len - m_orig_len, 36));
    else
        result.append("<" + QString::number(m_orig_len - m_new_len, 36));

    QString charbank;
    QList<Attribute> apool = attributes();
    for (int i = 0; i < m_ops.length(); i++) {
        if (i == m_ops.length() - 1 && m_ops[i].opType == Keep
            && m_ops[i].attributes.isEmpty()) {
            continue;  // final Keep is always implicit
        }
        result.append(m_ops[i].toString(apool));
        charbank.append(m_ops[i].charbank);
    }

    result.append("$");
    result.append(charbank);
    return result;
}

QList<Attribute> Changeset::attributes() const {
    if (!m_attributes_valid) {
        tidyOps();
        QMap<Attribute, bool> pool;
        Q_FOREACH (const Op & op, m_ops) {
            Q_FOREACH (const Attribute & attr, op.attributes) {
                pool[attr] = true;
            }
        }
        m_attributes = pool.keys();
        m_attributes_valid = true;
    }
    return m_attributes;
}

// Some code duplication between the addXXX functions,
// but not enough to warrant merging them.

void Changeset::addInsert(const QString & text,
                          const QList<Attribute> & attributes) {
    int lines = text.count('\n');

    if (lines > 0 && !text.endsWith('\n')) {
        // Multiline ops must end on a newline, and this one doesn't.
        // Split it into two.
        int whole = text.lastIndexOf('\n') + 1;
        addInsert(text.left(whole), attributes);
        addInsert(text.mid(whole), attributes);
        return;
    }

    Op new_op;
    new_op.opType = Insert;
    new_op.lines = lines;
    new_op.chars = text.length();
    new_op.charbank = text;
    new_op.attributes = attributes;
    qSort(new_op.attributes);

    m_ops << new_op;
    m_new_len += new_op.chars;
    m_attributes_valid = false;
    m_tidy = false;
}

void Changeset::addKeep(const QString & text,
                        const QList<Attribute> & attributes) {
    int lines = text.count('\n');

    if (lines > 0 && !text.endsWith('\n')) {
        // Multiline ops must end on a newline, and this one doesn't.
        // Split it into two.
        int whole = text.lastIndexOf('\n') + 1;
        addKeep(lines, whole, attributes);
        addKeep(0, text.length() - whole, attributes);
        return;
    }

    addKeep(lines, text.length(), attributes);
}

void Changeset::addKeep(int lines, int chars,
                        const QList<Attribute> & attributes) {
    Op new_op;
    new_op.opType = Keep;
    new_op.lines = lines;
    new_op.chars = chars;
    new_op.attributes = attributes;
    qSort(new_op.attributes);

    m_ops << new_op;
    m_orig_len += new_op.chars;
    m_new_len += new_op.chars;
    m_attributes_valid = false;
    m_tidy = false;
}

void Changeset::addDelete(const QString & text) {
    int lines = text.count('\n');

    if (lines > 0 && !text.endsWith('\n')) {
        // Multiline ops must end on a newline, and this one doesn't.
        // Split it into two.
        int whole = text.lastIndexOf('\n') + 1;
        addDelete(lines, whole);
        addDelete(0, text.length() - whole);
        return;
    }

    addDelete(lines, text.length());
}

void Changeset::addDelete(int lines, int chars) {
    Op new_op;
    new_op.opType = Delete;
    new_op.lines = lines;
    new_op.chars = chars;

    m_ops << new_op;
    m_orig_len += new_op.chars;
    m_tidy = false;
}

void Changeset::apply(const Changeset * other) {
    if (other->origLen() != this->newLen())
        m_errors << "applying changeset with wrong orig length";

    tidyOps();
    other->tidyOps();
    QList<Op> x_ops = other->m_ops;

    int a = 0;
    int b = 0;
    while (a < m_ops.length() && b < x_ops.length()) {
        if (m_ops[a].opType == Delete) {
            // The deleted text is already gone in the world of x_ops,
            // so there's no interaction.
            a++;
            continue;
        }

        if (x_ops[b].opType == Insert) {
            m_ops.insert(a, x_ops[b]);
            m_new_len += x_ops[b].chars;
            a++;
            b++;
            continue;
        }

        // make the ops equal size
        // The loop will still terminate because only one of the lists
        // will grow but in the cases that follow, both a and b get closer
        // to the end of their lists.
        if (m_ops[a].chars < x_ops[b].chars) {
            Op split_op;
            split_op.splitFrom(x_ops[b], m_ops[a].lines, m_ops[a].chars);
            x_ops.insert(b, split_op);
        } else if (m_ops[a].chars > x_ops[b].chars) {
            Op split_op;
            split_op.splitFrom(m_ops[a], x_ops[b].lines, x_ops[b].chars);
            m_ops.insert(a, split_op);
        }

        if (x_ops[b].opType == Keep) {
            m_ops[a].mergeAttributes(x_ops[b].attributes);
            a++;
            b++;
            continue;
        }

        // x_ops[b] must be a Delete
        m_new_len -= x_ops[b].chars;

        if (m_ops[a].opType == Insert) {
            // delete by undoing the insert
            m_ops.removeAt(a);
            b++;
        } else {
            // delete by replacing the Keep
            m_ops[a] = x_ops[b];
            a++;
            b++;
        }
    }

    // Leftover ops from the other changeset will replace the implicit Keep
    while (b < x_ops.length()) {
        m_ops << x_ops[b];
        if (x_ops[b].opType == Delete)
            m_new_len -= x_ops[b].chars;
        else if (x_ops[b].opType == Insert)
            m_new_len += x_ops[b].chars;
        b++;
    }

    m_tidy = false;
}

void Changeset::follow(const Changeset * other) { // stub
}

// Numbers in the changeset are base-36 so [0-9a-z] matches a digit
// Prefix tokens are * for attribute spec, | for line count.
// Op tokens are + for insert, - for delete, = for keep.
// The changeset starts with special ops : (orig length) followed
// by < or > (total change in length).
// After the ops is a $ sign followed by all the characters to be inserted.
#define OP_RE "([*][0-9a-z]+)*([|][0-9a-z]+)?[=+-][0-9a-z]+"
#define CHANGESET_RE "Z:([0-9a-z]+)([<>])([0-9a-z]+)(" OP_RE ")+([$](.*))?"

void Changeset::parse(const QString & changeset,
                      const QList<Attribute> & apool) {
    QRegExp changesetMatcher(CHANGESET_RE);
    QRegExp numberMatcher("[0-9a-z]+");

    if (!changeset.startsWith("Z:")) {
        m_errors << "not a changeset";
        return;
    }

    if (!changesetMatcher.exactMatch(changeset)) {
        m_errors << "changeset syntax error";
        return;
    }

    m_orig_len = changesetMatcher.cap(1).toInt(0, 36);
    int difference = changesetMatcher.cap(3).toInt(0, 36);
    if (changesetMatcher.cap(2) == ">")
        m_new_len = m_orig_len + difference;
    else
        m_new_len = m_orig_len - difference;

    QString charbank = changesetMatcher.cap(8);
    int charbank_used = 0;

    // pos(4) unhelpfully gives the position of the *last* op, so avoid it.
    int pos = changesetMatcher.pos(3) + changesetMatcher.cap(3).length();
    while (pos < changeset.length() && changeset[pos] != '$') {
        Op op;

        while (changeset[pos] == '*') {
            numberMatcher.indexIn(changeset, pos + 1);
            pos += 1 + numberMatcher.matchedLength();
            int a = numberMatcher.cap(0).toInt(0, 36);

            if (a >= apool.length()) {
                m_errors << "changeset attribute out of range";
            } else {
                op.attributes << apool[a];
            }
        }

        op.lines = 0;
        if (changeset[pos] == '|') {
            numberMatcher.indexIn(changeset, pos + 1);
            pos += 1 + numberMatcher.matchedLength();
            op.lines = numberMatcher.cap(0).toInt(0, 36);
        }

        op.opType = changeset[pos] == '=' ? Keep
                  : changeset[pos] == '+' ? Insert
                  : Delete;
        
        numberMatcher.indexIn(changeset, pos + 1);
        pos += 1 + numberMatcher.matchedLength();
        op.chars = numberMatcher.cap(0).toInt(0, 36);

        if (op.opType == Insert) {
            op.charbank = charbank.mid(charbank_used, op.chars);
            charbank_used += op.chars;
            if (op.charbank.length() != op.chars)
                m_errors << "charset charbank is too short";
            if (op.lines > 0 && !op.charbank.endsWith('\n'))
                m_errors << "multiline insert does not end with newline";
            Q_FOREACH (const Attribute & attr, op.attributes) {
                if (attr.value.isEmpty())
                    m_errors << "changeset inserts empty attribute";
            }
        }

        if (op.opType == Delete && !op.attributes.isEmpty())
            m_errors << "changeset has delete with attributes";

        m_ops << op;
    }

    if (m_errors.isEmpty() && this->toString() != changeset)
        m_errors << "changeset not in canonical form";
}

void Changeset::tidyOps() const {
    int i = 0;
    while (i < m_ops.length()) {
        // Remove empty ops
        if (m_ops[i].chars == 0) {
            m_ops.removeAt(i);
            continue;
        }

        if (i == m_ops.length() - 1) {
            // Remove implicit Keep at end
            if (m_ops[i].opType == Keep && m_ops[i].attributes.isEmpty()) {
                m_ops.removeAt(i);
            }
        } else {
            // Make sure Delete comes before Insert
            if (m_ops[i].opType == Insert && m_ops[i + 1].opType == Delete) {
                m_ops.swap(i, i + 1);
                // Recheck previous op now that it has a new neighbor.
                // This is guaranteed to terminate eventually because
                // the swaps only go in one direction.
                if (i > 0)
                    i--;
                continue;
            }

            // Merge neighboring ops if possible
            // A multiline op cannot be merged with a following single line op,
            // but the other way around is ok.
            if (m_ops[i].opType == m_ops[i + 1].opType
                && m_ops[i].attributes == m_ops[i + 1].attributes
                && (m_ops[i].lines == 0 || m_ops[i + 1].lines > 0)) {
                m_ops[i].lines += m_ops[i + 1].lines;
                m_ops[i].chars += m_ops[i + 1].chars;
                m_ops[i].charbank.append(m_ops[i + 1].charbank);
                m_ops.removeAt(i + 1);
                continue;
            }
        }
        i++;
    }
    m_attributes_valid = false;
    m_tidy = true;
}

QString Changeset::Op::toString(const QList<Attribute> & apool) const {
    QString result;
    Q_FOREACH (const Attribute & attr, attributes) {
        result.append("*" + QString::number(apool.indexOf(attr), 36));
    }
    if (lines > 0) {
        result.append("|" + QString::number(lines, 36));
    }
    result.append(opType == Keep ? '=' : opType == Insert ? '+' : '-');
    result.append(QString::number(chars, 36));
    return result;
}

void Changeset::Op::splitFrom(Op & other, int lines, int chars) {
    this->opType = other.opType;
    this->lines = lines;
    other.lines -= lines;
    this->chars = chars;
    other.chars -= chars;
    if (this->opType == Insert) {
        this->charbank = other.charbank.left(chars);
        other.charbank = other.charbank.mid(chars);
    }
    this->attributes = other.attributes;
}

void Changeset::Op::mergeAttributes(const QList<Attribute> & attributes) {
    bool needs_sort = false;

    Q_FOREACH(const Attribute & attr, attributes) {
        bool found = false;
        for (int a = 0; a < this->attributes.length(); a++) {
            if (this->attributes[a].key == attr.key) {
                if (attr.value.isEmpty() && opType == Insert) {
                    this->attributes.removeAt(a);
                } else {
                    this->attributes[a] = attr;
                }
                found = true;
                break;
            }
        }

        if (!found) {
            this->attributes << attr;
            needs_sort = true;
        }
    }

    if (needs_sort)
        qSort(this->attributes);
}

Attribute::Attribute(const QString & key, const QString & value) {
    this->key = key;
    this->value = value;
}

bool Attribute::operator< (const Attribute & other) const {
    return this->key < other.key
        || (this->key == other.key && this->value < other.value);
}

bool Attribute::operator== (const Attribute & other) const {
    return this->key == other.key && this->value == other.value;
}
