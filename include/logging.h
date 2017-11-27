#ifndef  LOGGING_H_
#define  LOGGING_H_

#include <memory>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "stacktrace.h"

// Returns a shared pointer to the current default log named "console"
std::shared_ptr<spdlog::logger> Log();

#define STRINGISE(X) #X
#define FANCY_LOG(level, msg, ...) Log()->level("{}:{} | " msg, __FILE__, \
                                               __LINE__, ##__VA_ARGS__)

#define FATAL_THROW(msg, ...)                               \
  PrintStack();                                             \
  throw std::runtime_error(fmt::format(msg, ##__VA_ARGS__))

#define FATAL(...) FANCY_LOG(error, __VA_ARGS__); FATAL_THROW(__VA_ARGS__)
#define ERROR(...) FANCY_LOG(error, __VA_ARGS__); PrintStack()
#define WARN(...)  FANCY_LOG(warn,  __VA_ARGS__)
#define INFO(...)  FANCY_LOG(info,  __VA_ARGS__)
#define DEBUG(...) FANCY_LOG(debug, __VA_ARGS__)

#endif  // #ifndef  LOGGING_H_
