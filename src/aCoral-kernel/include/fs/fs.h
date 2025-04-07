// src/aCoral-kernel/include/fs/fs.h
#ifndef ACORAL_FS_H
#define ACORAL_FS_H

#include "acoral.h"
#include "types.h"

// 文件系统错误码
#define ACORAL_ERR_FS_MOUNT    (-1)
#define ACORAL_ERR_FS_UNMOUNT  (-2)
#define ACORAL_ERR_FS_READ     (-3)
#define ACORAL_ERR_FS_WRITE    (-4)
#define ACORAL_ERR_FS_OPEN     (-5)
#define ACORAL_ERR_FS_CLOSE    (-6)

// 文件系统操作接口
struct acoral_fs_ops {
    acoral_err (*init)(void);
    acoral_32 (*open)(const char *path, const char *mode);
    acoral_32 (*close)(acoral_32 fd);
    acoral_32 (*read)(void *buf, acoral_32 size, acoral_32 count, acoral_32 fd);
    acoral_32 (*write)(const void *buf, acoral_32 size, acoral_32 count, acoral_32 fd);
};

// 文件系统初始化
acoral_err acoral_fs_init(void);

// 文件操作接口
acoral_32 acoral_fopen(const char *path, const char *mode);
acoral_32 acoral_fclose(acoral_32 fd);
acoral_32 acoral_fread(void *buf, acoral_32 size, acoral_32 count, acoral_32 fd);
acoral_32 acoral_fwrite(const void *buf, acoral_32 size, acoral_32 count, acoral_32 fd);

#endif