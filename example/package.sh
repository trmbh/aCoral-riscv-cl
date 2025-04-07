#!/bin/bash

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# 显示可用的示例目录
echo "可用的示例目录："
ls -d "$SCRIPT_DIR"/*/ 2>/dev/null | sed "s|$SCRIPT_DIR/||" | sed "s|/||"

# 提示用户输入
echo -e "\n请输入要打包的示例名称："
read EXAMPLE_NAME

# 检查输入是否为空
if [ -z "$EXAMPLE_NAME" ]; then
    echo "错误: 示例名称不能为空"
    exit 1
fi

# 检查示例目录是否存在
EXAMPLE_DIR="$SCRIPT_DIR/$EXAMPLE_NAME"
if [ ! -d "$EXAMPLE_DIR" ]; then
    echo "错误: 示例目录 $EXAMPLE_NAME 不存在"
    exit 1
fi

# 执行Python脚本
python "$SCRIPT_DIR/package.py" "$EXAMPLE_NAME" 