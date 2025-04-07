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
// #define IMAGE_CHANNELS 1     // 图像通道数
// #define BATCH_SIZE 1        // 批处理大小

// /* Flash地址定义 */
// #define FLASH_SECTOR_SIZE     0x1000    // Flash扇区大小：4KB
// #define MODEL_FLASH_ADDR      0xC00000   // 模型数据地址（已4KB对齐）
// #define IMAGE_FLASH_ADDR      0xD00000   // 图片数据起始地址（已4KB对齐）

// /* 全局变量声明 */
// static uint8_t *model_data;
// static kpu_model_context_t task;
// static volatile uint32_t g_ai_done_flag;
// static uint8_t image_buf[BATCH_SIZE * IMAGE_SIZE * IMAGE_SIZE * IMAGE_CHANNELS];  // 原始图像缓冲区
// static float preprocessed_buf[BATCH_SIZE * IMAGE_SIZE * IMAGE_SIZE * IMAGE_CHANNELS];  // 预处理后的float数据

// /* 数字标签 - 按Flash中的实际顺序排列 */
// static const int flash_to_digit[] = {7, 2, 1, 0, 4, 1, 4, 9, 5, 9};

// /**
//  * @brief AI推理完成回调函数
//  */
// static void ai_done(void* userdata)
// {
//     g_ai_done_flag = 1;
// }

// /**
//  * @brief 图像预处理
//  */
// static void preprocess_image(const uint8_t* input, float* output)
// {
//     printf("\n[预处理] 开始处理图像数据...\n");
    
//     // 跳过batch和channel维度的头部数据
//     const uint8_t* image_data = input + 4;  // 跳过(1,)维度
//     size_t image_size = IMAGE_SIZE * IMAGE_SIZE;
    
//     // 数据类型转换和归一化：使用与模型转换时相同的参数 (mean=0, std=255)
//     printf("[预处理] 转换数据类型并归一化: uint8 -> float32, mean=0, std=255\n");
//     for (int i = 0; i < image_size; i++) {
//         output[i] = (float)(image_data[i] - 0.0f) / 255.0f;
//     }

//     // 打印预处理后的数据统计
//     float min_val = output[0];
//     float max_val = output[0];
//     float sum_val = 0;
//     int non_zero_count = 0;
    
//     for (int i = 0; i < image_size; i++) {
//         if (output[i] < min_val) min_val = output[i];
//         if (output[i] > max_val) max_val = output[i];
//         sum_val += output[i];
//         if (output[i] > 0) non_zero_count++;
//     }
//     float avg_val = sum_val / image_size;
    
//     printf("\n[预处理] 数据统计:\n");
//     printf("- 最小值: %.6f\n", min_val);
//     printf("- 最大值: %.6f\n", max_val);
//     printf("- 平均值: %.6f\n", avg_val);
//     printf("- 非零值数量: %d (%.1f%%)\n", non_zero_count, 
//            (float)non_zero_count * 100 / image_size);
    
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
//     // 读取完整的NHWC格式数据
//     w25qxx_read_data(image_addr, data, BATCH_SIZE * IMAGE_SIZE * IMAGE_SIZE * IMAGE_CHANNELS, W25QXX_QUAD_FAST);
    
//     // 验证数据
//     int valid_pixels = 0;
//     uint8_t min_val = 255;
//     uint8_t max_val = 0;
    
//     // 跳过batch和channel维度的头部数据
//     uint8_t* image_data = data + 4;  // 跳过(1,)维度
//     size_t image_size = IMAGE_SIZE * IMAGE_SIZE;
    
//     for (int i = 0; i < image_size; i++) {
//         if (image_data[i] < min_val) min_val = image_data[i];
//         if (image_data[i] > max_val) max_val = image_data[i];
//         if (image_data[i] > 0) valid_pixels++;
//     }
    
//     printf("[加载图像] 数据统计:\n");
//     printf("- 最小值: %d\n", min_val);
//     printf("- 最大值: %d\n", max_val);
//     printf("- 有效像素: %d\n", valid_pixels);
    
//     if (valid_pixels < 10) {
//         printf("[警告] 图像数据可能无效：有效像素过少\n");
//         return -1;
//     }
    
//     return 0;
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
    
//     /* 读取模型数据 */
//     printf("[初始化] 从Flash读取模型数据...\n");
//     w25qxx_read_data(MODEL_FLASH_ADDR, model_data, KMODEL_SIZE, W25QXX_QUAD_FAST);
    
//     /* 加载模型 */
//     printf("[初始化] 加载KPU模型...\n");
//     if (kpu_load_kmodel(&task, model_data) != 0) {
//         printf("[错误] 模型加载失败\n");
//         acoral_free(model_data);
//         return -1;
//     }

//     /* 启用全局中断 */
//     sysctl_enable_irq();

//     printf("\n欢迎使用手写数字识别系统！\n");
//     printf("请输入要识别的图片序号（0-9，输入q退出）\n");

//     char input[MAX_PATH_LENGTH];
//     int total = 0;
//     int correct = 0;

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

//         total++;

//         // 加载图像
//         if (load_image(image_index, image_buf) != 0) {
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
//         dmac_set_single_mode(DMAC_CHANNEL5, preprocessed_buf, NULL, 
//                            DMAC_ADDR_INCREMENT, DMAC_ADDR_NOCHANGE,
//                            DMAC_MSIZE_4, DMAC_TRANS_WIDTH_32, IMAGE_SIZE * IMAGE_SIZE);
        
//         if (kpu_run_kmodel(&task, (uint8_t*)preprocessed_buf, DMAC_CHANNEL5, ai_done, NULL) != 0) {
//             printf("[错误] 模型运行失败\n");
//             continue;
//         }

//         while (!g_ai_done_flag) {
//             usleep(1000);
//         }

//         // 获取模型输出
//         float *output;
//         size_t output_size;
        
//         if (kpu_get_output(&task, 0, (uint8_t **)&output, &output_size) != 0) {
//             printf("[错误] 无法获取模型输出\n");
//             continue;
//         }

//         // 计算softmax概率
//         float probabilities[CLASS_NUMBER];
//         softmax(output, probabilities, CLASS_NUMBER);

//         // 找出最大概率对应的类别
//         float max_prob = probabilities[0];
//         int predicted_label = 0;
        
//         for (int i = 0; i < CLASS_NUMBER; i++) {
//             if (probabilities[i] > max_prob) {
//                 max_prob = probabilities[i];
//                 predicted_label = i;
//             }
//         }

//         // 打印预测结果
//         printf("\n[结果] 识别结果:\n");
//         printf("预测数字: %d (实际数字: %d)\n", predicted_label, expected_digit);
//         printf("置信度: %.2f%%\n", max_prob * 100);
        
//         if (predicted_label == expected_digit) {
//             correct++;
//             printf("预测正确 ✓\n");
//         } else {
//             printf("预测错误 ✗\n");
//         }
        
//         printf("当前准确率: %d/%d = %.2f%%\n", correct, total, (float)correct/total*100);
        
//         // 打印所有类别的概率
//         printf("\n所有数字的概率分布:\n");
//         for (int i = 0; i < CLASS_NUMBER; i++) {
//             printf("%d: %.2f%%\n", i, probabilities[i] * 100);
//         }
//     }

//     /* 清理资源 */
//     kpu_model_free(&task);
//     acoral_free(model_data);
//     return 0;
// } 