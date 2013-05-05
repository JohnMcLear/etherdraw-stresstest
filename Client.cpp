#include "Client.h"

#include <QVariantMap>

#include <QtGlobal>

#include <qjson/serializer.h>

#include "XhrClient.h"

Client::Client(QUrl padurl, const QString & name, QObject *parent)
  : QObject(parent), Logger(name), m_padurl(padurl), m_pad(name)  {

    m_state = CsCreated;
    m_logic = "lurk";

    QUrl baseurl(padurl);
    // Strip off p/PADNAME
    QString::SectionFlags flags = QString::SectionSkipEmpty
        | QString::SectionIncludeTrailingSep
        | QString::SectionIncludeLeadingSep;
    baseurl.setPath(padurl.path().section('/', 0, -3, flags));

    m_pad_id = m_padurl.path().section('/', -1, -1);

    m_xhr = new XhrClient(padurl, baseurl, name, this);
    connect(m_xhr, SIGNAL(ready()), SLOT(transportReady()));
    connect(m_xhr, SIGNAL(disconnected()), SLOT(transportDisconnected()));
    connect(m_xhr, SIGNAL(received_message(QVariant, QString)),
                   SLOT(received_message(QVariant, QString)));

    m_kick.setSingleShot(true);
    connect(&m_kick, SIGNAL(timeout()), SLOT(kick()));

    m_author_name = QString("robot") + name;
}

Client::~Client() {
    delete m_xhr;
}

void Client::setLogic(const QString & logic) {
    m_logic = logic;
}

void Client::start() {
    changeState(CsStarting);
    kickAfter(10);
    m_xhr->start();
}

void Client::end() {
    log(Info, "terminating");
    delete m_xhr;
    m_xhr = 0;
}

namespace {

    static QString randomChars(int len) {
        const char base[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        QString result(len, ' ');
        for (int i = 0; i < len; i++) {
            // size - 1 because base has a trailing NUL
            result[i] = base[qrand() % (sizeof(base) - 1)];
        }
        return result;
    }

}

void Client::transportReady() {
    // send CLIENT_READY message to server

    QString token = m_xhr->getCookie("token");
    if (token.isEmpty()) {
        token = QString("t.") + randomChars(20);
        m_xhr->setCookie("token", token);
    }
    
    QVariantMap msg;
    msg["component"] = "pad";
    msg["type"] = "CLIENT_READY";
    msg["padId"] = m_pad_id;
    msg["sessionID"] = m_xhr->getCookie("sessionID");
    msg["password"] = m_xhr->getCookie("password");
    msg["token"] = token;
    msg["protocolVersion"] = 2;

    if (m_logic == "oldreconnect" && m_pad.rev() > 0) {
        log(Info, "Sending CLIENT_VARS with reconnect and rev 0");
        msg["reconnect"] = true;
        msg["client_rev"] = 0; // lie!
        changeState(CsActive); // we won't get CLIENT_VARS on reconnect
    } else if (m_logic == "disconnect") {
        log(Info, "Skipping GETVARS");
    } else {
        changeState(CsGettingVars);
    }
    log(Info, "Sending initial CLIENT_READY");
    m_xhr->send(msg);

    if (m_logic == "disconnect") {
        log(Info, "Disconnecting");
        m_xhr->disconnect();
    }
}

void Client::transportDisconnected() {
    changeState(CsDisconnected);
    kickAfter(1, 10);
}

void Client::changeState(ClientState state) {
    if (m_state == state)
        return;
    log(Info, stateName(m_state) + " -> " + stateName(state));
    m_state = state;
}

QString Client::stateName(ClientState state) {
    switch (state) {
        case CsCreated: return "CREATED";
        case CsStarting: return "STARTING";
        case CsGettingVars: return "GETVARS";
        case CsActive: return "ACTIVE";
        case CsDisconnected: return "DISCONNECTED";
    }
    return "UNKNOWN";
}

void Client::kickAfter(int secs) {
    m_kick.start(secs * 1000);
    m_elapsed.start();
}

void Client::kickAfter(int secs_min, int secs_max) {
    // convert to milliseconds
    secs_min *= 1000;
    secs_max *= 1000;
    int msecs = qrand() % (secs_max - secs_min + 1) + secs_min;
    m_kick.start(msecs);
    m_elapsed.start();
}

int Client::elapsedSecs() {
    return m_elapsed.elapsed() / 1000;
}

void Client::makeRandomEdit() {
    int chars = qrand() % 20 + 1;
    int len = m_pad.getNewLen() - 1; // subtract final newline
    if (chars >= len || qrand() & 1024) { // coinflip
        QList<Attribute> attrs;
        attrs << Attribute("author", m_author_id);
        int pos = qrand() % m_pad.getNewLen();
        m_pad.insertAt(pos, randomChars(chars), attrs);
    } else {
        int pos = qrand() % (m_pad.getNewLen() - chars);
        m_pad.deleteAt(pos, chars);
    }
}

void Client::kick() {
    switch (m_state) {
        case CsCreated:
            log(Error, "got kicked in " + stateName(m_state) + " state");
            break;

        case CsStarting:
            log(Error, "transport not ready after "
                       + QString::number(elapsedSecs()) + " seconds");
            log(Info, "retrying start");
            start();
            break;

        case CsGettingVars:
            log(Error, "did not get client vars after "
                       + QString::number(elapsedSecs()) + " seconds");
            log(Info, "retrying CLIENT_READY");
            transportReady();
            break;

        case CsActive:
            if (m_logic == "badfollow") {
                sendBadFollow();
            } else if (m_logic == "draw") {
                for (int i = 1; i <= 3; i++)
                    makeRandomEdit();
                sendChangeset(m_pad.toChangeset(), m_pad.attributes());
                kickAfter(10);
            } else if (m_logic == "oldreconnect") {
                if (m_pad.rev() > 0) {
                    log(Info, "disconnecting for oldreconnect");
                    start();
                }
            }
            break;

        case CsDisconnected:
            log(Info, "reconnecting");
            start();
            break;
    }
}

void Client::received_message(QVariant message, QString orig_text) {
    QVariantMap msg = message.toMap();
    if (msg["disconnect"].isValid()) {
        QString disconnect_msg = msg["disconnect"].toString();
        log(Warning, "received disconnect message: " + disconnect_msg);
        changeState(CsDisconnected);
        kickAfter(10);
        return;
    }
    if (msg["type"].toString() == "CLIENT_VARS") {
        if (m_state != CsGettingVars)
            log(Error, "Received CLIENT_VARS in state " + stateName(m_state));
        getClientVars(msg["data"].toMap());
        changeState(CsActive);
        kickAfter(10);
        return;
    }
    if (msg["type"].toString() == "COLLABROOM") {
        if (m_state != CsActive) {
            log(Error, "Received COLLABROOM in state " + stateName(m_state));
        }
        QVariantMap data = msg["data"].toMap();
        if (data["type"].toString() == "USER_NEWINFO") {
            QVariantMap info = data["userInfo"].toMap();
            log(Verbose, "received USER_NEWINFO " + info["userId"].toString()
                         + " " + info["name"].toString());
            return;
        }
        if (data["type"].toString() == "USER_LEAVE") {
            QVariantMap info = data["userInfo"].toMap();
            log(Verbose, "received USER_LEAVE " + info["userId"].toString());
            return;
        }
    }
    log(Info, "Received unknown message " + orig_text);
}

void Client::sendUserInfo() {
    QVariantMap msg;
    QVariantMap data;
    QVariantMap info;

    info["userId"] = m_author_id;
    info["name"] = m_author_name;
    info["colorId"] = m_color;
    info["ip"] = "127.0.0.1";
    info["userAgent"] = "Anonymous";

    data["type"] = "USERINFO_UPDATE";
    data["userInfo"] = info;

    msg["type"] = "COLLABROOM";
    msg["component"] = "pad";
    msg["data"] = data;

    log(Verbose, "sending userinfo update " + m_author_id + " "
                 + m_color + " " + m_author_name);

    if (m_logic == "blackhat") {
        msg["disconnect"] = "mysterious server error";
        log(Info, "sending force-disconnect message to other clients");
    }

    m_xhr->send(msg);
}

void Client::getClientVars(QVariantMap vars) {
    // Known keys we don't care about
    //   abiwordAvailable    string "yes" or "no"
    //   clientIp            string "127.0.0.1" (literally)
    //   initialTitle        string "Pad: PADNAME"
    //   isProPad            bool
    //   numConnectedUsers   int
    //   plugins             {}
    //   readOnlyId          string "r.RANDOMTEXT"
    //   readOnly            bool
    //   serverTimestamp     int (msecs since epoch)
    //   userIsGuest         bool
    //   accountPrivs        {"maxRevisions": 100}
    //   cookiePrefsToSet    {"fullWidth": bool, "hideSidebar": bool}
    //   opts                {}
    //   initialOptions      {"guestPolicy": "deny"}
    //
    // Known keys we might care about in the future
    //   chatHistory         []
    //   initialChangesets   []
    //   initialRevisionList []
    //   savedRevisions      []

    if (vars["padId"].toString() != m_pad_id) {
        log(Warning, "Received client vars for pad " + vars["padId"].toString()
                     + " instead of expected " + m_pad_id);
    }
    if (vars["globalPadId"].toString() != m_pad_id) {
        log(Warning, "Received global pad id " + vars["globalPadId"].toString()
                     + " instead of expected " + m_pad_id);
    }

    m_author_id = vars["userId"].toString();
    log(Verbose, "received author id " + m_author_id);

    //   colorPalette        ["#ffc7c7", ...]
    QVariantList palette = vars["colorPalette"].toList();
    //   userColor           int (index into colorPalette)
    int colorindex = vars["userColor"].toInt();
    if (colorindex < 0 || colorindex >= palette.length()) {
        log(Error, "Received userColor " + QString::number(colorindex)
            + " into palette size " + QString::number(palette.length()));
        m_color = "#7f7f7f";
    } else {
        m_color = palette[colorindex].toString();
        log(Trace, "got assigned color " + m_color);
    }

    if (vars["userName"].toString().isEmpty()) {
        sendUserInfo();
    } else {
        m_author_name = vars["userName"].toString();
        log(Info, "accepting author name " + m_author_name);
    }

    QVariantMap collabvars = vars["collab_client_vars"].toMap();
    // Known collab vars we don't care about
    //   clientIp            string "127.0.0.1" (literally)
    //   time                int (msecs since epoch)
    //
    // Known collab vars to use later
    //   historicalAuthorData
    //     map indexed by author ids, values are maps with keys
    //        colorId, name (can be null), padIds
    //       where padIds is a map with pad names as keys and all values 1

    if (collabvars["globalPadId"].toString() != m_pad_id) {
        log(Warning, "Received collabvars global pad id "
                     + collabvars["globalPadId"].toString()
                     + " instead of expected " + m_pad_id);
    }
    if (collabvars["padId"].toString() != m_pad_id) {
        log(Warning, "Received collabvars pad id "
                     + collabvars["padId"].toString()
                     + " instead of expected " + m_pad_id);
    }

    //   apool["numToAttrib"]
    //     map indexed by numeric strings, values are [attrib, value]
    //   apool["nextNum"]    int, highest attrib + 1
    QVariantMap apool_data = collabvars["apool"].toMap()["numToAttrib"].toMap();
    int nextNum = collabvars["apool"].toMap()["nextNum"].toInt();
    QList<Attribute> apool;
    for (int i = 0; i < nextNum; i++) {
        QVariantList attrib_data = apool_data[QString::number(i)].toList();
        apool << Attribute(attrib_data[0].toString(),
                           attrib_data[1].toString());
    }

    m_pad.setInitialText(collabvars["rev"].toInt(),
        collabvars["initialAttributedText"].toMap()["text"].toString(),
        collabvars["initialAttributedText"].toMap()["attribs"].toString(),
        apool);
    log(Info, "received rev " + QString::number(m_pad.rev()));
}

void Client::sendBadFollow() {
    // create a USER_CHANGES message that creates a changeset claiming
    // to be based on an older revision (so that the server has to
    // update it to the current revision with the "follow" operation)
    // but which does not match the size of that older revision
    // (so that the "follow" will fail).
    // Actually we don't know the size of the older revision so
    // let's just guess that it isn't 5.
    QVariantMap msg;
    QVariantMap data;
    QVariantMap apool;

    apool["numToAttrib"] = QVariantMap();
    apool["nextNum"] = 0;

    data["type"] = "USER_CHANGES";
    data["baseRev"] = m_pad.rev() - 1;
    // Z (protocol start) :5 (previous size 5) >4 (final size 4 larger)
    // +4 (add 4 chars from char bank) $ (start of char bank) BAM! (char bank)
    // This adds the chars at the start of the pad. Keeping the rest of the
    // old pad is implicit.
    data["changeset"] = "Z:5>4+4$BAM!";
    data["apool"] = apool;

    msg["type"] = "COLLABROOM";
    msg["component"] = "pad";
    msg["data"] = data;

    log(Warning, "sending bad follow changeset for rev "
                  + data["baseRev"].toString());
    m_xhr->send(msg);
}

void Client::sendChangeset(const QString & changeset,
                           const QList<Attribute> & attributes) {
    QVariantMap msg;
    QVariantMap data;
    QVariantMap apool;
    QVariantMap num_to_attrib;
    QVariantMap attrib_to_num;

    // apool is sent in a particularly inefficient format.
    for (int i = 0; i < attributes.length(); i++) {
        QVariantList l;
        l << attributes[i].key;
        l << attributes[i].value;
        num_to_attrib[QString::number(i)] = l;
        attrib_to_num[attributes[i].key + "," + attributes[i].value] = i;
    }
    apool["numToAttrib"] = num_to_attrib;
    apool["attribToNum"] = attrib_to_num;
    apool["nextNum"] = attributes.length();

    data["type"] = "USER_CHANGES";
    data["baseRev"] = m_pad.rev();
    data["changeset"] = changeset;
    data["apool"] = apool;

    msg["type"] = "COLLABROOM";
    msg["component"] = "pad";
    msg["data"] = data;

    // Jump through hoops to make sure newlines in changeset don't spoil the log
    log(Info, "sending changeset for rev " + data["baseRev"].toString() + ": "
        + QString::fromUtf8(QJson::Serializer().serialize(changeset)));
    m_xhr->send(msg);
}
