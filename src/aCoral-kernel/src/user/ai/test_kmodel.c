/* Copyright 2023 SPG.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "acoral.h"
#include "w25qxx.h"
#include "fpioa.h"
#include "spi.h"
#include "sysctl.h"
#include "dmac.h"
#include "kpu.h"
#include "plic.h"
#include "uarths.h"
#include "lcd.h"
#include "region_layer.h"
#include <stdio.h>
#include <float.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* 模型类型定义 */
typedef enum {
    MODEL_TYPE_CLASSIFICATION = 0,  // 分类模型
    MODEL_TYPE_DETECTION = 1,       // 检测模型
    MODEL_TYPE_FEATURE = 2          // 特征提取模型
} model_type_t;

/* 模型配置结构体 */
typedef struct {
    model_type_t type;              // 模型类型
    uint32_t model_size;            // 模型大小
    uint32_t flash_addr;            // Flash中的模型地址
    uint32_t input_width;           // 输入图像宽度
    uint32_t input_height;          // 输入图像高度
    uint32_t input_channels;        // 输入通道数
    float input_mean;               // 输入均值
    float input_std;                // 输入标准差
    void *model_data;               // 模型数据指针
    kpu_model_context_t task;       // KPU任务上下文
    volatile uint32_t ai_done_flag; // AI完成标志
} model_config_t;

/* 模型输出处理回调函数类型 */
typedef void (*output_callback_t)(void *output, size_t output_size, void *user_data);

/* 全局变量 */
static uint8_t *g_image_buffer = NULL;
static model_config_t g_model_config;

/**
 * @brief 初始化系统时钟
 */
static void init_system_clock(void)
{
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);
}

/**
 * @brief AI推理完成回调函数
 * @param userdata 用户数据指针
 */
static void ai_done(void* userdata)
{
    g_model_config.ai_done_flag = 1;
}

/**
 * @brief 初始化模型配置
 * @param config 模型配置结构体
 * @return 成功返回0，失败返回-1
 */
static int init_model_config(model_config_t *config)
{
    if (!config) return -1;

    /* 分配模型数据内存 */
    config->model_data = acoral_malloc(config->model_size);
    if (!config->model_data) {
        ACORAL_LOG_ERROR("Failed to allocate model memory\n");
        return -1;
    }

    /* 分配图像缓冲区 */
    size_t image_size = config->input_width * config->input_height * config->input_channels;
    g_image_buffer = acoral_malloc(image_size);
    if (!g_image_buffer) {
        ACORAL_LOG_ERROR("Failed to allocate image buffer\n");
        acoral_free(config->model_data);
        return -1;
    }

    /* 初始化Flash并读取模型 */
    w25qxx_init(3, 0);
    w25qxx_enable_quad_mode();
    w25qxx_read_data(config->flash_addr, config->model_data, config->model_size, W25QXX_QUAD_FAST);

    /* 加载模型 */
    if (kpu_load_kmodel(&config->task, config->model_data) != 0) {
        ACORAL_LOG_ERROR("Failed to load model\n");
        acoral_free(config->model_data);
        acoral_free(g_image_buffer);
        return -1;
    }

    return 0;
}

/**
 * @brief 从文件加载图像数据
 * @param filename 图像文件路径
 * @param config 模型配置
 * @return 成功返回0，失败返回-1
 */
static int load_image_data(const char* filename, model_config_t *config)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        ACORAL_LOG_ERROR("Cannot open image file: %s\n", filename);
        return -1;
    }

    size_t image_size = config->input_width * config->input_height * config->input_channels;
    size_t read_size = fread(g_image_buffer, 1, image_size, fp);
    fclose(fp);

    if (read_size != image_size) {
        ACORAL_LOG_ERROR("Invalid image size\n");
        return -1;
    }

    return 0;
}

/**
 * @brief 运行模型推理
 * @param config 模型配置
 * @param callback 输出处理回调函数
 * @param user_data 用户数据
 * @return 成功返回0，失败返回-1
 */
static int run_model_inference(model_config_t *config, output_callback_t callback, void *user_data)
{
    if (!config || !callback) return -1;

    /* 运行模型 */
    config->ai_done_flag = 0;
    if (kpu_run_kmodel(&config->task, g_image_buffer, DMAC_CHANNEL5, ai_done, NULL) != 0) {
        ACORAL_LOG_ERROR("Failed to run model\n");
        return -1;
    }

    /* 等待推理完成 */
    while (!config->ai_done_flag) {
        msleep(1);
    }

    /* 获取输出并处理 */
    void *output;
    size_t output_size;
    if (kpu_get_output(&config->task, 0, (uint8_t **)&output, &output_size) != 0) {
        ACORAL_LOG_ERROR("Failed to get model output\n");
        return -1;
    }

    callback(output, output_size, user_data);
    return 0;
}

/**
 * @brief 清理模型资源
 * @param config 模型配置
 */
static void cleanup_model(model_config_t *config)
{
    if (config) {
        if (config->model_data) {
            acoral_free(config->model_data);
        }
        if (g_image_buffer) {
            acoral_free(g_image_buffer);
        }
    }
}

/**
 * @brief 通用模型测试主函数
 * @param config 模型配置
 * @return 成功返回0，失败返回-1
 */
int test_kmodel(model_config_t *config)
{
    if (!config) return -1;

    /* 初始化系统 */
    uarths_init();
    plic_init();
    init_system_clock();
    sysctl_enable_irq();

    /* 初始化模型 */
    if (init_model_config(config) != 0) {
        return -1;
    }

    /* 开始交互式测试 */
    ACORAL_LOG_TRACE("Starting Interactive Model Testing\n");
    printf("\n欢迎使用模型测试系统！\n");
    printf("请输入图像路径（输入q退出）\n");
    printf("注意：图片必须是%dx%d的RGB格式\n", 
           config->input_width, config->input_height);

    char image_path[256];
    while(1) {
        printf("\n请输入图片路径: ");
        fgets(image_path, sizeof(image_path), stdin);
        image_path[strcspn(image_path, "\n")] = 0;

        if (strcmp(image_path, "q") == 0) {
            break;
        }

        /* 加载并处理图像 */
        if (load_image_data(image_path, config) != 0) {
            continue;
        }

        /* 运行模型推理 */
        if (run_model_inference(config, NULL, NULL) != 0) {
            continue;
        }
    }

    /* 清理资源 */
    cleanup_model(config);
    ACORAL_LOG_TRACE("Model Testing Complete\n");

    return 0;
} 