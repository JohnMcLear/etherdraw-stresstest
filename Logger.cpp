#include "Logger.h"

#include <cstdio>

int Logger::c_level = Logger::Error;

void Logger::set_global_level(int level) {
    c_level = level;
}

void Logger::log(int level, const QString & message) const {
    if (level <= c_level) {
        QString line = m_name + ": " + message;
        if (level == Error)
            line.prepend("ERROR: ");
        else if (level == Warning)
            line.prepend("WARNING: ");
        puts(qPrintable(line));
    }
}

