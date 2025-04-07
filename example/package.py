import os
import shutil
import zipfile
import sys
from pathlib import Path

def create_kfpkg(example_name):
    # 获取当前脚本所在目录
    current_dir = Path(__file__).parent
    
    # 构建示例目录路径
    example_dir = current_dir / example_name
    
    # 检查示例目录是否存在
    if not example_dir.exists():
        print(f"错误: 示例目录 {example_name} 不存在")
        return
    
    # 定义需要打包的文件
    files_to_package = [
        # example_dir / f"{example_name}_k210.kmodel",  # kmodel文件
        example_dir / f"{example_name}.kmodel",  # kmodel文件
        example_dir / "flash-list.json",         # flash-list.json
        current_dir.parent / "build" / "acoral-kernel.bin"  # 从根目录获取kernel
    ]
    
    # 为MNIST示例添加特殊处理
    if example_name == "mnist":
        mnist_images = example_dir / "mnist_images.bin"
        if mnist_images.exists():
            files_to_package.append(mnist_images)
        else:
            print("警告: mnist_images.bin 不存在，请先运行 make_flash_data.py 生成图片数据")
            return
    
    # 检查所有文件是否存在
    for file in files_to_package:
        if not file.exists():
            print(f"错误: 文件 {file} 不存在")
            return
    
    # 创建临时目录
    temp_dir = example_dir / "temp_package"
    if temp_dir.exists():
        shutil.rmtree(temp_dir)
    temp_dir.mkdir()
    
    # 复制文件到临时目录
    for file in files_to_package:
        shutil.copy2(file, temp_dir)
    
    # 创建zip文件
    output_file = example_dir / f"{example_name}.kfpkg"
    with zipfile.ZipFile(output_file, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for file in temp_dir.glob("*"):
            zipf.write(file, file.name)
    
    # 清理临时目录
    shutil.rmtree(temp_dir)
    print(f"打包完成: {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("使用方法: python package.py <示例名称>")
        print("例如: python package.py iris")
        print("      python package.py mnist")
        sys.exit(1)
    
    example_name = sys.argv[1]
    create_kfpkg(example_name) 