// src/aCoral-kernel/include/fs/fatfs.h
#ifndef ACORAL_FATFS_H
#define ACORAL_FATFS_H

#include "fs.h"
#include "sem.h"
#include "ff.h"

// FatFs操作接口
extern const struct acoral_fs_ops fatfs_ops;

// FatFs初始化
acoral_err fatfs_init(void);

#endif