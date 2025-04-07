#!/bin/bash

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# 定义等待用户按回车键退出的函数
wait_for_exit() {
    echo -e "\n按回车键退出..."
    read -r
    exit 1
}

# 检查ncc命令是否可用
if ! command -v ncc &> /dev/null; then
    echo "错误: 未找到ncc命令，请确保已正确安装NNCase"
    wait_for_exit
fi

# 显示可用的模型文件
echo "可用的模型文件："
ls -d "$SCRIPT_DIR"/*/ 2>/dev/null | sed "s|$SCRIPT_DIR/||" | sed "s|/||"

# 提示用户输入模型名称
echo -e "\n请输入要转换的模型名称（不需要输入.tflite后缀）："
read MODEL_NAME

# 检查输入是否为空
if [ -z "$MODEL_NAME" ]; then
    echo "错误: 模型名称不能为空"
    wait_for_exit
fi

# 构建文件路径（使用相对路径）
MODEL_FILE="$SCRIPT_DIR/$MODEL_NAME/$MODEL_NAME.tflite"
OUTPUT_FILE="$SCRIPT_DIR/$MODEL_NAME/$MODEL_NAME.kmodel"
DATASET_DIR="$SCRIPT_DIR/$MODEL_NAME/images"

# 检查模型文件是否存在
if [ ! -f "$MODEL_FILE" ]; then
    echo "错误: 模型文件 $MODEL_FILE 不存在"
    wait_for_exit
fi

# 检查输出目录是否存在
OUTPUT_DIR="$(dirname "$OUTPUT_FILE")"
if [ ! -d "$OUTPUT_DIR" ]; then
    echo "创建输出目录: $OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"
fi

# 执行转换命令
echo "开始转换模型..."
echo "输入文件: $MODEL_FILE"
echo "输出文件: $OUTPUT_FILE"

#浮点数进行推理
if [ "$MODEL_NAME" == "iris" ]; then
    echo "使用浮点数推理模式..."
    ncc compile "$MODEL_FILE" "$OUTPUT_FILE" -i tflite -o kmodel -t k210 --inference-type float #保持模型的浮点数计算精度
elif [ "$MODEL_NAME" == "20classes_yolo" ]; then
    echo "使用YOLO模型转换模式..."
    if [ ! -d "$DATASET_DIR" ]; then
        echo "错误: 数据集目录 $DATASET_DIR 不存在"
        echo "请确保在模型目录下创建 images 文件夹并放入至少一张图片"
        wait_for_exit
    fi
    ncc compile "$MODEL_FILE" "$OUTPUT_FILE" -i tflite -o kmodel -t k210 --dataset "$DATASET_DIR"
elif [ "$MODEL_NAME" == "mnist" ]; then
    echo "使用MNIST模型转换模式..."
    if [ ! -d "$DATASET_DIR" ]; then
        echo "错误: 数据集目录 $DATASET_DIR 不存在"
        echo "请确保在模型目录下创建 images 文件夹并放入至少一张MNIST测试图片"
        wait_for_exit
    fi
    if [ ! "$(ls -A "$DATASET_DIR")" ]; then
        echo "错误: 数据集目录为空"
        echo "请确保在 images 文件夹中放入至少一张MNIST测试图片"
        wait_for_exit
    fi
    ncc compile "$MODEL_FILE" "$OUTPUT_FILE" -i tflite -o kmodel -t k210 --dataset "$DATASET_DIR"
else
    echo "使用默认转换模式..."
    ncc compile "$MODEL_FILE" "$OUTPUT_FILE" -i tflite -o kmodel -t k210
fi

# 检查转换是否成功
if [ $? -eq 0 ]; then
    echo "转换成功: $OUTPUT_FILE"
    echo "输出文件大小: $(ls -lh "$OUTPUT_FILE" | awk '{print $5}')"
else
    echo "错误: 模型转换失败"
    wait_for_exit
fi

# 等待用户按回车键退出
echo -e "\n按回车键退出..."
read -r