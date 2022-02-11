#include "Logger.h"

#include <iostream>

using namespace std;

// 单例模式
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}
// 设置日志级别
void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}
// 写日志 [级别信息] time : msg
void Logger::log(std::string msg)
{
    switch (logLevel_)
    {
    case INFO:
        cout << "[INFO]" ; 
        break;
    case ERROR:
        cout << "[ERROR]" ; 
        break;
    case FATAL:
        cout << "[FATAL]" ; 
        break;
    case DEBUG:
        cout << "[DEBUG]" ; 
        break;
    default:
        break;
    }

    cout << " print time " << ": " << msg << endl;
}