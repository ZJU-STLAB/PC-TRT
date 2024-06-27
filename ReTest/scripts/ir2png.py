#!/usr/bin/env python3
import os
import subprocess
import argparse

def check_executable(program):
    try:
        subprocess.check_output(["which", program])
        return True
    except subprocess.CalledProcessError:
        return False

def generate_cfg_png(ir_file, function_name):
    # 检测opt可执行程序
    if not check_executable("opt-13"):
        print("无法找到opt可执行程序")
        return
    
    opt_cmd = f"opt-13 -dot-cfg -enable-new-pm=0 {ir_file}"
    # 生成cfg的dot文件
    dot_file = f".{function_name}.dot"
    subprocess.run(opt_cmd, shell=True)

    # 检测dot可执行程序
    if not check_executable("dot"):
        print("无法找到dot可执行程序")
        return

    # 生成png图像文件
    png_file = f"{function_name}.png"
    dot_cmd = f"dot -Tpng -o {png_file} {dot_file}"
    subprocess.run(dot_cmd, shell=True)

    # 删除dot文件
    os.remove(dot_file)

    print(f"生成png图像文件成功: {png_file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate CFG PNG")
    parser.add_argument("ir_file", help="IR文件路径")
    parser.add_argument("function_name", help="函数名")
    args = parser.parse_args()

    ir_file = args.ir_file
    function_name = args.function_name
    generate_cfg_png(ir_file, function_name)