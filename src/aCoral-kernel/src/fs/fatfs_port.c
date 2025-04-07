// src/aCoral-kernel/src/fs/fatfs_port.c
#include "fs/fatfs.h"
#include "fs/fs.h"
#include <string.h>
#include "diskio.h"

static FATFS fs;
static FIL file_table[ACORAL_MAX_FILES];

// 函数声明
static acoral_32 fatfs_open(const char *path, const char *mode);
static acoral_32 fatfs_close(acoral_32 fd);
static acoral_32 fatfs_read(void *buf, acoral_32 size, acoral_32 count, acoral_32 fd);
static acoral_32 fatfs_write(const void *buf, acoral_32 size, acoral_32 count, acoral_32 fd);

// FatFs操作接口
const struct acoral_fs_ops fatfs_ops = {
    .init = fatfs_init,
    .open = fatfs_open,
    .close = fatfs_close,
    .read = fatfs_read,
    .write = fatfs_write
};

acoral_err fatfs_init(void) {
    FRESULT res;
    
    // 挂载文件系统
    res = f_mount(&fs, "", 1);
    return (res == FR_OK) ? ACORAL_ERR_NONE : ACORAL_ERR_FS_MOUNT;
}

// 实现其他接口...
static acoral_32 fatfs_open(const char *path, const char *mode) {
    BYTE flags = 0;
    int fd;
    
    // 转换模式字符串为FatFs标志
    if (strchr(mode, 'r')) flags |= FA_READ;
    if (strchr(mode, 'w')) flags |= FA_WRITE | FA_CREATE_ALWAYS;
    if (strchr(mode, 'a')) flags |= FA_WRITE | FA_OPEN_APPEND;
    if (strchr(mode, '+')) flags |= FA_READ | FA_WRITE;
    
    // 查找空闲文件描述符
    for (fd = 0; fd < ACORAL_MAX_FILES; fd++) {
        if (file_table[fd].obj.fs == NULL) break;
    }
    if (fd >= ACORAL_MAX_FILES) return -1;
    
    // 打开文件
    FRESULT res = f_open(&file_table[fd], path, flags);
    return (res == FR_OK) ? fd : -1;
}

static acoral_32 fatfs_close(acoral_32 fd) {
    if (fd < 0 || fd >= ACORAL_MAX_FILES) return -1;
    if (file_table[fd].obj.fs == NULL) return -1;
    
    FRESULT res = f_close(&file_table[fd]);
    return (res == FR_OK) ? 0 : -1;
}

static acoral_32 fatfs_read(void *buf, acoral_32 size, acoral_32 count, acoral_32 fd) {
    if (fd < 0 || fd >= ACORAL_MAX_FILES) return -1;
    if (file_table[fd].obj.fs == NULL) return -1;
    
    UINT br;
    FRESULT res = f_read(&file_table[fd], buf, size * count, &br);
    return (res == FR_OK) ? br : -1;
}

static acoral_32 fatfs_write(const void *buf, acoral_32 size, acoral_32 count, acoral_32 fd) {
    if (fd < 0 || fd >= ACORAL_MAX_FILES) return -1;
    if (file_table[fd].obj.fs == NULL) return -1;
    
    UINT bw;
    FRESULT res = f_write(&file_table[fd], buf, size * count, &bw);
    return (res == FR_OK) ? bw : -1;
}