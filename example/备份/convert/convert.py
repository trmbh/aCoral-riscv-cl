import nncase
import os

def read_model_file(model_file):
    with open(model_file, 'rb') as f:
        model_content = f.read()
    return model_content

def main():
    # Get the directory where the script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))
    model = os.path.join(script_dir, 'mnist.tflite')
    target = 'k210'
    
    # Set output paths in test directory
    model_name = os.path.splitext(os.path.basename(model))[0]  # Get model name without extension
    output_kmodel = os.path.join(script_dir, f'{model_name}.kmodel')
    dump_dir = os.path.join(script_dir, 'tmp')

    # compile_options
    compile_options = nncase.CompileOptions()
    compile_options.target = target
    compile_options.dump_ir = True
    compile_options.dump_asm = True
    compile_options.dump_dir = dump_dir
    
    # Set input shape to match MNIST format (28x28 grayscale)
    compile_options.input_shape = [1, 28, 28, 1]  # NHWC format for grayscale
    compile_options.input_type = 'uint8'
    compile_options.output_type = 'float32'
    
    # Enable preprocessing and quantization
    compile_options.preprocess = True
    compile_options.quant_type = 'int8'  # Use int8 quantization for better compression
    compile_options.w_quant_type = 'int8'  # Weight quantization
    compile_options.input_range = [-128, 127]  # Input range for int8
    compile_options.mean = [127.5]  # Mean value for normalization
    compile_options.std = [127.5]   # Standard deviation for normalization
    
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
    
    # Verify kmodel size
    if len(kmodel) > 435000:  # KMODEL_SIZE from test_mnist.c
        print(f"Warning: Generated kmodel size ({len(kmodel)} bytes) exceeds expected size (435000 bytes)")
    
    # Save kmodel in test directory
    with open(output_kmodel, 'wb') as f:
        f.write(kmodel)
    
    print(f"Generated kmodel size: {len(kmodel)} bytes")
    print(f"Saved kmodel to: {output_kmodel}")
    print("Compilation completed successfully!")

if __name__ == '__main__':
    main()
