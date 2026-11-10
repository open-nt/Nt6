// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>

// Resolves a path that starts with a search path prefix registered with QDir::setSearchPaths(),
// such as "icons:open.png", to the first existing file in the prefix's search paths, or to the
// candidate in the first search path if none exists. Any other path is returned unchanged.
QString ntResolveSearchPath(const QString &path);
