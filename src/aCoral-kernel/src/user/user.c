#include "acoral.h"
#include "user.h"
#include "fs/fs.h"
#include "fs/fatfs.h"
#include <string.h>
#include "log.h"

void user_main(){
    ACORAL_LOG_TRACE("Init Thread -> user_main");
    // test_comm_thread();
    // test_period_thread();
    // test_iris();
    // test_iris_2();
    // test_yolo2();
    // test_dag();
    // test_mnist();
    // test_face();

    // 初始化文件系统
    acoral_err err = acoral_fs_init();
    if (err != ACORAL_ERR_NONE) {
        ACORAL_LOG_ERROR("Failed to init filesystem");
        return;
    }
    
    // 测试文件操作
    acoral_32 fd = acoral_fopen("/test.txt", "w");
    if (fd >= 0) {
        const char *data = "Hello FatFs!\n";
        acoral_fwrite(data, 1, strlen(data), fd);
        acoral_fclose(fd);
    }
}