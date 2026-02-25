/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#ifndef NX_LOGGER_H
#define NX_LOGGER_H

#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Initializers/ConsoleInitializer.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

// Convenience macros matching plog severity levels:
//   LOGT  - trace (verbose)
//   LOGD  - debug
//   LOGI  - info
//   LOGW  - warning
//   LOGE  - error
//   LOGF  - fatal

#define LOGT PLOG_VERBOSE
#define LOGD PLOG_DEBUG
#define LOGI PLOG_INFO
#define LOGW PLOG_WARNING
#define LOGE PLOG_ERROR
#define LOGF PLOG_FATAL

namespace nx {

/// Call once from main() before any logging.
/// @param severity  Minimum severity to log (default: plog::info)
inline void initLogger(plog::Severity severity = plog::info) {
	static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
	plog::init(severity, &consoleAppender);
}

} // namespace nx

#endif // NX_LOGGER_H
