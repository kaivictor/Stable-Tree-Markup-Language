"""
检查不同文件内容之间的一次性
"""

PROJECT_ROOT = r"F:\Studio\Project\my_graduation_project2\Language"
import sys
import os
# stml_py 在上级目录
sys.path.insert(0, os.path.join(PROJECT_ROOT, "stml_py", "src"))


import shutil
import yaml
import json
import stml

def merge_yaml_documents(yaml_docs):
    """将多文档 YAML 字符串合并为一个字典"""
    merged_dict = {}
    
    # 使用 safe_load_all 加载所有文档
    for doc in yaml_docs:
        if isinstance(doc, dict):
            # 递归合并字典
            def deep_merge(base, update):
                for key, value in update.items():
                    if key in base and isinstance(base[key], dict) and isinstance(value, dict):
                        deep_merge(base[key], value)
                    else:
                        base[key] = value
            
            deep_merge(merged_dict, doc)
    
    return merged_dict

def convert_to_strings(obj):
    """递归地将所有值转换为字符串"""
    if isinstance(obj, dict):
        return {key: convert_to_strings(value) for key, value in obj.items()}
    elif isinstance(obj, list):
        return [convert_to_strings(item) for item in obj]
    elif obj is None:
        return "null"
    elif isinstance(obj, bool):
        return str(obj).lower()  # true/false
    else:
        return str(obj)


def check_json_and_yaml(json_file, yaml_file, test_model=False):
    # 检查文件是否存在
    if not os.path.exists(json_file) or not os.path.exists(yaml_file):
        return None
    with open(json_file, 'r', encoding='utf-8') as f:
        json_content = json.load(f)
    with open(yaml_file, 'r', encoding='utf-8') as f:
        yaml_content = list(yaml.safe_load_all(f))
        # yaml_content = yaml.safe_load_all(f)
        yaml_content = merge_yaml_documents(yaml_content)
    # 将所有值转换为字符串
    json_str = convert_to_strings(json_content)
    yaml_str = convert_to_strings(yaml_content)
    if test_model:
        print("[JSON]", "=" * 20)
        print(json_content)
        print("[YAML]", "=" * 20)
        print(yaml_content)
    return json_content == yaml_content, json_str == yaml_str


def check_stml_and_yaml(stml_file, yaml_file, test_model=False):
    # 检查文件是否存在
    if not os.path.exists(stml_file) or not os.path.exists(yaml_file):
        return None
    with open(stml_file, 'r', encoding='utf-8') as f:
        stml_content, warn = stml.loads(f.read())
        if stml_content:
            stml_content = stml_content["docs"]
            stml_content = merge_yaml_documents(stml_content)
    with open(yaml_file, 'r', encoding='utf-8') as f:
        yaml_content = list(yaml.safe_load_all(f))
        yaml_content = merge_yaml_documents(yaml_content)
    # 将所有值转换为字符串
    stml_str = convert_to_strings(stml_content)
    yaml_str = convert_to_strings(yaml_content)
    if test_model:
        print("[STML]", "=" * 20)
        print(stml_content)
        print("[YAML]", "=" * 20)
        print(yaml_content)
    return stml_content == yaml_content, stml_str == yaml_str


def check_stml_and_json(stml_file, json_file, test_model=False):
    # 检查文件是否存在
    if not os.path.exists(stml_file) or not os.path.exists(json_file):
        return None
    with open(stml_file, 'r', encoding='utf-8') as f:
        stml_content, warn = stml.loads(f.read())
        if stml_content:
            stml_content = stml_content["docs"]
            # stml_content = merge_yaml_documents(stml_content)
    with open(json_file, 'r', encoding='utf-8') as f:
        json_content = json.load(f)
    # 将所有值转换为字符串
    json_str = convert_to_strings(json_content)
    stml_str = convert_to_strings(stml_content)
    if test_model:
        print("[JSON]", "=" * 20)
        print(json_content)
        print("[STML]", "=" * 20)
        print(stml_content)
    return json_content == stml_content, json_str == stml_str



########## 第一类测试 ##########
FIRST_GROUP_SIZE = 30

def group_json_yaml(folder_path, group_number):
    valid_count = 0
    same_count = 0
    loose_same_count = 0
    for group_index in range(1, FIRST_GROUP_SIZE + 1):
        json_file_name = f"group_{group_number}_{group_index}.json"
        yaml_file_name = f"group_{group_number}_{group_index}.yaml"
        json_file_path = os.path.join(folder_path, json_file_name)
        yaml_file_path = os.path.join(folder_path, yaml_file_name)
        
        check_result = check_json_and_yaml(json_file_path, yaml_file_path, False)
        
        if check_result==None:
            # print(f"文件不存在: \n{json_file_path}\n{yaml_file_path}")
            continue

        valid_count += 1
        if check_result[0]==True:
            # print(f"文件一致: \n{json_file_path}\n{yaml_file_path}")
            same_count += 1
        if check_result[1]==True:
            # print(f"文件宽松一致: \n{json_file_path}\n{yaml_file_path}")
            loose_same_count += 1

    print(f"有效文件对数: {valid_count}, 相同文件对数: {same_count}, 宽松相同文件对数: {loose_same_count}")

def group_stml_yaml(folder_path, group_number):
    valid_count = 0
    same_count = 0
    loose_same_count = 0
    for group_index in range(1, FIRST_GROUP_SIZE + 1):
        stml_file_name = f"group_{group_number}_{group_index}.stml"
        yaml_file_name = f"group_{group_number}_{group_index}.yaml"
        stml_file_path = os.path.join(folder_path, stml_file_name)
        yaml_file_path = os.path.join(folder_path, yaml_file_name)
        
        check_result = check_stml_and_yaml(stml_file_path, yaml_file_path, False)
        
        if check_result==None:
            # print(f"文件不存在: \n{stml_file_path}\n{yaml_file_path}")
            continue

        valid_count += 1
        if check_result[0]==True:
            # print(f"文件一致: \n{stml_file_path}\n{yaml_file_path}")
            same_count += 1
        if check_result[1]==True:
            # print(f"文件宽松一致: \n{stml_file_path}\n{yaml_file_path}")
            loose_same_count += 1

    print(f"有效文件对数: {valid_count}, 相同文件对数: {same_count}, 宽松相同文件对数: {loose_same_count}")


def group_json_stml(folder_path, group_number):
    valid_count = 0
    same_count = 0
    loose_same_count = 0
    for group_index in range(1, FIRST_GROUP_SIZE + 1):
        json_file_name = f"group_{group_number}_{group_index}.json"
        stml_file_name = f"group_{group_number}_{group_index}.stml"
        json_file_path = os.path.join(folder_path, json_file_name)
        stml_file_path = os.path.join(folder_path, stml_file_name)
        
        check_result = check_stml_and_json(stml_file_path, json_file_path, True)
        
        if check_result==None:
            # print(f"文件不存在: \n{json_file_path}\n{stml_file_path}")
            continue

        valid_count += 1
        if check_result[0]==True:
            # print(f"文件一致: \n{json_file_path}\n{stml_file_path}")
            same_count += 1
        if check_result[1]==True:
            # print(f"文件宽松一致: \n{json_file_path}\n{stml_file_path}")
            loose_same_count += 1

    print(f"有效文件对数: {valid_count}, 相同文件对数: {same_count}, 宽松相同文件对数: {loose_same_count}")



if __name__ == "__main__":
    group_json_yaml(PROJECT_ROOT + "\\EffectsInLargeModels2\\test_data2", 2)
    group_stml_yaml(PROJECT_ROOT + "\\EffectsInLargeModels2\\test_data2", 3)
    group_json_stml(PROJECT_ROOT + "\\EffectsInLargeModels2\\test_data2", 1)

