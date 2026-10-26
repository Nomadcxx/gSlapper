#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <sys/stat.h>

#include "../src/process_utils.h"

int main(void) {
    char path[PATH_MAX];
    assert(executable_path(path, sizeof(path)) == 0);
    struct stat info;
    assert(stat(path, &info) == 0 && S_ISREG(info.st_mode));
    char short_path[2];
    assert(executable_path(short_path, sizeof(short_path)) == -1);
    assert(errno == ENAMETOOLONG);
    assert(executable_path(path, 0) == -1 && errno == EINVAL);
    assert(executable_path(NULL, sizeof(path)) == -1 && errno == EINVAL);
    return 0;
}
