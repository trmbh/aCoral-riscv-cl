import nncase
import os

def read_model_file(model_file):
    with open(model_file, 'rb') as f:
        model_content = f.read()
    return model_content

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    model = os.path.join(script_dir, 'mnist.tflite')
    target = 'k210'

    # compile_options
    compile_options = nncase.CompileOptions()
    compile_options.target = target
    compile_options.input_type = 'int8'  # 使用int8量化
    compile_options.preprocess = True # if False, the args below will unworked
    compile_options.swapRB = True
    compile_options.input_shape = [1,64,64,3] # 减小输入尺寸到64x64
    compile_options.input_layout = 'NHWC'
    compile_options.output_layout = 'NHWC'
    compile_options.mean = [127.5,127.5,127.5]  # int8量化时的均值
    compile_options.std = [128,128,128]  # int8量化时的标准差
    compile_options.input_range = [-128,127]  # int8的输入范围
    compile_options.letterbox_value = 0  # int8下的padding值
    compile_options.dump_ir = False  # 关闭IR导出以节省内存
    compile_options.dump_asm = False  # 关闭汇编导出以节省内存
    compile_options.dump_dir = 'tmp'

    # compiler
    compiler = nncase.Compiler(compile_options)

    # import_options
    import_options = nncase.ImportOptions()

    # import
    model_content = read_model_file(model)
    compiler.import_tflite(model_content, import_options)

    # compile
    compiler.compile()

    # kmodel
    kmodel = compiler.gencode_tobytes()
    output_model = os.path.join(script_dir, '0x300000.kmodel')  # 将模型保存为0x300000.kmodel，表明其在Flash中的地址
    with open(output_model, 'wb') as f:
        f.write(kmodel)
    print(f"Model saved to {output_model}, size: {len(kmodel)} bytes")
    print(f"Please flash it to address 0x300000")

if __name__ == '__main__':
    main()
