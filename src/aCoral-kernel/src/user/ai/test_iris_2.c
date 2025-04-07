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
#include <stdio.h>
#include <float.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* 模型相关参数定义 */
#define KMODEL_SIZE (720)
#define PLL0_OUTPUT_FREQ 800000000UL
#define PLL1_OUTPUT_FREQ 400000000UL
#define PLL2_OUTPUT_FREQ 45158400UL
#define MAX_PATH_LENGTH 256

/* 图像相关参数 */
#define IMAGE_WIDTH 224
#define IMAGE_HEIGHT 224
#define IMAGE_CHANNELS 3

/* 全局变量声明 */
static uint8_t *model_data;
static kpu_model_context_t task1;
static volatile uint32_t g_ai_done_flag_iris;
static float features[4];  // 用于存储提取的特征
static uint8_t *image_data = NULL;  // 用于存储图像数据

/* 分类标签 */
static const char *labels[] = { "setosa", "versicolor", "virginica" };

/**
 * @brief AI推理完成回调函数
 * @param userdata 用户数据指针
 */
static void ai_done(void* userdata)
{
    g_ai_done_flag_iris = 1;
    
    float *output_features;
    size_t count;
    kpu_get_output(&task1, 0, (uint8_t **)&output_features, &count);
    count /= sizeof(float);

    size_t i;
    for (i = 0; i < count; i++)
    {
        if (i % 64 == 0)
            printf("\n");
        printf("%f, ", output_features[i]);
    }

    printf("\n");
}

/**
 * @brief 找出数组中最大值的索引
 * @param src 输入数组
 * @param count 数组长度
 * @return 最大值的索引
 */
static size_t argmax(const float *src, size_t count)
{
    float max = FLT_MIN;
    size_t max_id = 0, i;
    for (i = 0; i < count; i++)
    {
        if (src[i] > max)
        {
            max = src[i];
            max_id = i;
        }
    }

    return max_id;
}

/**
 * @brief 初始化系统时钟
 */
static void init_system_clock(void)
{
    /* Set CPU and AI clk */
    sysctl_pll_set_freq(SYSCTL_PLL0, PLL0_OUTPUT_FREQ);
    sysctl_pll_set_freq(SYSCTL_PLL1, PLL1_OUTPUT_FREQ);
    sysctl_pll_set_freq(SYSCTL_PLL2, PLL2_OUTPUT_FREQ);
}

/**
 * @brief 从图像中提取鸢尾花特征
 * @param image_data 图像数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param features 输出特征数组
 * @return 成功返回0，失败返回-1
 */
static int extract_iris_features(uint8_t *image_data, int width, int height, float *features) {
    // 这里应该实现图像处理算法来提取鸢尾花的四个特征
    // 1. 图像预处理（去噪、增强等）
    // 2. 边缘检测找到花瓣和花萼
    // 3. 测量并计算特征值
    
    // 目前使用简化版本，仅作为示例
    // 实际应用中需要实现完整的图像处理算法
    
    // 临时使用图像的一些基本统计特征作为示例
    float sum_r = 0, sum_g = 0, sum_b = 0;
    int pixels = width * height;
    
    for(int i = 0; i < pixels; i++) {
        sum_r += image_data[i];
        sum_g += image_data[i + pixels];
        sum_b += image_data[i + pixels * 2];
    }
    
    // 使用颜色分布特征作为临时替代
    features[0] = sum_r / pixels / 255.0f * 7.0f;  // 映射到大约0-7范围
    features[1] = sum_g / pixels / 255.0f * 4.0f;  // 映射到大约0-4范围
    features[2] = sum_b / pixels / 255.0f * 6.0f;  // 映射到大约0-6范围
    features[3] = (sum_r + sum_g + sum_b) / (pixels * 3) / 255.0f * 2.0f;  // 映射到大约0-2范围
    
    ACORAL_LOG_INFO("提取的特征值: %.2f %.2f %.2f %.2f\n", 
                    features[0], features[1], features[2], features[3]);
    
    return 0;
}

/**
 * @brief 从文件读取图像数据
 * @param filename 图像文件路径
 * @param data 输出图像数据缓冲区
 * @return 成功返回0，失败返回-1
 */
static int load_image_data(const char* filename, uint8_t* data) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        ACORAL_LOG_ERROR("Cannot open image file: %s\n", filename);
        return -1;
    }

    size_t read_size = fread(data, 1, IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_CHANNELS, fp);
    fclose(fp);

    if (read_size != IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_CHANNELS) {
        ACORAL_LOG_ERROR("Invalid image size\n");
        return -1;
    }

    return 0;
}

/**
 * @brief 鸢尾花分类测试主函数（图像输入版）
 * @return 正常运行返回0，错误返回-1
 */
int test_iris_2(void)
{
    /* 初始化串口和中断 */
    uarths_init();
    plic_init();

    /* 初始化系统时钟 */
    init_system_clock();

    /* 分配内存 */
    model_data = (uint8_t *)acoral_malloc(KMODEL_SIZE);
    image_data = (uint8_t *)acoral_malloc(IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_CHANNELS);
    if (model_data == NULL || image_data == NULL) {
        ACORAL_LOG_ERROR("Failed to allocate memory\n");
        if (model_data) acoral_free(model_data);
        if (image_data) acoral_free(image_data);
        return -1;
    }

    /* 初始化Flash并读取模型 */
    ACORAL_LOG_TRACE("Flash Init Start\n");
    w25qxx_init(3, 0);
    w25qxx_enable_quad_mode();
    w25qxx_read_data(0xC00000, model_data, KMODEL_SIZE, W25QXX_QUAD_FAST);

    /* 加载并解析模型 */
    ACORAL_LOG_TRACE("Loading KPU Model\n");
    if (kpu_load_kmodel(&task1, model_data) != 0)
    {
        ACORAL_LOG_ERROR("Cannot load kmodel.\n");
        acoral_free(model_data);
        acoral_free(image_data);
        return -1;
    }

    /* 启用全局中断 */
    sysctl_enable_irq();

    /* 开始处理 */
    ACORAL_LOG_TRACE("Starting Interactive Iris Classification (Image Input)\n");
    printf("\n欢迎使用鸢尾花图像分类系统！\n");
    printf("请输入鸢尾花图片路径（输入q退出）\n");
    printf("注意：图片必须是%dx%d的RGB格式\n", IMAGE_WIDTH, IMAGE_HEIGHT);

    while(1) {
        char image_path[MAX_PATH_LENGTH];
        printf("\n请输入图片路径: ");
        fgets(image_path, MAX_PATH_LENGTH, stdin);
        image_path[strcspn(image_path, "\n")] = 0;  // 移除换行符
        
        if (strcmp(image_path, "q") == 0) {
            break;
        }

        /* 加载并处理图像 */
        if (load_image_data(image_path, image_data) != 0) {
            continue;
        }

        /* 从图像中提取特征 */
        if (extract_iris_features(image_data, IMAGE_WIDTH, IMAGE_HEIGHT, features) != 0) {
            ACORAL_LOG_ERROR("Failed to extract features from image\n");
            continue;
        }

        /* 运行模型 */
        g_ai_done_flag_iris = 0;
        if (kpu_run_kmodel(&task1, (const uint8_t *)features, DMAC_CHANNEL5, ai_done, NULL) != 0)
        {
            ACORAL_LOG_ERROR("Cannot run kmodel.\n");
            continue;
        }

        /* 等待推理完成 */
        while (!g_ai_done_flag_iris) {
            msleep(1);
        }

        /* 获取并显示结果 */
        float *output;
        size_t output_size;
        kpu_get_output(&task1, 0, (uint8_t **)&output, &output_size);
        ACORAL_LOG_INFO("预测结果: %s\n", labels[argmax(output, output_size / sizeof(float))]);
    }

    /* 清理资源 */
    acoral_free(model_data);
    acoral_free(image_data);
    ACORAL_LOG_TRACE("Iris Image Classification Complete\n");

    return 0;
}