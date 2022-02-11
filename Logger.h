#pragma once

#include <string>

#include "noncopyable.h"

/*****************
 * __FILE__ 文件名
 * __FUNCTION__ 函数名
 * __LINE__ 行号
 * 
 * #define func(A) cout << #A; #A表示A这个字符
 * #define func(A) x ## A      ##用于拼接x和A
 * ##__VA_ARGS__              匹配任意参数
 * *************/

// LOG_INFO("%s %d",arg1,arg2)
#define LOG_INFO(logmsgFomat, ...)\
do{ \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(INFO); \
    char buf[1024] = {0}; \
    snprintf(buf, 1024, logmsgFomat, ##__VA_ARGS__); \
    logger.log(buf); \
}while(0) \

// LOG_ERROR("%s %d",arg1,arg2)
#define LOG_ERROR(logmsgFomat, ...)\
do{ \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(ERROR); \
    char buf[1024] = {0}; \
    snprintf(buf, 1024, logmsgFomat, ##__VA_ARGS__); \
    logger.log(buf); \
}while(0)

// LOG_FATAL("%s %d",arg1,arg2)
// 致命错误退出当前进程
#define LOG_FATAL(logmsgFomat, ...)\
do{ \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(FATAL); \
    char buf[1024] = {0}; \
    snprintf(buf, 1024, logmsgFomat, ##__VA_ARGS__); \
    logger.log(buf); \
    exit(-1); \
}while(0)

// LOG_DEBUG("%s %d",arg1,arg2)
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFomat, ...)\
do{ \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(DEBUG); \
    char buf[1024] = {0}; \
    snprintf(buf, 1024, logmsgFomat, ##__VA_ARGS__); \
    logger.log(buf); \
}while(0)
#else
    #define LOG_DEBUG(logmsgFomat, ...)
#endif

// 定义日志的级别 INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // coredump信息
    DEBUG  // 调试信息
};

class Logger : noncopyable
{
public:
    // 单例模式
    static Logger& instance();
    // 设置日志级别
    void setLogLevel(int level);
    // 写日志
    void log(std::string msg);

private:
    int logLevel_;
};
