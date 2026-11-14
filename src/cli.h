#pragma once

#include <QStringList>

// Returns true if the command-line arguments describe a headless CLI command
// (check, voices, export, skill, --help, --version) rather than GUI launch.
bool isCliInvocation(const QStringList &args);

// Runs the CLI command and returns the process exit code.
int runCli(int argc, char *argv[]);
