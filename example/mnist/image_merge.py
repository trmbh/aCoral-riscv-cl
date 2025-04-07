# make_flash_data.py
import numpy as np

# 创建一个大的二进制文件
with open('mnist_images.bin', 'wb') as f:
    # 为每个数字（0-9）创建一个4KB的扇区
    for i in range(10):
        # 创建一个4KB的全零数组
        sector = np.zeros(4096, dtype=np.uint8)
        # 在扇区开始位置放置28x28的图像数据
        # 这里假设图像数据文件名格式为 '0.raw', '1.raw' 等
        try:
            with open(f'{i}.raw', 'rb') as img:
                img_data = img.read(784)  # 28x28 = 784字节
                sector[:784] = np.frombuffer(img_data, dtype=np.uint8)
        except FileNotFoundError:
            print(f"警告: 找不到图像 {i}.raw")
        # 写入这个扇区
        f.write(sector.tobytes())