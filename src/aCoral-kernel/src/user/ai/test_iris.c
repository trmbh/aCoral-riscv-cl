// /* Copyright 2023 SPG.
//  *
//  * Licensed under the Apache License, Version 2.0 (the "License");
//  * you may not use this file except in compliance with the License.
//  * You may obtain a copy of the License at
//  *
//  *     http://www.apache.org/licenses/LICENSE-2.0
//  *
//  * Unless required by applicable law or agreed to in writing, software
//  * distributed under the License is distributed on an "AS IS" BASIS,
//  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  * See the License for the specific language governing permissions and
//  * limitations under the License.
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

// /* 模型相关参数定义 */
// #define KMODEL_SIZE (720)
// #define PLL0_OUTPUT_FREQ 800000000UL
// #define PLL1_OUTPUT_FREQ 400000000UL
// #define PLL2_OUTPUT_FREQ 45158400UL

// /* 全局变量声明 */
// uint8_t *model_data;
// kpu_model_context_t task1;
// volatile uint32_t g_ai_done_flag_iris;

// /* 鸢尾花数据集示例 - 每组数据依次为：花萼长度、花萼宽度、花瓣长度、花瓣宽度 */
// // const float features[] = {5.1, 3.8, 1.9, 0.4};  // 当前使用的示例 - 预期结果: setosa

// /* 其他鸢尾花示例数据（已注释） */

// // 示例1 - Setosa典型特征
// // const float features[] = {5.0, 3.5, 1.3, 0.3};  // 预期结果: setosa
// // 特征说明：短花瓣(1.3, 0.3)是山鸢尾的典型特征

// // 示例2 - Versicolor典型特征
// // const float features[] = {6.4, 2.9, 4.3, 1.3};  // 预期结果: versicolor
// // 特征说明：中等大小的花瓣(4.3, 1.3)是变色鸢尾的特征

// // 示例3 - Virginica典型特征
// // const float features[] = {7.7, 3.8, 6.7, 2.2};  // 预期结果: virginica
// // 特征说明：大花瓣(6.7, 2.2)是维吉尼亚鸢尾的特征

// // 示例4 - Setosa的另一个例子
// // const float features[] = {4.8, 3.4, 1.6, 0.2};  // 预期结果: setosa
// // 特征说明：短而窄的花瓣(1.6, 0.2)

// // 示例5 - Versicolor边界情况
// // const float features[] = {5.7, 2.8, 4.1, 1.3};  // 预期结果: versicolor
// // 特征说明：特征值接近versicolor和virginica的边界，但仍属于versicolor


// const char *labels[] = { "setosa", "versicolor", "virginica" };  // 分类标签

// /**
//  * @brief AI推理完成回调函数
//  * @param userdata 用户数据指针
//  */
// static void ai_done(void* userdata)
// {
//     g_ai_done_flag_iris = 1;
    
//     float *features;
//     size_t count;
//     kpu_get_output(&task1, 0, (uint8_t **)&features, &count);
//     count /= sizeof(float);

//     size_t i;
//     for (i = 0; i < count; i++)
//     {
//         if (i % 64 == 0)
//             printf("\n");
//         printf("%f, ", features[i]);
//     }

//     printf("\n");
// }

// /**
//  * @brief 找出数组中最大值的索引
//  * @param src 输入数组
//  * @param count 数组长度
//  * @return 最大值的索引
//  */
// size_t argmax(const float *src, size_t count)
// {
//     float max = FLT_MIN;
//     size_t max_id = 0, i;
//     for (i = 0; i < count; i++)
//     {
//         if (src[i] > max)
//         {
//             max = src[i];
//             max_id = i;
//         }
//     }

//     return max_id;
// }

// /**
//  * @brief 初始化系统时钟
//  */
// static void init_system_clock(void)
// {
//     /* Set CPU and AI clk */
//     sysctl_pll_set_freq(SYSCTL_PLL0, PLL0_OUTPUT_FREQ);
//     sysctl_pll_set_freq(SYSCTL_PLL1, PLL1_OUTPUT_FREQ);
//     sysctl_pll_set_freq(SYSCTL_PLL2, PLL2_OUTPUT_FREQ);
// }

// /**
//  * @brief 鸢尾花分类测试主函数
//  * @return 正常运行返回0，错误返回-1
//  */
// int test_iris(){
//     /* 初始化串口和中断 */
//     uarths_init();
//     plic_init();

//     /* 初始化系统时钟 */
//     init_system_clock();

//     /* 分配模型内存 */
//     model_data = (uint8_t *)acoral_malloc(KMODEL_SIZE);
//     if (model_data == NULL) {
//         ACORAL_LOG_ERROR("Failed to allocate memory for model\n");
//         return -1;
//     }

//     /* 初始化Flash并读取模型 */
//     ACORAL_LOG_TRACE("Flash Init Start\n");
//     w25qxx_init(3, 0);
//     w25qxx_enable_quad_mode();
//     w25qxx_read_data(0xC00000, model_data, KMODEL_SIZE, W25QXX_QUAD_FAST);

//     /* 加载并解析模型 */
//     ACORAL_LOG_TRACE("Loading KPU Model\n");
//     if (kpu_load_kmodel(&task1, model_data) != 0)
//     {
//         ACORAL_LOG_ERROR("Cannot load kmodel.\n");
//         acoral_free(model_data);
//         return -1;
//     }

//     /* 启用全局中断 */
//     sysctl_enable_irq();

//     /* 开始推理 */
//     ACORAL_LOG_TRACE("Starting Inference\n");
//     int j;
//     for (j = 0; j < 1; j++)
//     {
//         g_ai_done_flag_iris = 0;

//         if (kpu_run_kmodel(&task1, (const uint8_t *)features, DMAC_CHANNEL5, ai_done, NULL) != 0)
//         {
//             ACORAL_LOG_ERROR("Cannot run kmodel.\n");
//             acoral_free(model_data);
//             return -1;
//         }

//         /* 等待推理完成 */
//         while (!g_ai_done_flag_iris) {
//             msleep(1);
//         }

//         /* 获取并显示结果 */
//         float *output;
//         size_t output_size;
//         kpu_get_output(&task1, 0, (uint8_t **)&output, &output_size);
//         ACORAL_LOG_INFO("Prediction: %s\n", labels[argmax(output, output_size / sizeof(float))]);
//     }

//     /* 清理资源 */
//     acoral_free(model_data);
//     ACORAL_LOG_TRACE("Iris Test Complete\n");

//     return 0;
// }