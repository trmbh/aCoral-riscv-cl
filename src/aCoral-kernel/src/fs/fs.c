// src/aCoral-kernel/src/fs/fs.c
#include "fs.h"
#include "fatfs.h"

static const struct acoral_fs_ops *current_fs = &fatfs_ops;

acoral_err acoral_fs_init(void) {
    return current_fs->init();
}

acoral_32 acoral_fopen(const char *path, const char *mode) {
    return current_fs->open(path, mode);
}

// 实现其他接口...
acoral_32 acoral_fclose(acoral_32 fd) {
    return current_fs->close(fd);
}

acoral_32 acoral_fread(void *buf, acoral_32 size, acoral_32 count, acoral_32 fd) {
    return current_fs->read(buf, size, count, fd);
}

acoral_32 acoral_fwrite(const void *buf, acoral_32 size, acoral_32 count, acoral_32 fd) {
    return current_fs->write(buf, size, count, fd);
}