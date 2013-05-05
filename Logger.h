#ifndef LOGGER_H
#define LOGGER_H

#include <QString>

// Mixin class

class Logger {
  public:
    Logger(const QString & name) : m_name(name) { }

    enum levels { Error = 1, Warning, Info, Verbose, Trace };
    static void set_global_level(int level);

    void log(int level, const QString & message) const;

  private:
    static int c_level;
    QString m_name;
};

#endif
