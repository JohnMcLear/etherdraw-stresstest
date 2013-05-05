#ifndef CLIENT_H
#define CLIENT_H

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include "Logger.h"
#include "Pad.h"

class XhrClient;

class Client : public QObject, private Logger {
    Q_OBJECT

  public:
    Client(QUrl padurl, const QString & name, QObject *parent = 0);
    virtual ~Client();
    void start();

    enum ClientState {
      CsCreated, CsStarting, CsGettingVars, CsActive, CsDisconnected
    };
    static QString stateName(ClientState state);

    void setLogic(const QString & logic);

  protected slots:
    void transportReady();
    void transportDisconnected();
    void received_message(QVariant message, QString orig_text);

  private slots:
    void end();
    void kick();

  private:
    void changeState(ClientState state);
    void kickAfter(int secs);
    void kickAfter(int secs_min, int secs_max);
    int elapsedSecs();
    void getClientVars(QVariantMap vars);
    void sendUserInfo();
    void sendBadFollow();
    void sendChangeset(const QString & changeset,
                       const QList<Attribute> & attributes);
    void makeRandomEdit();

    ClientState m_state;
    QString m_logic;
    QUrl m_padurl;
    XhrClient *m_xhr;
    QTimer m_kick;
    QElapsedTimer m_elapsed;
    QString m_pad_id;
    // filled in from CLIENT_VARS message
    QString m_author_id;
    QString m_author_name;  // constructor fills in a default
    QString m_color;
    Pad m_pad;
};

#endif
