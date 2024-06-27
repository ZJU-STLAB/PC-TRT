#!/usr/bin/env python3
import os

def delete_files():
    # 获取当前执行目录
    current_dir = os.getcwd()
    
    # 遍历当前目录下的所有文件
    for filename in os.listdir(current_dir):
        file_path = os.path.join(current_dir, filename)
        # 检查文件名是否以 .ll 结尾或者包含 driver
        if filename.endswith('.ll') or 'driver' in filename or 'instrumented' in filename:
            try:
                # 如果是文件，则删除
                if os.path.isfile(file_path):
                    os.remove(file_path)
                # 如果是目录，可以根据需要选择删除目录及其内容
                elif os.path.isdir(file_path):
                    import shutil
                    shutil.rmtree(file_path)
            except Exception as e:
                print(f"Failed to delete {file_path}: {e}")

if __name__ == "__main__":
    delete_files()