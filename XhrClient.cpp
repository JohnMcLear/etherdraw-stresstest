#include "XhrClient.h"

#include <QAuthenticator>
#include <QChar>
#include <QDateTime>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QTimer>

#include <qjson/serializer.h>
#include <qjson/parser.h>

#define START_RETRY_SECS 1

#define MULTIMSG QChar(0xfffd)


XhrClient::XhrClient(QUrl padurl, QUrl baseurl, const QString & name,
                     QObject *parent)
  : QObject(parent), Logger(name), m_padurl(padurl), m_baseurl(baseurl)  {

    m_state = XhrInit;

    m_network = 0;
    m_receive = 0;

    if (m_baseurl.userName() != "") {
        m_username = m_baseurl.userName();
        m_baseurl.setUserName("");
    }
    if (m_baseurl.password() != "") {
        m_password = m_baseurl.password();
        m_baseurl.setPassword("");
    }
    m_padurl.setUserName("");
    m_padurl.setPassword("");
}

XhrClient::~XhrClient() {
    log(Trace, "transport disconnecting");
    delete m_receive;
    delete m_network;
}

void XhrClient::authenticate(QNetworkReply *, QAuthenticator *auth) {
    log(Trace, QString("authenticating ") + auth->realm());
    auth->setUser(m_username);
    auth->setPassword(m_password);
}

void XhrClient::get(QUrl url) {
    if (m_receive)
        log(Error, QString("xhr getting url while waiting for reply: ")
                   + url.toString());
    url.addQueryItem("t", QString::number(QDateTime::currentMSecsSinceEpoch()));
    log(Trace, "GET " + url.toString());
    m_receive = m_network->get(QNetworkRequest(url));
    m_receive->ignoreSslErrors();
    connect(m_receive, SIGNAL(finished()), SLOT(get_reply()));
    connect(m_receive, SIGNAL(error(QNetworkReply::NetworkError)),
                       SLOT(error(QNetworkReply::NetworkError)));
}

void XhrClient::send_packet(const QByteArray & msg_string) {
    QUrl url(m_baseurl);
    url.setPath(m_baseurl.path() + "socket.io/1/xhr-polling/" + m_id);
    url.addQueryItem("t", QString::number(QDateTime::currentMSecsSinceEpoch()));
    log(Trace, "POST " + QString::fromUtf8(msg_string));
    QNetworkReply *reply = m_network->post(QNetworkRequest(url), msg_string);
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                   SLOT(send_error(QNetworkReply::NetworkError)));
    // autodestruct reply object
    connect(reply, SIGNAL(finished()), reply, SLOT(deleteLater()));
}

void XhrClient::send(const QVariant & msg) {
    QByteArray msg_string = QJson::Serializer().serialize(msg);
    msg_string.prepend("4:::");
    send_packet(msg_string);
}

void XhrClient::disconnect() {
    QByteArray msg_string = "0::";
    send_packet(msg_string);
}

void XhrClient::start() {
    QNetworkCookieJar *jar = 0;

    // Clean up, in case this is a restart
    if (m_receive) {
        m_receive->deleteLater();
        m_receive = 0;
    }
    if (m_network) {
        log(Trace, "cleaning up old transport");
        // Reuse the cookie jar. It will be reparented to the new m_network.
        jar = m_network->cookieJar();
        m_network->deleteLater();
    }

    m_network = new QNetworkAccessManager(this);
    connect(m_network,
      SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator *)),
      SLOT(authenticate(QNetworkReply *, QAuthenticator *)));
    if (jar)
        m_network->setCookieJar(jar);

    log(Trace, "transport opening session");
    // first contact padurl to get the session cookie
    m_state = XhrOpenSession;
    get(m_padurl);
}

void XhrClient::request_id() {
    QUrl url = m_baseurl;
    url.setPath(m_baseurl.path() + "socket.io/1/");
    m_state = XhrGetId;
    get(url);
}

void XhrClient::parse_message(const QString & message) {
    int msg_type = message.section(':', 0, 0).toInt();
    QString payload = message.section(':', 3);
    switch (msg_type) {
        case 4: { // json payload
            log(Trace, "received " + message);
            bool ok;
            QVariant decoded = QJson::Parser().parse(payload.toUtf8(), &ok);
            if (!ok) {
                log(Error, "received bad message: " + message);
            } else {
                emit received_message(decoded, payload);
            }
            break;
        }

        case 3: // string payload
            log(Trace, "received " + message);
            emit received_message(payload, payload);
            break;

        case 0: // disconnect
            log(Warning, "received disconnect message " + message);
            m_state = XhrDisconnected;
            emit disconnected();
            break;

        case 1: // connect
        case 8: // noop
            log(Trace, "received " + message);
            break;

        default:
            log(Info, "received " + message);
            break;
    }
}

void XhrClient::get_reply() {
    if (!m_receive)
        return;
    QString reply = QString::fromUtf8(m_receive->readAll());
    m_receive->deleteLater();
    m_receive = 0;

    switch (m_state) {
        case XhrOpenSession:
            request_id();
            break;

        case XhrGetId:
            m_id = reply.section(':', 0, 0);
            if (m_id == "") {
                log(Error, QString("xhr init error: ") + reply);
                QTimer::singleShot(START_RETRY_SECS * 1000,
                                   this, SLOT(start()));
                m_state = XhrInit;
            } else {
                log(Info, QString("xhr init ") + reply);
                m_state = XhrReceiving;
                emit ready();
            }
            break;

        case XhrReceiving:
            if (reply[0] == MULTIMSG) {
                int i = 1;
                while (i < reply.length()) {
                    int nextsep = reply.indexOf(MULTIMSG, i);
                    int length = reply.mid(i, nextsep - i).toInt();
                    parse_message(reply.mid(nextsep + 1, length));
                    i = nextsep + 1 + length + 1;
                }
            } else {
                parse_message(reply);
            }
            break;

        default:
            log(Error, "unexpected message: " + reply);
            m_state = XhrDisconnected;
            emit disconnected();
            break;
    }

    if (m_state == XhrReceiving && m_id != "") {
        QUrl url(m_baseurl);
        url.setPath(m_baseurl.path() + "socket.io/1/xhr-polling/" + m_id);
        get(url);
    }
}

void XhrClient::error(QNetworkReply::NetworkError) {
    if (!m_receive)
        return;
    log(Error, "HTTP GET error: " + m_receive->errorString());
    m_receive->deleteLater();
    m_receive = 0;
    if (m_id == "") {
        QTimer::singleShot(START_RETRY_SECS * 1000, this, SLOT(start()));
        m_state = XhrInit;
    } else {
        m_state = XhrDisconnected;
        emit disconnected();
    }
}

void XhrClient::send_error(QNetworkReply::NetworkError code) {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply)
        log(Error, "HTTP POST error: " + reply->errorString());
    else
        log(Error, "HTTP POST error (unknown object) code "
                   + QString::number(code));
    // TODO: notify Client? should it affect the state machine?
}

QString XhrClient::getCookie(const QString & name) const {
    QByteArray comparableName = name.toUtf8();
    Q_FOREACH(QNetworkCookie cookie, 
              m_network->cookieJar()->cookiesForUrl(m_baseurl)) {
        if (cookie.name() == comparableName)
            return QString::fromUtf8(cookie.value());
    }
    return QString();
}

void XhrClient::setCookie(const QString & name, const QString & value) {
    QList<QNetworkCookie> cookies;
    cookies.append(QNetworkCookie(name.toUtf8(), value.toUtf8()));
    m_network->cookieJar()->setCookiesFromUrl(cookies, m_baseurl);
}
