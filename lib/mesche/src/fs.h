#ifndef mesche_fs_h
#define mesche_fs_h

#include <stdbool.h>

bool mesche_fs_path_exists_p(const char *fs_path);
bool mesche_fs_path_absolute_p(const char *fs_path);
char *mesche_fs_resolve_path(const char *fs_path);
char *mesche_fs_file_read_all(const char *file_path);

#endif
