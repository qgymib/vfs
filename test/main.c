/*
 * ASAN in windows does not detect memory leak, so we use CRT to do this work.
 * @note It does not work if ASAN is enabled.
 * @see https://learn.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-170
 * @see https://learn.microsoft.com/en-us/cpp/c-runtime-library/find-memory-leaks-using-the-crt-library?view=msvc-170
 */
#if defined(_WIN32)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "test.h"

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    static cutest_hook_t hook;
    hook.before_all_test = vfs_test_init;
    hook.after_all_test = vfs_test_exit;

    return cutest_run_tests(argc, argv, stdout, &hook);
}
