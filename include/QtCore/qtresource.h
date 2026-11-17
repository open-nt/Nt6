// MIT License - see LICENSE
#pragma once

// Registers the resources compiled from the .qrc file `name` (its base name, or the --name given to
// ntrcc). Resources compiled into an executable register themselves when the program starts; the
// macro is needed for resources in a static library. Call it from a function in the global
// namespace, such as main(). See "The Nt Resource System" in the documentation.
#define Q_INIT_RESOURCE(name)                                                                                          \
    do {                                                                                                               \
        extern int qInitResources_##name();                                                                            \
        qInitResources_##name();                                                                                       \
    } while (false)

// Unregisters the resources compiled from the .qrc file `name`, once for each Q_INIT_RESOURCE().
#define Q_CLEANUP_RESOURCE(name)                                                                                       \
    do {                                                                                                               \
        extern int qCleanupResources_##name();                                                                         \
        qCleanupResources_##name();                                                                                    \
    } while (false)
