#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <string_view>

#include "inttypes.h"

__attribute__((format(printf, 4, 5)))
void logError(const char *filename, const char *content, int64_t position, const char *format, ...);

__attribute__((format(printf, 5, 6)))
void logError(const char *filename, const char *content, int64_t start, int64_t end, const char *format, ...);

std::string xml_encode(std::string_view data);

#endif
