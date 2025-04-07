import os
import subprocess
import numpy as np
import tempfile
import shutil
from PIL import Image

def print_image_info(data):
    """打印图像信息"""
    min_val = 255
    max_val = 0
    avg_val = 0
    non_zero = 0

    for i in range(28 * 28):
        if data[i] < min_val:
            min_val = data[i]
        if data[i] > max_val:
            max_val = data[i]
        avg_val += data[i]
        if data[i] > 0:
            non_zero += 1
    
    avg_val /= (28 * 28)

    print("\n图像统计信息:")
    print(f"最小像素值: {min_val}")
    print(f"最大像素值: {max_val}")
    print(f"平均像素值: {avg_val:.2f}")
    print(f"非零像素数: {non_zero} ({non_zero * 100 / (28 * 28):.1f}%)")

    print("\n图像预览 (o表示非零像素):")
    for y in range(28):
        for x in range(28):
            print('o' if data[y * 28 + x] > 0 else '.', end='')
        print()

def softmax(x):
    """计算softmax"""
    x = np.array(x)
    max_x = np.max(x)
    exp_x = np.exp(x - max_x)
    return exp_x / np.sum(exp_x)

current_dir = os.path.dirname(os.path.abspath(__file__))
raw_dir = os.path.join(current_dir, 'raw_images')
kmodel_path = os.path.join(current_dir, 'mnist_k210.kmodel')
ncc_path = r'F:\zw\K210\ncc_win_x86_64\ncc.exe'

# Get all raw files and sort them
raw_files = sorted([f for f in os.listdir(raw_dir) if f.endswith('.raw')])[:5]  # Only take first  files
total = len(raw_files)
correct = 0

# Create a temporary directory for outputs
temp_dir = tempfile.mkdtemp()

try:
    for i, raw_file in enumerate(raw_files):
        # Get expected label from filename (format: *_X.raw)
        expected_label = int(raw_file.split('_')[-1].split('.')[0])
        raw_path = os.path.join(raw_dir, raw_file)
        
        # 读取并显示图像信息
        with open(raw_path, 'rb') as f:
            image_data = np.frombuffer(f.read(), dtype=np.uint8)
        print(f"\n处理图像 {raw_file}, 期望数字: {expected_label}")
        print_image_info(image_data)
        
        # Run inference using ncc command
        cmd = [
            ncc_path, 'infer',
            kmodel_path,
            temp_dir,
            '--dataset', raw_path,
            '--dataset-format', 'raw',
            '--input-mean', '0',
            '--input-std', '255'
        ]
        try:
            print("\n运行模型:")
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                # Try to read the output file
                output_files = os.listdir(temp_dir)
                if output_files:
                    output_path = os.path.join(temp_dir, output_files[0])
                    with open(output_path, 'rb') as f:
                        output_data = np.frombuffer(f.read(), dtype=np.float32)
                    
                    # 打印原始输出
                    print("\n原始模型输出:")
                    for j, value in enumerate(output_data):
                        print(f"{j}: {value:.6f}")
                    
                    # 计算softmax概率
                    probabilities = softmax(output_data)
                    predicted_label = np.argmax(probabilities)
                    confidence = probabilities[predicted_label] * 100
                    
                    print(f"\n识别结果详情:")
                    print(f"预测数字: {predicted_label} (置信度: {confidence:.2f}%)")
                    
                    print("\n所有数字的概率分布:")
                    for j, prob in enumerate(probabilities):
                        print(f"{j}: {prob * 100:.2f}%")
                    
                    if predicted_label == expected_label:
                        correct += 1
                else:
                    print("错误：未找到输出文件")
            else:
                print("错误:", result.stderr)
            
        except Exception as e:
            print(f"处理图像 {raw_file} 时发生错误: {str(e)}")
        
        # Clean output directory for next inference
        for f in os.listdir(temp_dir):
            os.remove(os.path.join(temp_dir, f))

finally:
    # Clean up temporary directory
    shutil.rmtree(temp_dir)

print(f"\n准确率: {correct}/{total} = {correct/total*100:.2f}%") 