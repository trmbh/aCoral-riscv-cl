import os
import numpy as np
import pandas as pd
from nncase import Compiler
from nncase.quantization import Quantizer
from nncase.quantization import *
from nncase.quantization.calibrate import Calibrator

def load_iris_data(csv_path):
    # 读取CSV文件
    df = pd.read_csv(csv_path)
    # 提取特征列（前4列）
    features = df.iloc[:, :4].values
    # 归一化数据
    features = (features - np.mean(features, axis=0)) / np.std(features, axis=0)
    return features

def main():
    # 设置路径
    tflite_path = "iris.tflite"
    kmodel_path = "iris.kmodel"
    csv_path = "iris.csv"
    
    # 加载校准数据
    calibration_data = load_iris_data(csv_path)
    
    # 创建量化器
    quantizer = Quantizer()
    
    # 设置量化参数
    quantizer.set_quantization_scheme('uint8')
    quantizer.set_calibration_data(calibration_data)
    
    # 加载TFLite模型
    with open(tflite_path, 'rb') as f:
        tflite_model = f.read()
    
    # 转换模型
    compiler = Compiler()
    compiler.import_tflite(tflite_model)
    
    # 量化模型
    compiler.quantize(quantizer)
    
    # 编译模型
    kmodel = compiler.compile()
    
    # 保存kmodel
    with open(kmodel_path, 'wb') as f:
        f.write(kmodel)
    
    print(f"Model converted and quantized successfully. Saved to {kmodel_path}")

if __name__ == "__main__":
    main() 