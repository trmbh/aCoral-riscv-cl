# 检查参数
if ($args.Count -eq 0) {
    Write-Host "用法: .\mnist_tflite2kmodel.ps1 mnist.tflite"
    exit 1
}

# 获取输入文件名（不含扩展名）
$inputFile = $args[0]
$name = [System.IO.Path]::GetFileNameWithoutExtension($inputFile)
$outputFile = "$name.kmodel"

# 检查输入文件是否存在
if (-not (Test-Path $inputFile)) {
    Write-Host "错误: 输入文件 $inputFile 不存在"
    exit 1
}

# 检查ncc工具是否存在
$nccPath = ".\ncc\ncc.exe"
if (-not (Test-Path $nccPath)) {
    Write-Host "错误: ncc工具不存在，请确保ncc.exe位于.\ncc\目录下"
    exit 1
}

# 执行转换
Write-Host "正在转换 $inputFile 到 $outputFile ..."
& $nccPath -i tflite -o k210model `
    --dataset /images `
    --input-mean 0 `
    --input-std 255 `
    --input-type uint8 `
    --output-type float32 `
    --input-shape "1,28,28,1" `
    --target k210 `
    --quantize `
    $inputFile $outputFile

if ($LASTEXITCODE -eq 0) {
    Write-Host "转换完成！输出文件: $outputFile"
} else {
    Write-Host "转换失败！请检查错误信息"
    exit 1
} 