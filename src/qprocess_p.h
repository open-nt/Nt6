// MIT License - see LICENSE
#pragma once

namespace QtPrivate {

// Test hook: with false, QProcess watches children by polling waitpid() instead of a pidfd.
void setProcessPidfdEnabled(bool enabled);

} // namespace QtPrivate
