// MIT License - see LICENSE
// A shared library for the QLibrary tests (tests/test_ipc.cpp). It uses nothing of Nt, so that it need not be
// linked with the static libraries.
#if defined(_MSC_VER)
#  define NT_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define NT_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace {
int g_calls = 0;
}

extern "C" NT_PLUGIN_EXPORT int nt_plugin_add(int a, int b) { return a + b; }

// Counts its calls for as long as the library stays in memory.
extern "C" NT_PLUGIN_EXPORT int nt_plugin_calls() { return ++g_calls; }

extern "C" NT_PLUGIN_EXPORT const char *nt_plugin_name() { return "nt_test_plugin"; }
