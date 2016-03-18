#ifndef __LOG_H__
#define __LOG_H__

// #ifdef _DEBUG

#include <sstream>
#include <string>
#include <stdio.h>

inline String NowTime();

enum TLogLevel {logERROR, logWARNING, logINFO, logDEBUG, logDEBUG1, logDEBUG2, logDEBUG3, logDEBUG4};

template <typename T>
class Log
{
public:
    Log();
    virtual ~Log();
    OStringStream& Get(TLogLevel level = logINFO);
public:
    static TLogLevel& ReportingLevel();
    static String ToString(TLogLevel level);
    static TLogLevel FromString(const String& level);
protected:
    OStringStream os;
private:
    Log(const Log&);
    Log& operator =(const Log&);
};

template <typename T>
Log<T>::Log()
{
}

template <typename T>
OStringStream& Log<T>::Get(TLogLevel level)
{
    os << L"- " << NowTime();
    os << L" " << ToString(level) << L": ";
    os << String(level > logDEBUG ? level - logDEBUG : 0, '\t');
    return os;
}

template <typename T>
Log<T>::~Log()
{
    os << std::endl;
    T::Output(os.str());
}

template <typename T>
TLogLevel& Log<T>::ReportingLevel()
{
    static TLogLevel reportingLevel = logDEBUG4;
    return reportingLevel;
}

template <typename T>
String Log<T>::ToString(TLogLevel level)
{
	static const TCHAR* const buffer[] = {L"ERROR", L"WARNING", L"INFO", L"DEBUG", L"DEBUG1", L"DEBUG2", L"DEBUG3", L"DEBUG4"};
    return buffer[level];
}

template <typename T>
TLogLevel Log<T>::FromString(const String& level)
{
    if (level == L"DEBUG4")
        return logDEBUG4;
    if (level == L"DEBUG3")
        return logDEBUG3;
    if (level == L"DEBUG2")
        return logDEBUG2;
    if (level == L"DEBUG1")
        return logDEBUG1;
    if (level == L"DEBUG")
        return logDEBUG;
    if (level == L"INFO")
        return logINFO;
    if (level == L"WARNING")
        return logWARNING;
    if (level == L"ERROR")
        return logERROR;
    Log<T>().Get(logWARNING) << L"Unknown logging level '" << level << L"'. Using INFO level as default.";
    return logINFO;
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#   if defined (BUILDING_FILELOG_DLL)
#       define FILELOG_DECLSPEC   __declspec (dllexport)
#   elif defined (USING_FILELOG_DLL)
#       define FILELOG_DECLSPEC   __declspec (dllimport)
#   else
#       define FILELOG_DECLSPEC
#   endif // BUILDING_DBSIMPLE_DLL
#else
#   define FILELOG_DECLSPEC
#endif // _WIN32

#ifndef FILELOG_MAX_LEVEL
#define FILELOG_MAX_LEVEL logDEBUG4
#endif

class Output2FILE
{
public:
    static FILE*& Stream();
    static void Output(const String& msg);
};

inline FILE*& Output2FILE::Stream()
{
    static FILE* pStream = stderr;
    return pStream;
}

inline void Output2FILE::Output(const String& msg)
{
    FILE* pStream = Stream();
    if (!pStream)
        return;
    _ftprintf(pStream, L"%s", msg.c_str());
    fflush(pStream);
}

class FILELOG_DECLSPEC FILELog : public Log<Output2FILE> {};
//typedef Log<Output2FILE> FILELog;

#define FILE_LOG(level) \
    if (level > FILELOG_MAX_LEVEL) ;\
    else if (level > FILELog::ReportingLevel() || !Output2FILE::Stream()) ; \
    else FILELog().Get(level)

class Output2DEBUG
{
public:
    static void Output(const String& msg);
};

inline void Output2DEBUG::Output(const String& msg)
{
    OutputDebugString(msg.c_str());
}

class FILELOG_DECLSPEC DEBUGLog : public Log<Output2DEBUG> {};
//typedef Log<Output2DEBUG> DEBUGLog;

#define DEBUG_LOG(level) \
    if (level > FILELOG_MAX_LEVEL) ;\
    else if (level > DEBUGLog::ReportingLevel()) ; \
    else DEBUGLog().Get(level)

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)

#include <windows.h>

inline String NowTime()
{
    const int MAX_LEN = 200;
    TCHAR buffer[MAX_LEN];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, 0, L"HH':'mm':'ss", buffer, MAX_LEN) == 0)
        return L"Error in NowTime()";

    TCHAR result[100] = {0};
    static DWORD first = GetTickCount();
    _stprintf_s(result, L"%s.%03ld", buffer, (long)(GetTickCount() - first) % 1000);
    return result;
}

#else

#include <sys/time.h>

inline String NowTime()
{
    TCHAR buffer[11];
    time_t t;
    time(&t);
    tm r = {0};
    strftime(buffer, sizeof(buffer), "%X", localtime_r(&t, &r));
    struct timeval tv;
    gettimeofday(&tv, 0);
    char result[100] = {0};
    std::sprintf(result, "%s.%03ld", buffer, (long)tv.tv_usec / 1000);
    return result;
}

#endif //WIN32

#else // !_DEBUG

#include <iostream>

// http://stackoverflow.com/a/11826787
#define DEBUG_LOG(level) \
    if (true) {} else std::cerr

// #endif // _DEBUG

#endif //__LOG_H__
