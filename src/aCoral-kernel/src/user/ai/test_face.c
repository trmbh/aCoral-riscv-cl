/* Copyright 2023 SPG.
 * Licensed under the Apache License, Version 2.0
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

/* 模型相关参数定义 */
#define KMODEL_SIZE (200000)  // 模型大小约200KB
#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240
#define IMAGE_CHANNELS 3

/* 全局变量声明 */
static uint8_t *model_data;
static kpu_model_context_t task;
static volatile uint32_t g_ai_done_flag;
static uint8_t *image_buf;
static uint16_t *lcd_buf;

/* 人脸检测参数 */
#define FACE_THRESHOLD 0.5    // 人脸检测阈值
#define MAX_FACE_NUM 10      // 最大检测人脸数

/**
 * @brief 人脸检测结果结构体
 */
typedef struct {
    float x;
    float y;
    float w;
    float h;
    float prob;
} face_rect_t;

static face_rect_t face_results[MAX_FACE_NUM];
static int face_count = 0;

/**
 * @brief AI推理完成回调函数
 */
static void ai_done(void* userdata)
{
    g_ai_done_flag = 1;
}

/**
 * @brief 从文件读取图像数据
 */
static int load_image(const char* filename, uint8_t* data)
{
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
 * @brief 处理模型输出，提取人脸位置
 */
static void process_output(float *output, size_t size)
{
    face_count = 0;
    size_t box_num = size / 5;  // 每个框5个值：x,y,w,h,prob
    
    for (size_t i = 0; i < box_num && face_count < MAX_FACE_NUM; i++) {
        float prob = output[i * 5 + 4];
        if (prob > FACE_THRESHOLD) {
            face_results[face_count].x = output[i * 5];
            face_results[face_count].y = output[i * 5 + 1];
            face_results[face_count].w = output[i * 5 + 2];
            face_results[face_count].h = output[i * 5 + 3];
            face_results[face_count].prob = prob;
            face_count++;
        }
    }
}

/**
 * @brief 在LCD上绘制人脸框
 */
static void draw_faces(void)
{
    for (int i = 0; i < face_count; i++) {
        face_rect_t *face = &face_results[i];
        uint32_t x1 = (uint32_t)(face->x * IMAGE_WIDTH);
        uint32_t y1 = (uint32_t)(face->y * IMAGE_HEIGHT);
        uint32_t x2 = x1 + (uint32_t)(face->w * IMAGE_WIDTH);
        uint32_t y2 = y1 + (uint32_t)(face->h * IMAGE_HEIGHT);
        
        // 确保坐标在有效范围内
        if (x1 >= IMAGE_WIDTH) x1 = IMAGE_WIDTH - 1;
        if (x2 >= IMAGE_WIDTH) x2 = IMAGE_WIDTH - 1;
        if (y1 >= IMAGE_HEIGHT) y1 = IMAGE_HEIGHT - 1;
        if (y2 >= IMAGE_HEIGHT) y2 = IMAGE_HEIGHT - 1;
        
        // 绘制矩形框
        lcd_draw_rectangle(x1, y1, x2, y2, 2, RED);
        
        // 显示置信度
        char prob_str[10];
        sprintf(prob_str, "%.2f", face->prob);
        lcd_draw_string(x1, y1 - 16, prob_str, RED);
    }
}

/**
 * @brief 人脸检测测试主函数
 */
int test_face(void)
{
    /* 初始化系统 */
    uarths_init();
    plic_init();

    /* 设置系统时钟 */
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);

    /* 分配内存 */
    model_data = (uint8_t *)acoral_malloc(KMODEL_SIZE);
    image_buf = (uint8_t *)acoral_malloc(IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_CHANNELS);
    lcd_buf = (uint16_t *)acoral_malloc(IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    
    if (!model_data || !image_buf || !lcd_buf) {
        ACORAL_LOG_ERROR("Failed to allocate memory\n");
        if (model_data) acoral_free(model_data);
        if (image_buf) acoral_free(image_buf);
        if (lcd_buf) acoral_free(lcd_buf);
        return -1;
    }

    /* 初始化Flash并读取模型 */
    ACORAL_LOG_TRACE("Flash Init Start\n");
    w25qxx_init(3, 0);
    w25qxx_enable_quad_mode();
    w25qxx_read_data(0xE00000, model_data, KMODEL_SIZE, W25QXX_QUAD_FAST);

    /* 加载模型 */
    if (kpu_load_kmodel(&task, model_data) != 0) {
        ACORAL_LOG_ERROR("Cannot load kmodel\n");
        acoral_free(model_data);
        acoral_free(image_buf);
        acoral_free(lcd_buf);
        return -1;
    }

    /* 初始化LCD */
    lcd_init();
    lcd_set_direction(DIR_YX_RLDU);
    lcd_clear(BLACK);

    /* 启用全局中断 */
    sysctl_enable_irq();

    /* 开始检测 */
    ACORAL_LOG_TRACE("Starting Face Detection\n");
    printf("\n欢迎使用人脸检测系统！\n");
    printf("请输入320x240的RGB图像路径（输入q退出）\n");

    char image_path[256];
    while(1) {
        printf("\n请输入图片路径: ");
        fgets(image_path, sizeof(image_path), stdin);
        image_path[strcspn(image_path, "\n")] = 0;

        if (strcmp(image_path, "q") == 0) {
            break;
        }

        /* 加载图像 */
        if (load_image(image_path, image_buf) != 0) {
            continue;
        }

        /* 运行模型 */
        g_ai_done_flag = 0;
        if (kpu_run_kmodel(&task, image_buf, DMAC_CHANNEL5, ai_done, NULL) != 0) {
            ACORAL_LOG_ERROR("Cannot run model\n");
            continue;
        }

        /* 等待推理完成 */
        while (!g_ai_done_flag) {
            msleep(1);
        }

        /* 获取并处理结果 */
        float *output;
        size_t output_size;
        kpu_get_output(&task, 0, (uint8_t **)&output, &output_size);
        process_output(output, output_size / sizeof(float));

        /* 显示结果 */
        rgb888_to_lcd(image_buf, lcd_buf, IMAGE_WIDTH, IMAGE_HEIGHT);
        lcd_draw_picture(0, 0, IMAGE_WIDTH, IMAGE_HEIGHT, (uint32_t*)lcd_buf);
        draw_faces();

        ACORAL_LOG_INFO("检测到 %d 个人脸\n", face_count);
    }

    /* 清理资源 */
    acoral_free(model_data);
    acoral_free(image_buf);
    acoral_free(lcd_buf);
    ACORAL_LOG_TRACE("Face Detection Test Complete\n");

    return 0;
} 