#include "FileLogger.h"
#include <sstream>
#include <iomanip>

FileLogger::FileLogger(const wchar_t* aPath)
{
    File.open(aPath);
}

FileLogger::~FileLogger()
{
    File.flush();
    File.close();
}

void FileLogger::LogMessage(LogEntry aLogEntry)
{
    MessageMutex.lock();

    File << aLogEntry.ToString();
    File.flush();

    MessageMutex.unlock();
}