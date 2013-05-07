#include <QCoreApplication>
#include <QRegExp>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <QtGlobal>

#include <cstdlib>  // for exit()
#include <unistd.h>  // for getpass() and usleep()
#include <time.h>

#include "Client.h"
#include "Logger.h"

static QString clientspec;
static int verbosity = Logger::Info;
static int duration = 300;  // run 300 seconds (5 minutes)
static QUrl padurl;   // etherdraw URL to connect to (drawing must exist)
// Authorization for etherdraw connection
static QString username;
static QString password;


void parse_arguments() {
    QStringList args = qApp->arguments();

    int i;
    for (i = 1; i < args.length(); i++) {
        QString arg = args[i];

        if (!arg.startsWith('-'))
            break;

        if (arg == "--") {
            i++;
            break;
        }

        QString value;

        if (arg.startsWith('-') && arg.contains('=')) {
            value = arg.section('=', 1);
            arg = arg.section('=', 0, 0);
        } else {
            if (i+1 == args.length()) {
                qCritical("No value for option %s", qPrintable(arg));
                exit(2);
            }
            value = args[i+1];
            i += 1;
        }

        if (arg == "--clients")
            clientspec = value;
        else if (arg == "--duration")
            duration = value.toInt();
        else if (arg == "--verbosity")
            verbosity = value.toInt();
        else if (arg == "--user")
            username = value;
    }

    if (i == args.length()) {
        qCritical("Usage: %s [options] URL", qPrintable(args[0]));
        exit(2);
    }
    padurl = args[i];
    if (args[i].section('/', -2, -2) != "d") {
        qCritical("draw url must end with /d/DRAWNAME to be valid");
        exit(2);
    }

    if (username != "") {
        // Blatantly break portability because there is no sane
        // cross-platform way to do this in a console app.
        password = getpass("password: ");
    }

    if (clientspec.isEmpty()) {
        clientspec = "lurk:30";
    } else if (clientspec.toInt() != 0) {
        clientspec.prepend("lurk:");
    } else if (!QRegExp("\\w+:\\d+(,\\w+:\\d+)*").exactMatch(clientspec)) {
        qCritical("clients value must be numeric or like foo:10,bar:20");
        exit(2);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qsrand(time(0));

    parse_arguments();

    Logger::set_global_level(verbosity);

    padurl.setUserName(username);
    padurl.setPassword(password);

    Q_FOREACH(QString spec, clientspec.split(',')) {
        QString logic = spec.section(':', 0, 0);
        QString clientid = logic[0].toUpper();
        int clients = spec.section(':', 1).toInt();
        for (int i = 1; i <= clients; i++) {
            Client *cl = new Client(padurl, clientid + QString::number(i));
            cl->setLogic(logic);
            cl->connect(&app, SIGNAL(aboutToQuit()), SLOT(end()));
            cl->start();
            usleep(1000); // give XhrClient a unique microsecond-based url
        }
    }

    QTimer::singleShot(duration * 1000, &app, SLOT(quit()));
    return app.exec();
}
