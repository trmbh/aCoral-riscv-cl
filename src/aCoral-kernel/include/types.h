/**
 * @file types.h
 * @brief aCoral基本类型定义
 */
#ifndef ACORAL_TYPES_H
#define ACORAL_TYPES_H

typedef int acoral_err;
typedef int acoral_32;
typedef unsigned int acoral_u32;
typedef short acoral_16;
typedef unsigned short acoral_u16;
typedef char acoral_8;
typedef unsigned char acoral_u8;
typedef long long acoral_64;
typedef unsigned long long acoral_u64;

/* 错误码定义 */
#define ACORAL_ERR_NONE      0  /* 无错误 */
#define ACORAL_ERR_FAIL     -1  /* 通用错误 */

/* 文件系统相关配置 */
#define ACORAL_MAX_FILES    16  /* 最大打开文件数 */

#endif 