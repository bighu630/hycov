#pragma once

#include <hyprland/src/debug/log/Logger.hpp>

#define HYCOV_LOG  Hyprutils::CLI::LOG_DEBUG
#define HYCOV_WARN Hyprutils::CLI::LOG_WARN
#define HYCOV_ERR  Hyprutils::CLI::LOG_ERR
#define HYCOV_CRIT Hyprutils::CLI::LOG_CRIT

template <typename... Args>
void hycov_log(Hyprutils::CLI::eLogLevel level, std::format_string<Args...> fmt, Args &&...args)
{
	auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
	Log::logger->log(level, "[hycov] {}", msg);
}
