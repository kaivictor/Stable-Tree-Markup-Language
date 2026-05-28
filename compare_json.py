import json
import sys
import os

def compare_json_files(file1_path, file2_path):
    """
    比较两个JSON文件的内容是否相同
    """
    try:
        # 读取文件内容
        with open(file1_path, 'r', encoding='utf-8') as f1, \
             open(file2_path, 'r', encoding='utf-8') as f2:
            content1 = f1.read()
            content2 = f2.read()
        
        # 读取原始行用于行号定位
        with open(file1_path, 'r', encoding='utf-8') as f1:
            lines1 = f1.readlines()
        
        # 解析JSON
        data1 = json.loads(content1)
        data2 = json.loads(content2)
        
        # 方法1: 直接比较JSON对象
        if data1 == data2:
            return True, 0
        
        # 方法2: 比较去除所有空白字符后的内容
        # 移除所有空白字符（空格、换行、制表符等）
        stripped1 = ''.join(content1.split())
        stripped2 = ''.join(content2.split())
        
        if stripped1 == stripped2:
            return True, 0
        
        # 如果到这里，说明内容不同
        # 找出在去除空白字符后的第一个不同位置
        min_len = min(len(stripped1), len(stripped2))
        
        for i in range(min_len):
            if stripped1[i] != stripped2[i]:
                # 在原始内容中查找对应的行
                char_count = 0
                for line_num, line in enumerate(lines1, 1):
                    for char in line:
                        if char.strip():  # 如果不是空白字符
                            if char_count == i:
                                return False, line_num
                            char_count += 1
                return False, 1
        
        # 如果一个是另一个的前缀
        if len(stripped1) > len(stripped2):
            # 找到最后一个非空白字符所在的行
            char_count = 0
            for line_num, line in enumerate(lines1, 1):
                for char in line:
                    if char.strip():
                        if char_count == len(stripped2) - 1:
                            return False, line_num
                        char_count += 1
        
        return False, 1
        
    except json.JSONDecodeError as e:
        print(f"JSON解析错误: {e}")
        return False, 0
    except FileNotFoundError as e:
        print(f"文件不存在: {e}")
        return False, 0
    except Exception as e:
        print(f"比较过程中发生错误: {e}")
        return False, 0

def main():
    # 检查是否提供了两个文件路径参数
    if len(sys.argv) == 3:
        file1_path = sys.argv[1]
        file2_path = sys.argv[2]
    else:
        # 如果没有提供参数，使用硬编码的路径
        # 在这里修改为您的文件路径
        file1_path = "file1.json"
        file2_path = "file2.json"
        
        # 检查文件是否存在
        if not os.path.exists(file1_path) or not os.path.exists(file2_path):
            print("请通过命令行提供两个JSON文件路径，或修改代码中的文件路径")
            print("用法: python compare_json.py <file1> <file2>")
            return
    
    # 比较两个JSON文件
    is_same, diff_line = compare_json_files(file1_path, file2_path)
    
    if is_same:
        print("相同")
    else:
        if diff_line > 0:
            print(f"从第{diff_line}行开始不同")
        else:
            print("不同")

if __name__ == "__main__":
    main()