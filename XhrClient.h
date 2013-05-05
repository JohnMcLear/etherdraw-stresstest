#ifndef XHRCLIENT_H
#define XHRCLIENT_H

#include <QObject>
#include <QNetworkReply>
#include <QUrl>

#include "Logger.h"

class QNetworkAccessManager;

// Simulate the xhr-polling backend of socket.io.js

// xhr polling is named after the XmlHttpRequest object which is
// available in most browsers. Despite its name it doesn't involve
// any XML. socket.io uses it for JSON data.

// The basic idea is that the client keeps a long term HTTP GET request
// open to the server. When the server wants to push a message, it replies
// to this request. The client receives the message and immediately
// opens a new request.

// When the client wants to send a message, it uses an HTTP POST request
// through a different connection.
// The server acknowledges the message immediately, before processing it.
// The reply data, if any, is pushed by the server as above.

// The task of this class is to encapsulate all that, and allow the
// caller to send and receive JSON messages asynchronously.

class XhrClient : public QObject, private Logger {
    Q_OBJECT

  public:
    XhrClient(QUrl padurl, QUrl baseurl,
              const QString & name, QObject *parent = 0);
    virtual ~XhrClient();

    QString getCookie(const QString & name) const;
    void setCookie(const QString & name, const QString & value);


  signals:
    void ready();
    void disconnected();
    void received_message(QVariant message, QString orig_text);

  public slots:
    void start();
    void send(const QVariant & msg);
    void disconnect();

  protected slots:
    void error(QNetworkReply::NetworkError code);
    void send_error(QNetworkReply::NetworkError code);
    void get_reply();
    void authenticate(QNetworkReply *, QAuthenticator *);
    void send_packet(const QByteArray & msg_string);

  private:
    QUrl m_padurl;
    QUrl m_baseurl;
    // Normally one network access manager would be enough for the
    // whole application, but the point here is to simulate multiple
    // clients so they shouldn't share connections.
    QNetworkAccessManager *m_network;
    // socket.io session id received from server; used in url
    QString m_id;
    // the object representing the long-running http connection
    QNetworkReply *m_receive;

    QString m_username;
    QString m_password;

    enum State { XhrInit,
        XhrOpenSession, XhrGetId, XhrReceiving, XhrDisconnected };
    State m_state;

    void get(QUrl url);
    void request_id();
    void parse_message(const QString & message);
};

#endif
