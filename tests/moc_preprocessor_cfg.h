// MIT License - see LICENSE
// Macros for moc_preprocessor.h, which ntmoc reads through its #include.
#pragma once

#define NT_MOC_TEST_HAVE_B 1
#define NT_MOC_TEST_VERSION(major, minor) ((major) * 100 + (minor))
#define NT_MOC_TEST_CURRENT NT_MOC_TEST_VERSION(6, 2)
#define NT_MOC_TEST_CAT(a, b) a##b
#define NT_MOC_TEST_FEATURE_x 1
