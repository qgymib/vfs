#include "cutest.h"
#include "vfs.h"

TEST(init_and_exit, lifecycle)
{
    vfs_init();
    vfs_exit();
}
