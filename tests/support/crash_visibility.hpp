#pragma once

// Makes a crashing headless test say why, and keeps it from blocking on a dialog.
//
// - spdlog buffers, so a Flecs abort or fast-fail used to exit with no log line. Warnings and above flush at once.
// - An uncaught exception reaches std::terminate, which prints the exception text before aborting.
// - On Windows the "has stopped working" and CRT assert dialogs wait for a person; with them off the process exits
//   with its status code, and the CRT writes the assert text to stderr instead.

#include <cstdio>
#include <cstdlib>
#include <exception>

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

namespace test_support {

inline void InstallCrashVisibility() {
	spdlog::flush_on(spdlog::level::warn);

	std::set_terminate([] {
		try {
			if (const auto current = std::current_exception()) {
				std::rethrow_exception(current);
			}
			std::fputs("[terminate] called with no active exception\n", stderr);
		}
		catch (const std::exception& e) {
			std::fprintf(stderr, "[terminate] uncaught exception: %s\n", e.what());
		}
		catch (...) {
			std::fputs("[terminate] uncaught non-standard exception\n", stderr);
		}
		std::abort();
	});

#ifdef _WIN32
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	for (const int report_type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
		_CrtSetReportMode(report_type, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(report_type, _CRTDBG_FILE_STDERR);
	}
#endif
}

} // namespace test_support
