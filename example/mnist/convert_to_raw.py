import numpy as np
import os
from PIL import Image

# 设置目录
current_dir = os.path.dirname(os.path.abspath(__file__))
input_dir = os.path.join(current_dir, 'images/test')
output_dir = os.path.join(current_dir, 'raw_images')
os.makedirs(output_dir, exist_ok=True)

# 获取所有PNG文件
png_files = sorted([f for f in os.listdir(input_dir) if f.endswith('.png')])
png_files = png_files[:30]  # 处理第11-30个文件

results = []
for png_file in png_files:
    # 读取PNG图片
    img_path = os.path.join(input_dir, png_file)
    img = Image.open(img_path).convert('L')  # 确保是灰度图
    img_array = np.array(img)
    
    # 确保图片是uint8类型
    img_array = img_array.astype(np.uint8)
    
    # 扩展数据维度到(1,28,28,1)
    img_array = np.expand_dims(img_array, axis=(0, -1))
    
    # 确保数据是连续的C顺序
    img_array = np.ascontiguousarray(img_array)
    
    # 使用原始文件名（只改变扩展名）保存为raw格式
    raw_filename = os.path.splitext(png_file)[0] + '.raw'
    output_path = os.path.join(output_dir, raw_filename)
    
    # 保存数据
    img_array.tofile(output_path)
    
    # 从文件名中提取标签（格式为XXXXX_Y.png）
    label = int(png_file.split('_')[1].split('.')[0])
    
    results.append((png_file, label, raw_filename))

print("\nConverting PNG images to RAW format:")
print("-" * 70)
print("Original PNG  Label  RAW Filename")
print("-" * 70)

for png_file, label, raw_file in results:
    print(f"{png_file}  {label:5d}  {raw_file}")

print("-" * 70)
print(f"\nAll images have been saved to: {output_dir}")
print("Note: Each image is now in (1,28,28,1) format (3136 bytes per image)")

# 验证生成的文件大小
print("\nVerifying file sizes:")
for png_file, label, raw_file in results:
    file_path = os.path.join(output_dir, raw_file)
    file_size = os.path.getsize(file_path)
    print(f"{raw_file}: {file_size} bytes") 