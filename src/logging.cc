#include <cstdlib>

#include <pystring.h>

#include "logging.h"

std::shared_ptr<spdlog::logger> Log() {
  static std::shared_ptr<spdlog::logger> log = []() {
    auto log = spdlog::get("console");
    if (!log) {
      // By default log to stdout
      log = spdlog::stdout_logger_st("console", true);
      log->set_pattern("[%Y%m%d %H:%M:%S.%e] %l | %v");
      log->info("Logging to default console");

      char* log_level_raw = getenv("LOGLEVEL");
      if (log_level_raw)
      {
        std::string log_level = pystring::lower(std::string{log_level_raw});
        if      (log_level == "debug") { log->set_level(spdlog::level::debug); }
        else if (log_level == "info")  { log->set_level(spdlog::level::info ); }
        else if (log_level == "warn")  { log->set_level(spdlog::level::warn ); }
        else if (log_level == "error") { log->set_level(spdlog::level::err  ); }
        else                           { }
      }
    }
    return log;
  }();

  return log;
}

