"""
从大模型生成的内容中检查格式的有效性
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

def check_yaml(files_folder, backup_folder):
    # 获取所有 yaml 文件
    yaml_files = [f for f in os.listdir(files_folder) if f.endswith('.yaml')]
    valid_files = []
    yaml_length = 0
    valid_length = 0
    for yaml_file in yaml_files:
        yaml_file_path = os.path.join(files_folder, yaml_file)
        try:
            with open(yaml_file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                yaml_length += len(content)
                data = list(yaml.safe_load_all(content))
            # 如果能成功解析，则将文件复制到备份文件夹
            valid_length += len(content)
            shutil.copy(yaml_file_path, os.path.join(backup_folder, yaml_file))
            valid_files.append(yaml_file)
            
        except yaml.YAMLError as e:
            print(f"文件 {yaml_file} 解析错误: {str(e)}")
            pass
        except UnicodeDecodeError as e:
            # print(f"文件 {yaml_file} 编码错误: {str(e)}")
            pass
    print(f"在 {files_folder} 中")
    print(f"共 {len(yaml_files)} 个 YAML 文件，其中 {len(valid_files)} 个文件有效，有效率 {len(valid_files) / len(yaml_files) * 100:.2f} %")
    print(f"有效长度 {valid_length} / {yaml_length} , 占比 {valid_length / yaml_length * 100:.2f} % ")
    print(f"已复制 {len(valid_files)} 个有效文件到: {backup_folder}")

def check_json(files_folder, backup_folder):
    # 获取所有 json 文件
    json_files = [f for f in os.listdir(files_folder) if f.endswith('.json')]
    valid_files = []
    json_length = 0
    valid_length = 0
    for json_file in json_files:
        json_file_path = os.path.join(files_folder, json_file)
        try: 
            with open(json_file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                json_length += len(content)
                data = json.loads(content)
            valid_length += len(content)
            shutil.copy(json_file_path, os.path.join(backup_folder, json_file))
            valid_files.append(json_file)

        except json.JSONDecodeError as e:
            print(f"文件 {json_file} 解析错误: {str(e)}")
            pass
        except UnicodeDecodeError as e:
            # print(f"文件 {json_file} 编码错误: {str(e)}")
            pass
    print(f"在 {files_folder} 中")
    print(f"共 {len(json_files)} 个 JSON 文件，其中 {len(valid_files)} 个文件有效，有效率 {len(valid_files) / len(json_files) * 100:.2f} %")
    print(f"有效长度 {valid_length} / {json_length} , 占比 {valid_length / json_length * 100:.2f} % ")
    print(f"已复制 {len(valid_files)} 个有效文件到: {backup_folder}")

def check_stml(files_folder, backup_folder):
    # 获取所有 stml 文件
    stml_files = [f for f in os.listdir(files_folder) if f.endswith('.stml')]
    valid_files = []
    stml_length = 0
    valid_length = 0
    for stml_file in stml_files:
        stml_file_path = os.path.join(files_folder, stml_file)
        try: 
            with open(stml_file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                stml_length += len(content)
                data, warnings = stml.loads(content)
            valid_length += len(content)
            valid_files.append(stml_file)
            shutil.copy(stml_file_path, os.path.join(backup_folder, stml_file))
        except Exception as e:
            print(f"文件 {stml_file} 解析错误: {str(e)}")
            pass
        except UnicodeDecodeError as e:
            # print(f"文件 {stml_file} 解码错误: {str(e)}")
            pass
    print(f"在 {files_folder} 中")
    print(f"共 {len(stml_files)} 个 STML 文件，其中 {len(valid_files)} 个文件有效，有效率 {len(valid_files) / len(stml_files) * 100:.2f} %")
    print(f"有效长度 {valid_length} / {stml_length} , 占比 {valid_length / stml_length * 100:.2f} % ")
    print(f"已复制 {len(valid_files)} 个有效文件到: {backup_folder}")


CHECK_FOLDER_1 = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2\test_data1"
OUTPUT_FOLDER_1 = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2\test_data2"
CHECK_FOLDER_2 = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2\test_data3"
OUTPUT_FOLDER_2 = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2\test_data4"


if __name__ == "__main__":
    print("=" * 60)
    print("Phase 1: test_data1 -> test_data2")
    print("=" * 60)
    check_json(CHECK_FOLDER_1, OUTPUT_FOLDER_1)
    check_stml(CHECK_FOLDER_1, OUTPUT_FOLDER_1)
    check_yaml(CHECK_FOLDER_1, OUTPUT_FOLDER_1)
    print()
    print("=" * 60)
    print("Phase 2: test_data3 -> test_data4")
    print("=" * 60)
    check_json(CHECK_FOLDER_2, OUTPUT_FOLDER_2)
    check_stml(CHECK_FOLDER_2, OUTPUT_FOLDER_2)
    check_yaml(CHECK_FOLDER_2, OUTPUT_FOLDER_2)

