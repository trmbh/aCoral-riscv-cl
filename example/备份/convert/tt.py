import os
import numpy as np
import nncase

def read_model_file(model_file):
    """
    read model
    """
    with open(model_file, 'rb') as f:
        model_content = f.read()
    return model_content

def run_kmodel(kmodel_path, input_data):
    """
    Run kmodel and get output
    """
    print("\n---------start run kmodel---------")
    print("Load kmodel...")
    model_sim = nncase.Simulator()
    with open(kmodel_path, 'rb') as f:
        model_sim.load_model(f.read())
    
    print("Set input data...")
    for i, p_d in enumerate(input_data):
        model_sim.set_input_tensor(i, nncase.RuntimeTensor.from_numpy(p_d))
    
    print("Run...")
    model_sim.run()
    
    print("Get output result...")
    return [model_sim.get_output_tensor(i).to_numpy() 
            for i in range(model_sim.outputs_size)]

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    model = os.path.join(script_dir, '20classes_yolo.tflite')
    target = 'k210'

    # compile_options
    compile_options = nncase.CompileOptions()
    compile_options.target = target
    compile_options.input_type = 'int8'
    compile_options.preprocess = True
    compile_options.swapRB = True
    compile_options.input_shape = [1, 32, 32, 3]
    compile_options.input_layout = 'NHWC'
    compile_options.output_layout = 'NHWC'
    compile_options.mean = [127.5, 127.5, 127.5]
    compile_options.std = [128, 128, 128]
    compile_options.input_range = [-128, 127]
    compile_options.letterbox_value = 0
    compile_options.dump_ir = False
    compile_options.dump_asm = False
    compile_options.dump_dir = 'tmp'

    print("Compiling model...")
    compiler = nncase.Compiler(compile_options)

    # import model
    print("Importing model...")
    model_content = read_model_file(model)
    compiler.import_tflite(model_content)

    # compile
    print("Compiling...")
    compiler.compile()

    # generate kmodel
    print("Generating kmodel...")
    kmodel = compiler.gencode_tobytes()
    output_model = os.path.join(script_dir, '20classes_yolo.kmodel')
    with open(output_model, 'wb') as f:
        f.write(kmodel)
    
    model_size = len(kmodel)
    print(f"Model saved to {output_model}")
    print(f"Model size: {model_size/1024/1024:.2f} MB")
    print(f"Please copy the model to /sd/20classes_yolo.kmodel on your SD card")

    # quick test
    test_input = np.zeros((1, 32, 32, 3), dtype=np.int8)
    try:
        results = run_kmodel(output_model, [test_input])
        print(f"Model test passed. Output shapes: {[r.shape for r in results]}")
    except Exception as e:
        print(f"Model test failed: {str(e)}")

if __name__ == '__main__':
    main()
