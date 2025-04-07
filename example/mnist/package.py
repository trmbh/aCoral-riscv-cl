# make_flash_data.py
import numpy as np
import os

def preprocess_image(img):
    """预处理图像数据"""
    # 确保图像是28x28的灰度图
    if img.shape != (28, 28):
        raise ValueError(f"图像大小必须是28x28，当前大小: {img.shape}")
    
    # 确保数据类型是uint8
    if img.dtype != np.uint8:
        img = img.astype(np.uint8)
    
    # 确保像素值在0-255范围内
    img = np.clip(img, 0, 255)
    
    return img

def create_flash_data():
    """创建用于烧录的二进制数据"""
    # 创建一个大的二进制文件
    with open('mnist_images.bin', 'wb') as f:
        # 为每个数字（0-9）创建一个4KB的扇区
        for i in range(10):
            # 创建一个4KB的全零数组
            sector = np.zeros(4096, dtype=np.uint8)
            
            # 读取并预处理图像
            try:
                # 支持多种图像格式
                if os.path.exists(f'{i}.png'):
                    img = np.array(Image.open(f'{i}.png').convert('L'))
                elif os.path.exists(f'{i}.jpg'):
                    img = np.array(Image.open(f'{i}.jpg').convert('L'))
                elif os.path.exists(f'{i}.raw'):
                    img = np.fromfile(f'{i}.raw', dtype=np.uint8).reshape(28, 28)
                else:
                    print(f"警告: 找不到图像 {i} (尝试 .png, .jpg, .raw)")
                    continue
                
                # 预处理图像
                img = preprocess_image(img)
                
                # 将图像数据写入扇区
                sector[:784] = img.flatten()
                print(f"处理图像 {i}: 形状={img.shape}, 类型={img.dtype}, "
                      f"值范围=[{img.min()}, {img.max()}]")
                
            except Exception as e:
                print(f"处理图像 {i} 时出错: {str(e)}")
                continue
            
            # 写入这个扇区
            f.write(sector.tobytes())

if __name__ == '__main__':
    create_flash_data()
    print("\n生成烧录文件: mnist_images.bin")
    print("使用以下命令烧录:")
    print("kflash -p COM3 -b 2000000 mnist_images.bin -a 0xD00000")