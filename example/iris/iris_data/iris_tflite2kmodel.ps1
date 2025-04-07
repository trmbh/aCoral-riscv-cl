# 设置Python环境
$env:PYTHONPATH = "..\..\third_party\nncase"

# 运行Python脚本
python tflite2kmodel.py

# 检查是否成功生成kmodel文件
if (Test-Path "iris.kmodel") {
    Write-Host "Successfully generated iris.kmodel"
} else {
    Write-Host "Failed to generate iris.kmodel"
    exit 1
} 