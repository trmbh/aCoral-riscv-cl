#include "acoral.h"
#include "w25qxx.h"
#include "fpioa.h"
#include "spi.h"
#include "sysctl.h"
#include "dmac.h"
#include "kpu.h"
#include "plic.h"
#include "uarths.h"
#include <stdio.h>
#include <float.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

/* 模型相关参数定义 */
#define KMODEL_SIZE (19648)  // 模型大小约445KB
#define IMAGE_SIZE 28        // MNIST图像是28x28
#define CLASS_NUMBER 10      // 0-9共10个数字类别
#define MAX_PATH_LENGTH 256  // 最大路径长度

/* Flash地址定义 */
#define FLASH_SECTOR_SIZE     0x1000    // Flash扇区大小：4KB
#define MODEL_FLASH_ADDR      0xC00000   // 模型数据地址（已4KB对齐）
#define IMAGE_FLASH_ADDR      0xD00000   // 图片数据起始地址（已4KB对齐）

/* 全局变量声明 */
static uint8_t *model_data;
static kpu_model_context_t task;
static volatile uint32_t g_ai_done_flag;
static uint8_t image_buf[IMAGE_SIZE * IMAGE_SIZE];  // 原始图像缓冲区
static float preprocessed_buf[IMAGE_SIZE * IMAGE_SIZE];  // 预处理后的float数据

/* 数字标签 - 按Flash中的实际顺序排列 */
static const int flash_to_digit[] = {7, 2, 1, 0, 4, 1, 4, 9, 5, 9};

/* 可能的模型问题解决办法 */
static int swap_weights = 0; // 如果模型标签顺序不正确，可以尝试交换
static int use_raw_image = 0; // 尝试直接使用uint8数据
static int input_is_float = 1; // 标记模型输入是否为float

/**
 * @brief AI推理完成回调函数
 */
static void ai_done(void* userdata)
{
    g_ai_done_flag = 1;
}

/**
 * @brief 图像预处理 - 转换为float32并归一化
 */
static void preprocess_image(const uint8_t* input, float* output)
{
    printf("\n[预处理] 开始处理图像数据...\n");
    
    // 计算图像统计信息
    uint8_t min_val = 255;
    uint8_t max_val = 0;
    int sum_val = 0;
    int non_zero_pixels = 0;
    
    for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
        if (input[i] < min_val) min_val = input[i];
        if (input[i] > max_val) max_val = input[i];
        sum_val += input[i];
        if (input[i] > 0) non_zero_pixels++;
    }
    
    // 在归一化前检查最大值，避免除以零
    float scale = (max_val > 0) ? (1.0f / 255.0f) : 0.0f;
    
    // 1. 数据类型转换和归一化
    printf("[预处理] 转换数据类型并归一化: uint8 -> float32\n");
    
    int preprocess_method = swap_weights ? 3 : 1;  // 默认使用方法1，除非开启交换
    
    switch(preprocess_method) {
        case 1:  // 标准[0,1]归一化
            printf("[预处理] 使用方法1: 标准[0,1]归一化\n");
            for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
                output[i] = input[i] * scale;
            }
            break;
            
        case 2:  // 标准[-1,1]归一化
            printf("[预处理] 使用方法2: 标准[-1,1]归一化\n");
            for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
                output[i] = input[i] * (2.0f * scale) - 1.0f;
            }
            break;
            
        case 3:  // 颠倒标签的[0,1]归一化
            printf("[预处理] 使用方法3: 颠倒标签顺序的[0,1]归一化\n");
            // 仍然使用标准归一化，但在softmax中会颠倒标签顺序
            for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
                output[i] = input[i] * scale;
            }
            break;
            
        default:
            printf("[警告] 未知的预处理方法\n");
            break;
    }
    
    // 打印前32个数据
    printf("[预处理] 输入数据样本 (前32字节):\n");
    for (int i = 0; i < 32; i++) {
        printf("%02X ", input[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    
    printf("[预处理] 归一化后数据样本 (前8个值):\n");
    for (int i = 0; i < 8; i++) {
        printf("%.6f ", output[i]);
    }
    printf("\n");
    
    // 2. 验证预处理后的数据
    float out_min = output[0];
    float out_max = output[0];
    float out_sum = 0.0f;
    int out_nonzero = 0;
    
    for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
        if (output[i] < out_min) out_min = output[i];
        if (output[i] > out_max) out_max = output[i];
        out_sum += output[i];
        if (output[i] > 0.001f) out_nonzero++;
    }
    
    printf("\n[预处理] 数据统计:\n");
    printf("- 原始最小值: %d\n", min_val);
    printf("- 原始最大值: %d\n", max_val);
    printf("- 原始平均值: %.2f\n", (float)sum_val / (IMAGE_SIZE * IMAGE_SIZE));
    printf("- 预处理后最小值: %.6f\n", out_min);
    printf("- 预处理后最大值: %.6f\n", out_max);
    printf("- 预处理后平均值: %.6f\n", out_sum / (IMAGE_SIZE * IMAGE_SIZE));
    printf("- 非零值数量: %d (%.1f%%)\n", out_nonzero, 
           (float)out_nonzero * 100 / (IMAGE_SIZE * IMAGE_SIZE));
    
    printf("[预处理] 处理完成\n");
}

/**
 * @brief 从Flash读取图像
 */
static int load_image(int image_index, uint8_t* data)
{
    printf("\n[加载] 开始读取图像 %d...\n", image_index);
    
    if (image_index < 0 || image_index >= CLASS_NUMBER) {
        printf("[错误] 图像索引无效：%d (应在0-9之间)\n", image_index);
        return -1;
    }

    uint32_t image_addr = IMAGE_FLASH_ADDR + (image_index * FLASH_SECTOR_SIZE);
    printf("[Flash] 从地址0x%X读取图像数据...\n", image_addr);
    w25qxx_read_data(image_addr, data, IMAGE_SIZE * IMAGE_SIZE, W25QXX_QUAD_FAST);
    
    printf("\n图像统计信息:\n");
    printf("尺寸: %dx%d\n", IMAGE_SIZE, IMAGE_SIZE);
    
    // 验证和显示图像数据统计
    int valid_pixels = 0;
    uint8_t min_val = 255;
    uint8_t max_val = 0;
    int sum_val = 0;
    
    for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
        sum_val += data[i];
        if (data[i] > 0) valid_pixels++;
    }
    
    printf("最小像素值: %d\n", min_val);
    printf("最大像素值: %d\n", max_val);
    printf("平均像素值: %.2f\n", (float)sum_val / (IMAGE_SIZE * IMAGE_SIZE));
    printf("非零像素数: %d (%.1f%%)\n", valid_pixels, 
           (float)valid_pixels * 100 / (IMAGE_SIZE * IMAGE_SIZE));
    
    if (valid_pixels < 10) {
        printf("[警告] 图像数据可能无效：有效像素过少\n");
        return -1;
    }
    
    // 简单的ASCII图像预览
    printf("\n图像预览 (o表示非零像素):\n");
    for (int y = 0; y < IMAGE_SIZE; y++) {
        for (int x = 0; x < IMAGE_SIZE; x++) {
            int pixel = data[y * IMAGE_SIZE + x];
            printf("%c", (pixel > 0) ? 'o' : '.');
        }
        printf("\n");
    }
    
    return 0;
}

/**
 * @brief 计算Softmax
 */
static void softmax(const float* input, float* output, size_t size)
{
    // 打印原始输出
    printf("\n[模型输出] 原始值:\n");
    float min_val = input[0];
    float max_val = input[0];
    float sum_val = 0.0f;
    
    for (size_t i = 0; i < size; i++) {
        printf("%zu: %.6f\n", i, input[i]);
        if (input[i] < min_val) min_val = input[i];
        if (input[i] > max_val) max_val = input[i];
        sum_val += input[i];
    }
    
    printf("\n[模型输出] 统计信息:\n");
    printf("- 最小值: %.6f\n", min_val);
    printf("- 最大值: %.6f\n", max_val);
    printf("- 平均值: %.6f\n", sum_val / size);
    printf("- 数值范围: %.6f\n", max_val - min_val);

    // 找出最大值 - 避免指数计算溢出
    float max_val_for_softmax = input[0];
    for (size_t i = 1; i < size; i++) {
        if (input[i] > max_val_for_softmax) {
            max_val_for_softmax = input[i];
        }
    }

    // 处理一个特殊情况：如果模型可能将7和其他数字混淆
    if (swap_weights) {
        printf("[诊断] 尝试颠倒标签顺序...\n");
        
        // 创建临时数组复制输入数据
        float swapped_input[CLASS_NUMBER];
        
        // 颠倒标签，将7映射到1，因为它们经常被混淆
        swapped_input[0] = input[0];     // 0
        swapped_input[1] = input[7];     // 将7映射到1
        swapped_input[2] = input[2];     // 2
        swapped_input[3] = input[3];     // 3
        swapped_input[4] = input[4];     // 4
        swapped_input[5] = input[5];     // 5
        swapped_input[6] = input[6];     // 6
        swapped_input[7] = input[1];     // 将1映射到7
        swapped_input[8] = input[8];     // 8
        swapped_input[9] = input[9];     // 9
        
        // 重新找最大值
        max_val_for_softmax = swapped_input[0];
        for (size_t i = 1; i < size; i++) {
            if (swapped_input[i] > max_val_for_softmax) {
                max_val_for_softmax = swapped_input[i];
            }
        }
        
        // 计算exp和归一化
        float sum = 0.0f;
        for (size_t i = 0; i < size; i++) {
            output[i] = exp(swapped_input[i] - max_val_for_softmax);
            sum += output[i];
        }
        
        // 归一化概率
        if (sum > 0) {
            for (size_t i = 0; i < size; i++) {
                output[i] /= sum;
            }
        } else {
            // 避免除以零
            printf("[警告] Softmax计算出现零和，使用均等概率\n");
            for (size_t i = 0; i < size; i++) {
                output[i] = 1.0f / size;  // 均等概率
            }
        }
    } else {
        // 计算exp和归一化
        float sum = 0.0f;
        for (size_t i = 0; i < size; i++) {
            output[i] = exp(input[i] - max_val_for_softmax);
            sum += output[i];
        }

        // 归一化概率
        if (sum > 0) {
            for (size_t i = 0; i < size; i++) {
                output[i] /= sum;
            }
        } else {
            // 避免除以零
            printf("[警告] Softmax计算出现零和，使用均等概率\n");
            for (size_t i = 0; i < size; i++) {
                output[i] = 1.0f / size;  // 均等概率
            }
        }
    }
    
    // 打印概率分布
    printf("\n[模型输出] 概率分布:\n");
    for (size_t i = 0; i < size; i++) {
        printf("%zu: %.2f%%\n", i, output[i] * 100.0f);
    }
}

/**
 * @brief 简单推理方法，使用最少的设置
 */
static int simple_inference(void* input_data, float* output_data)
{
    // 开始推理
    g_ai_done_flag = 0;
    printf("[推理] 开始运行模型...\n");
    
    // 直接使用KPU官方API运行模型
    if (use_raw_image) {
        // 使用原始uint8图像数据作为输入
        printf("[推理] 使用原始uint8图像数据作为输入\n");
        if (kpu_run_kmodel(&task, (uint8_t *)image_buf, DMAC_CHANNEL5, ai_done, NULL) != 0) {
            printf("[错误] 模型启动失败\n");
            return -1;
        }
    } else {
        // 使用预处理后的float数据作为输入
        printf("[推理] 使用预处理后的float数据作为输入\n");
        if (kpu_run_kmodel(&task, (uint8_t *)input_data, DMAC_CHANNEL5, ai_done, NULL) != 0) {
            printf("[错误] 模型启动失败\n");
            return -1;
        }
    }
    
    // 等待推理完成
    printf("[推理] 等待模型运行完成...\n");
    while (!g_ai_done_flag) {
        usleep(10000);  // 10ms
    }
    printf("[推理] 模型运行完成\n");
    
    // 获取输出
    size_t output_size;
    float* model_output;
    
    if (kpu_get_output(&task, 0, (uint8_t **)&model_output, &output_size) != 0) {
        printf("[错误] 无法获取模型输出\n");
        return -1;
    }
    
    if (output_size != CLASS_NUMBER * sizeof(float)) {
        printf("[错误] 输出大小不匹配: %zu vs %zu\n", 
               output_size, CLASS_NUMBER * sizeof(float));
        return -1;
    }
    
    // 复制输出数据到输出缓冲区
    memcpy(output_data, model_output, output_size);
    
    return 0;
}

/**
 * @brief 主函数
 */
int test_mnist(void)
{
    printf("\n[系统] 初始化开始...\n");
    
    /* 初始化系统 */
    uarths_init();
    plic_init();
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);
    fpioa_init();
    dmac_init();
    printf("[系统] 基础组件初始化完成\n");

    /* 分配模型内存 */
    model_data = (uint8_t *)acoral_malloc(KMODEL_SIZE);
    if (!model_data) {
        printf("[错误] 无法分配模型内存\n");
        return -1;
    }
    printf("[模型] 开始加载模型, 大小: %d 字节...\n", KMODEL_SIZE);

    /* 初始化Flash并读取模型 */
    w25qxx_init(3, 0);
    w25qxx_enable_quad_mode();
    
    /* 读取模型数据 */
    printf("[Flash] 从地址0x%X读取模型...\n", MODEL_FLASH_ADDR);
    w25qxx_read_data(MODEL_FLASH_ADDR, model_data, KMODEL_SIZE, W25QXX_QUAD_FAST);
    
    /* 加载模型 */
    printf("[模型] 加载到KPU...\n");
    int ret = kpu_load_kmodel(&task, model_data);
    if (ret != 0) {
        printf("[错误] 模型加载失败，错误码：%d\n", ret);
        acoral_free(model_data);
        return -1;
    }
    printf("[模型] 加载成功\n");

    /* 启用全局中断 */
    sysctl_enable_irq();
    printf("[系统] 初始化完成\n");

    printf("\n欢迎使用手写数字识别系统！\n");
    printf("请输入要识别的图片序号（0-9，输入q退出）\n");
    printf("或特殊命令：d1、d2、d3 (诊断模式)\n");  // 新增诊断命令提示

    char input[MAX_PATH_LENGTH];
    int total = 0;
    int correct = 0;

    while(1) {
        printf("\n请输入图片序号 (0-9，输入q退出): ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "q") == 0) {
            break;
        }
        
        // 特殊诊断命令处理
        if (strcmp(input, "d1") == 0) {
            printf("[诊断] 启用诊断模式1: 尝试直接使用原始图像数据\n");
            use_raw_image = 1;
            input_is_float = 0;
            swap_weights = 0;
            continue;
        }
        
        if (strcmp(input, "d2") == 0) {
            printf("[诊断] 启用诊断模式2: 尝试交换7和1标签\n");
            use_raw_image = 0;
            input_is_float = 1;
            swap_weights = 1;
            continue;
        }
        
        if (strcmp(input, "d3") == 0) {
            printf("[诊断] 重置为标准模式\n");
            use_raw_image = 0;
            input_is_float = 1;
            swap_weights = 0;
            continue;
        }

        int image_index = atoi(input);
        if (image_index < 0 || image_index >= CLASS_NUMBER) {
            printf("[错误] 无效的图片序号，请输入0-9之间的数字\n");
            continue;
        }

        // 加载图像
        if (load_image(image_index, image_buf) != 0) {
            printf("[错误] 图像加载失败\n");
            continue;
        }

        // 获取实际的数字标签
        int expected_digit = flash_to_digit[image_index];
        printf("\n[信息] 图片序号 %d 对应的实际数字是: %d\n", image_index, expected_digit);

        // 预处理图像 - 如果使用原始图像数据模式，此步骤仍会执行但不会被用于推理
        preprocess_image(image_buf, preprocessed_buf);

        // 运行简单推理
        float output_data[CLASS_NUMBER];
        if (simple_inference(preprocessed_buf, output_data) != 0) {
            printf("[错误] 模型推理失败\n");
            continue;
        }

        // 计算softmax概率
        float probabilities[CLASS_NUMBER];
        softmax(output_data, probabilities, CLASS_NUMBER);

        // 找出最大概率对应的类别
        float max_prob = probabilities[0];
        int predicted_label = 0;
        
        for (int i = 1; i < CLASS_NUMBER; i++) {
            if (probabilities[i] > max_prob) {
                max_prob = probabilities[i];
                predicted_label = i;
            }
        }

        // 打印预测结果
        printf("\n[结果] 识别结果详情:\n");
        printf("- 预测数字: %d\n", predicted_label);
        printf("- 置信度: %.2f%%\n", max_prob * 100);
        printf("- 目标数字: %d\n", expected_digit);
        
        if (predicted_label == expected_digit) {
            correct++;
            printf("- 预测正确 ✓\n");
        } else {
            printf("- 预测错误 ✗\n");
        }
        
        // 添加当前使用的诊断模式信息
        printf("- 诊断模式: %s\n", 
               use_raw_image ? "使用原始图像" : 
               (swap_weights ? "交换标签" : "标准模式"));
        
        total++;
        printf("\n[统计] 当前准确率: %d/%d = %.2f%%\n", 
               correct, total, (float)correct/total*100);
    }

    /* 清理资源 */
    printf("\n[系统] 清理资源...\n");
    kpu_model_free(&task);
    acoral_free(model_data);
    printf("[系统] 退出\n");
    return 0;
} 