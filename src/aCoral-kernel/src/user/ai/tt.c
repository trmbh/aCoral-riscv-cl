// /* Copyright 2023 SPG.
//  * Licensed under the Apache License, Version 2.0
//  */

// #include "acoral.h"
// #include "w25qxx.h"
// #include "fpioa.h"
// #include "spi.h"
// #include "sysctl.h"
// #include "dmac.h"
// #include "kpu.h"
// #include "plic.h"
// #include "uarths.h"
// #include <stdio.h>
// #include <float.h>
// #include <unistd.h>
// #include <string.h>
// #include <math.h>
// #include <stdlib.h>
// #include <stdint.h>
// #include <stddef.h>

// /* 模型相关参数定义 */
// #define KMODEL_SIZE (19648)  // 模型大小约445KB
// #define IMAGE_SIZE 28        // MNIST图像是28x28
// #define CLASS_NUMBER 10      // 0-9共10个数字类别
// #define MAX_PATH_LENGTH 256  // 最大路径长度

// /* Flash地址定义 */
// #define FLASH_SECTOR_SIZE     0x1000    // Flash扇区大小：4KB
// #define MODEL_FLASH_ADDR      0xC00000   // 模型数据地址（已4KB对齐）
// #define IMAGE_FLASH_ADDR      0xD00000   // 图片数据起始地址（已4KB对齐）

// /* 全局变量声明 */
// static uint8_t *model_data;
// static kpu_model_context_t task;
// static volatile uint32_t g_ai_done_flag;
// static uint8_t image_buf[IMAGE_SIZE * IMAGE_SIZE];  // 单通道灰度图
// static float preprocessed_buf[IMAGE_SIZE * IMAGE_SIZE];  // 预处理后的浮点数据

// /* 数字标签 - 按Flash中的实际顺序排列 */
// static const int flash_to_digit[] = {7, 2, 1, 0, 4, 1, 4, 9, 5, 9};

// /**
//  * @brief 重新加载并初始化模型
//  */
// static int reload_model(void)
// {
//     printf("[重置] 开始重新加载模型...\n");
    
//     // 先卸载现有模型
//     kpu_model_free(&task);
    
//     // 重新从Flash读取模型数据
//     printf("[重置] 从Flash读取模型数据...\n");
//     w25qxx_read_data(MODEL_FLASH_ADDR, model_data, KMODEL_SIZE, W25QXX_QUAD_FAST);
    
//     // 重新加载模型
//     printf("[重置] 加载KPU模型...\n");
//     int ret = kpu_load_kmodel(&task, model_data);
//     if (ret != 0) {
//         printf("[错误] 模型重新加载失败，错误码：%d\n", ret);
//         return -1;
//     }
    
//     // 配置模型参数
//     kpu_config(&task, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    
//     printf("[重置] 模型重新加载完成\n");
//     return 0;
// }

// /**
//  * @brief AI推理完成回调函数
//  */
// static void ai_done(void* userdata)
// {
//     g_ai_done_flag = 1;
// }

// /**
//  * @brief 计算Softmax
//  */
// static void softmax(const float* input, float* output, size_t size)
// {
//     float max_val = input[0];
//     // 找出最大值以防止数值溢出
//     for (size_t i = 1; i < size; i++) {
//         if (input[i] > max_val) {
//             max_val = input[i];
//         }
//     }

//     float sum = 0.0f;
//     // 计算exp并求和
//     for (size_t i = 0; i < size; i++) {
//         output[i] = exp(input[i] - max_val);
//         sum += output[i];
//     }

//     // 归一化
//     for (size_t i = 0; i < size; i++) {
//         output[i] /= sum;
//     }
// }

// /**
//  * @brief 找出最可能的数字类别
//  */
// static size_t argmax(const float *src, size_t count)
// {
//     float max = src[0];
//     size_t max_id = 0;
//     for (size_t i = 1; i < count; i++) {
//         if (src[i] > max) {
//             max = src[i];
//             max_id = i;
//         }
//     }
//     return max_id;
// }

// /**
//  * @brief 打印图像信息
//  */
// static void print_image_info(const uint8_t* data)
// {
//     uint8_t min_val = 255;
//     uint8_t max_val = 0;
//     float avg_val = 0;
//     int non_zero = 0;

//     // 计算基本统计信息
//     for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
//         if (data[i] < min_val) min_val = data[i];
//         if (data[i] > max_val) max_val = data[i];
//         avg_val += data[i];
//         if (data[i] > 0) non_zero++;
//     }
//     avg_val /= (IMAGE_SIZE * IMAGE_SIZE);

//     // 打印统计信息
//     printf("\n图像统计信息:\n");
//     printf("最小像素值: %d\n", min_val);
//     printf("最大像素值: %d\n", max_val);
//     printf("平均像素值: %.2f\n", avg_val);
//     printf("非零像素数: %d (%.1f%%)\n", non_zero, 
//            (float)non_zero * 100 / (IMAGE_SIZE * IMAGE_SIZE));

//     // 打印ASCII图像预览
//     printf("\n图像预览 (o表示非零像素):\n");
//     for (int y = 0; y < IMAGE_SIZE; y++) {
//         for (int x = 0; x < IMAGE_SIZE; x++) {
//             printf("%c", data[y * IMAGE_SIZE + x] > 0 ? 'o' : '.');
//         }
//         printf("\n");
//     }
// }

// /**
//  * @brief 图像预处理
//  */
// static void preprocess_image(const uint8_t* input, float* output)
// {
//     printf("\n[预处理] 开始处理图像数据...\n");
    
//     // 1. 归一化处理：将uint8转换为float32并使用mean=0, std=255进行标准化
//     printf("[预处理] 转换数据类型并归一化: uint8 -> float32, mean=0, std=255\n");
//     for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
//         output[i] = ((float)input[i] - 0.0f) / 255.0f;  // 与Python版本保持一致
//     }

//     // 2. 打印预处理后的数据统计
//     float min_val = output[0];
//     float max_val = output[0];
//     float sum_val = 0;
//     int non_zero_count = 0;
    
//     for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
//         if (output[i] < min_val) min_val = output[i];
//         if (output[i] > max_val) max_val = output[i];
//         sum_val += output[i];
//         if (output[i] > 0) non_zero_count++;
//     }
//     float avg_val = sum_val / (IMAGE_SIZE * IMAGE_SIZE);
    
//     printf("\n[预处理] 数据统计:\n");
//     printf("- 最小值: %.6f\n", min_val);
//     printf("- 最大值: %.6f\n", max_val);
//     printf("- 平均值: %.6f\n", avg_val);
//     printf("- 非零值数量: %d (%.1f%%)\n", non_zero_count, 
//            (float)non_zero_count * 100 / (IMAGE_SIZE * IMAGE_SIZE));
    
//     // 3. 检查数据有效性
//     if (non_zero_count < 10) {
//         printf("[警告] 图像可能存在问题：非零像素过少\n");
//     }
    
//     printf("[预处理] 处理完成\n");
// }

// /**
//  * @brief 从Flash读取图像
//  */
// static int load_image(int image_index, uint8_t* data)
// {
//     printf("\n[加载图像] 开始读取图像 %d...\n", image_index);
    
//     if (image_index < 0 || image_index >= CLASS_NUMBER) {
//         printf("[错误] 图像索引无效：%d (应在0-9之间)\n", image_index);
//         return -1;
//     }

//     uint32_t image_addr = IMAGE_FLASH_ADDR + (image_index * FLASH_SECTOR_SIZE);
//     printf("[加载图像] Flash地址: 0x%X\n", image_addr);
//     printf("[加载图像] 读取大小: %d bytes\n", IMAGE_SIZE * IMAGE_SIZE);
    
//     w25qxx_read_data(image_addr, data, IMAGE_SIZE * IMAGE_SIZE, W25QXX_QUAD_FAST);
//     printf("[加载图像] Flash读取完成\n");
    
//     // 验证数据
//     int valid_pixels = 0;
//     uint8_t min_val = 255;
//     uint8_t max_val = 0;
    
//     for (int i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
//         if (data[i] < min_val) min_val = data[i];
//         if (data[i] > max_val) max_val = data[i];
//         if (data[i] > 0) valid_pixels++;
//     }
    
//     printf("[加载图像] 数据验证:\n");
//     printf("- 最小值: %d\n", min_val);
//     printf("- 最大值: %d\n", max_val);
//     printf("- 有效像素: %d\n", valid_pixels);
    
//     if (valid_pixels < 10) {
//         printf("[警告] 图像数据可能无效：有效像素过少\n");
//     }
    
//     print_image_info(data);
//     printf("[加载图像] 加载完成\n");
    
//     return 0;
// }

// /**
//  * @brief 主函数
//  */
// int test_mnist(void)
// {
//     /* 初始化系统 */
//     uarths_init();
//     plic_init();
//     sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
//     sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
//     sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);
//     fpioa_init();
//     dmac_init();

//     /* 分配模型内存 */
//     model_data = (uint8_t *)acoral_malloc(KMODEL_SIZE);
//     if (!model_data) {
//         printf("[错误] 无法分配模型内存\n");
//         return -1;
//     }

//     /* 初始化Flash并读取模型 */
//     printf("[初始化] 初始化Flash...\n");
//     w25qxx_init(3, 0);
//     w25qxx_enable_quad_mode();
    
//     /* 首次加载模型 */
//     if (reload_model() != 0) {
//         acoral_free(model_data);
//         return -1;
//     }

//     /* 启用全局中断 */
//     sysctl_enable_irq();

//     /* 开始识别 */
//     printf("\n欢迎使用手写数字识别系统！\n");
//     printf("请输入要识别的图片序号（0-9，输入q退出）\n");

//     char input[MAX_PATH_LENGTH];
//     int total = 0;
//     int correct = 0;
//     int consecutive_errors = 0;  // 连续错误计数

//     while(1) {
//         printf("\n请输入图片序号 (0-9，输入q退出): ");
//         fgets(input, sizeof(input), stdin);
//         input[strcspn(input, "\n")] = 0;

//         if (strcmp(input, "q") == 0) {
//             break;
//         }

//         int image_index = atoi(input);
//         if (image_index < 0 || image_index >= CLASS_NUMBER) {
//             printf("错误：请输入0-9之间的数字\n");
//             continue;
//         }

//         // 如果连续错误超过2次，尝试重新加载模型
//         if (consecutive_errors >= 2) {
//             printf("[警告] 检测到连续错误，尝试重新加载模型...\n");
//             if (reload_model() != 0) {
//                 printf("[错误] 模型重新加载失败，退出程序\n");
//                 break;
//             }
//             consecutive_errors = 0;
//         }

//         total++;

//         // 加载图像并显示信息
//         if (load_image(image_index, image_buf) != 0) {
//             consecutive_errors++;
//             continue;
//         }

//         // 获取实际的数字标签
//         int expected_digit = flash_to_digit[image_index];
//         printf("\n[信息] 图片序号 %d 对应的实际数字是: %d\n", image_index, expected_digit);

//         // 预处理图像
//         preprocess_image(image_buf, preprocessed_buf);

//         // 运行模型
//         g_ai_done_flag = 0;
//         printf("\n[推理] 开始运行模型...\n");

//         // 配置DMA通道
//         dmac_set_single_mode(DMAC_CHANNEL5, (void*)preprocessed_buf, NULL, DMAC_ADDR_INCREMENT, DMAC_ADDR_NOCHANGE, DMAC_MSIZE_4, DMAC_TRANS_WIDTH_32, 784);
        
//         int ret = kpu_run_kmodel(&task, (uint8_t*)preprocessed_buf, DMAC_CHANNEL5, ai_done, NULL);
//         if (ret != 0) {
//             printf("[错误] 模型运行失败，错误码：%d\n", ret);
//             consecutive_errors++;
//             continue;
//         }

//         printf("[推理] 等待模型运行完成...\n");
//         while (!g_ai_done_flag) {
//             usleep(1000);
//         }
//         printf("[推理] 模型运行完成\n");

//         // 获取模型输出
//         float *output;
//         size_t output_size;
        
//         printf("[推理] 获取模型输出...\n");
//         ret = kpu_get_output(&task, 0, (uint8_t **)&output, &output_size);
//         if (ret != 0) {
//             printf("[错误] 无法获取模型输出，错误码：%d\n", ret);
//             consecutive_errors++;
//             continue;
//         }

//         // 检查输出大小是否正确
//         printf("[推理] 检查输出大小: 实际=%zu, 期望=%zu\n", 
//                output_size, CLASS_NUMBER * sizeof(float));
               
//         if (output_size != CLASS_NUMBER * sizeof(float)) {
//             printf("[错误] 输出大小不正确\n");
//             consecutive_errors++;
//             continue;
//         }

//         // 检查输出是否有效
//         int valid_output = 0;
//         for (int i = 0; i < CLASS_NUMBER; i++) {
//             if (output[i] != 0.0f) {
//                 valid_output = 1;
//                 break;
//             }
//         }
        
//         if (!valid_output) {
//             printf("[错误] 模型输出全为0，可能存在问题\n");
//             consecutive_errors++;
//             continue;
//         }

//         // 检查输出值范围
//         float min_output = output[0];
//         float max_output = output[0];
//         float sum_output = 0.0f;
        
//         for (int i = 0; i < CLASS_NUMBER; i++) {
//             if (output[i] < min_output) min_output = output[i];
//             if (output[i] > max_output) max_output = output[i];
//             sum_output += output[i];
//         }
        
//         // 如果输出范围异常，尝试重新加载模型
//         if (max_output > 20.0f || min_output < -20.0f) {
//             printf("[警告] 模型输出范围异常 [%.2f, %.2f]，尝试重新加载模型...\n",
//                    min_output, max_output);
//             if (reload_model() != 0) {
//                 printf("[错误] 模型重新加载失败，退出程序\n");
//                 break;
//             }
//             consecutive_errors++;
//             continue;
//         }

//         // 打印原始输出
//         printf("\n[推理] 原始模型输出:\n");
//         for (int i = 0; i < CLASS_NUMBER; i++) {
//             printf("%d: %.6f\n", i, output[i]);
//         }
        
//         printf("\n[推理] 输出统计:\n");
//         printf("- 最小值: %.6f\n", min_output);
//         printf("- 最大值: %.6f\n", max_output);
//         printf("- 平均值: %.6f\n", sum_output / CLASS_NUMBER);

//         // 计算softmax概率
//         printf("\n[后处理] 计算softmax概率...\n");
//         float probabilities[CLASS_NUMBER];
//         softmax(output, probabilities, CLASS_NUMBER);
        
//         // 获取预测结果
//         size_t predicted_label = argmax(probabilities, CLASS_NUMBER);
//         float confidence = probabilities[predicted_label] * 100.0f;

//         // 打印预测结果
//         printf("\n[结果] 识别结果详情:\n");
//         printf("- 预测数字: %ld\n", predicted_label);
//         printf("- 置信度: %.2f%%\n", confidence);
//         printf("- 目标数字: %d\n", expected_digit);
//         printf("- 预测%s\n", predicted_label == expected_digit ? "正确" : "错误");
        
//         printf("\n[结果] 所有数字的概率分布:\n");
//         for (int i = 0; i < CLASS_NUMBER; i++) {
//             printf("%d: %.2f%%\n", i, probabilities[i] * 100.0f);
//         }

//         // 更新准确率统计
//         if (predicted_label == expected_digit) {
//             correct++;
//             consecutive_errors = 0;  // 重置连续错误计数
//             printf("\n[统计] 预测正确 ✓\n");
//         } else {
//             consecutive_errors++;
//             printf("\n[统计] 预测错误 ✗\n");
//         }
//         printf("[统计] 当前准确率: %d/%d = %.2f%%\n", 
//                correct, total, (float)correct/total*100);
//     }

//     // 打印最终准确率
//     if (total > 0) {
//         printf("\n准确率: %d/%d = %.2f%%\n", correct, total, (float)correct/total*100);
//     }

//     /* 清理资源 */
//     kpu_model_free(&task);
//     acoral_free(model_data);
//     return 0;
// } 