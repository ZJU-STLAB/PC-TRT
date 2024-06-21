#!/usr/bin/env python3
import binascii
import hashlib
import io
import string
import struct
import sys
import os
import subprocess

#klee = "/home/sin/download/klee-3.1/build/bin/klee"
klee = "klee"
timeout = 1000
version_no = 3

class KTestError(Exception):
    pass

class KTest:
    valid_chars = string.digits + string.ascii_letters + string.punctuation + ' '

    @staticmethod
    def fromfile(path):
        try:
            f = open(path, 'rb')
        except IOError:
            print('ERROR: file %s not found' % path)
            sys.exit(1)

        hdr = f.read(5)
        if len(hdr) != 5 or (hdr != b'KTEST' and hdr != b'BOUT\n'):
            raise KTestError('unrecognized file')
        version, = struct.unpack('>i', f.read(4))
        if version > version_no:
            raise KTestError('unrecognized version')
        numArgs, = struct.unpack('>i', f.read(4))
        args = []
        for i in range(numArgs):
            size, = struct.unpack('>i', f.read(4))
            args.append(str(f.read(size).decode(encoding='ascii')))

        if version >= 2:
            symArgvs, = struct.unpack('>i', f.read(4))
            symArgvLen, = struct.unpack('>i', f.read(4))
        else:
            symArgvs = 0
            symArgvLen = 0

        numObjects, = struct.unpack('>i', f.read(4))
        objects = []
        for i in range(numObjects):
            size, = struct.unpack('>i', f.read(4))
            name = f.read(size).decode('utf-8')
            size, = struct.unpack('>i', f.read(4))
            bytes = f.read(size)
            objects.append((name, bytes))

        # Create an instance
        b = KTest(version, path, args, symArgvs, symArgvLen, objects)
        return b
    
    def __init__(self, version, path, args, symArgvs, symArgvLen, objects):
        self.version = version
        self.path = path
        self.symArgvs = symArgvs
        self.symArgvLen = symArgvLen
        self.args = args
        self.objects = objects

    def __format__(self, format_spec):
        sio = io.StringIO()
        width = str(len(str(max(1, len(self.objects) - 1))))

        # print ktest info
        print('ktest file : %r' % self.path, file=sio)
        print('args       : %r' % self.args, file=sio)
        print('num objects: %r' % len(self.objects), file=sio)

        # format strings
        fmt = dict()
        fmt['name'] = "object {0:" + width + "d}: name: '{1}'"
        fmt['size'] = "object {0:" + width + "d}: size: {1}"
        fmt['int' ] = "object {0:" + width + "d}: int : {1}"
        fmt['uint'] = "object {0:" + width + "d}: uint: {1}"
        fmt['data'] = "object {0:" + width + "d}: data: {1}"
        fmt['hex' ] = "object {0:" + width + "d}: hex : 0x{1}"
        fmt['text'] = "object {0:" + width + "d}: text: {1}"

        # print objects
        for i, (name, data) in enumerate(self.objects):
            def p(key, arg): print(fmt[key].format(i, arg), file=sio)

            blob = data.rstrip(b'\x00') if format_spec.endswith('trimzeros') else data
            txt = ''.join(c if c in self.valid_chars else '.' for c in blob.decode('ascii', errors='replace').replace('�', '.'))
            size = len(data)

            p('name', name)
            p('size', size)
            p('data', blob)
            p('hex', binascii.hexlify(blob).decode('ascii'))
            for n, m in [(1, 'b'), (2, 'h'), (4, 'i'), (8, 'q')]:
                if size == n:
                    p('int', struct.unpack(m, data)[0])
                    p('uint', struct.unpack(m.upper(), data)[0])
                    break
            p('text', txt)

        return sio.getvalue()

    def extract(self, object_names, trim_zeros):
        extracted_objects = set()
        for name, data in self.objects:
            if name not in object_names:
                continue

            f = open(self.path + '.' + name, 'wb')
            blob = data.rstrip(b'\x00') if trim_zeros else data
            f.write(blob)
            f.close()
            extracted_objects.add(name)
        missing_objects = list(object_names - extracted_objects)
        missing_objects.sort()
        if missing_objects:
            sys.exit(f'Could not find object{"s"[:len(missing_objects)^1]}: {", ".join(missing_objects)}')

    def to_dict(self):
        object_list = []
        for i, (name, data) in enumerate(self.objects):
            # blob = int.from_bytes(data.rstrip(b'\x00'), 'little')
            size = len(data)
            trimmed_data = data#.rstrip(b'\x00')
            blob = []
            type_str = 'int'
            if size == 1:  # Convert to char
                blob = chr(trimmed_data[0])
                type_str = 'char'
            elif size == 4:  # Convert to int
                # 转成有符号整数
                blob = int.from_bytes(trimmed_data, 'little', signed=True)
            else:  # Interpret as string or list of ints
                if all(0 <= b < 128 for b in trimmed_data) and size % 4 != 0:  # All bytes are in ASCII range
                    trimmed_data = data.rstrip(b'\x00')
                    blob = ''.join(chr(b) for b in trimmed_data)
                    type_str = 'string'
                else:  # Interpret as list of ints
                    for i in range(0, len(trimmed_data), 4):
                        chunk = trimmed_data[i:i+4]
                        blob.append(int.from_bytes(chunk, 'little', signed=True))
                    type_str = 'int array'
            # 转换成str
            blob = str(blob)
            obj_dict = {'name': name, 'data': blob, 'type': type_str}
            object_list.append(obj_dict)
        return object_list
    
    def data_hash(self):
        # 创建一个hash对象
        hash_obj = hashlib.sha256()
        # 遍历所有objects，更新hash值
        for _, data in sorted(self.objects):  # 对objects按name排序以保证顺序一致性
            hash_obj.update(data)
        # 返回十六进制表示的哈希值
        return hash_obj.hexdigest()

# 从KTest读取testcase
def parse_ktest(ktest, i):
    test_case = {"inputs":ktest.to_dict(), "outputs":{}, "description": "test case {} generated".format(i), "reserved": False}
    return test_case

def dump_ktests(ktests):
    test_cases = []
    for i, ktest in enumerate(ktests):
        test_case = parse_ktest(ktest, i)
        test_cases.append(test_case)
    test_suite = {"coverage": 0.0, "description": "new test suite generated", "executed": False, "funcName": "test", "srcFile": "./test.c", "testCases": test_cases}
    with open("test_suite_generated.json", "w") as f:
        # 将test_suite写入文件,使用json格式
        import json
        json.dump(test_suite, f, indent=4)

# 使用klee对IR文件进行符号执行,返回生成的ktest文件
def klee_execute_ir_files(ir_files):
    i = 0
    ktest_files = []
    for ir_file in ir_files:
        # 检查输出目录是否存在，若存在则删除
        # 获取ir_file所在目录地址，加上klee-out-i生成输出目录
        output_dir = os.path.join(os.path.dirname(ir_file), f"klee-out-{i}")
        i = i + 1
        # 生成klee执行命令
        klee_cmd = f"{klee} -optimize -output-dir={output_dir} {ir_file}"
        if os.path.exists(output_dir):
            os.system(f"rm -rf {output_dir}")
        # 使用subprocess执行klee_cmd并计时，超过10秒强制停止，并且不显示klee的输出
        klee_process = subprocess.Popen(klee_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            klee_process.wait(timeout)
        except subprocess.TimeoutExpired:
            klee_process.kill()
            print(f"KLEE执行超时: {ir_file}")
            continue
        print(f"KLEE执行成功: {ir_file}")
        # 遍历klee输出目录中,获取所有的.ktest文件名
        ktest_files.append([os.path.join(output_dir, f) for f in os.listdir(output_dir) if f.endswith(".ktest")])
    # 对每个ktest文件,解析成KTest
    ktests = []
    for ktest_file in ktest_files:
        for file in ktest_file:
            ktests.append(KTest.fromfile(file))
    # 根据每个KTest中的objects去重
    unique_hash = set()
    ktests = [ktest for ktest in ktests if ktest.data_hash() not in unique_hash and not unique_hash.add(ktest.data_hash())]
    # 删除klee输出目录
    i = 0
    for ir_file in ir_files:
        output_dir = os.path.join(os.path.dirname(ir_file), f"klee-out-{i}")
        i = i + 1
        if os.path.exists(output_dir):
            os.system(f"rm -rf {output_dir}")
    return ktests

if __name__ == "__main__":
    # 从命令行参数获取IR文件名
    ir_files = sys.argv[1:]
    # 检查每个IR文件名对应的文件是否存在且后缀为.ll,将有效的文件加入待分析列表
    valid_ir_files = []
    for ir_file in ir_files:
        if not ir_file.endswith(".ll"):
            print(f"文件{ir_file}不是LLVM IR文件")
        elif not os.path.exists(ir_file):
            print(f"文件{ir_file}不存在")
        else:
            valid_ir_files.append(ir_file)
    # 执行KLEE符号执行
    ktests = klee_execute_ir_files(valid_ir_files)
    dump_ktests(ktests)
