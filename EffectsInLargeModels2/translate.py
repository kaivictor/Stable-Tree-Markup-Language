import os
import llmChat
import re
import random
import shutil


SUB_PROJECT_ROOT = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2"
input_folder = os.path.join(SUB_PROJECT_ROOT, "test_data2")
output_folder = os.path.join(SUB_PROJECT_ROOT, "test_data3")

# 读取STML规范（与generate.py共用同一份规范文件）
with open(os.path.join(SUB_PROJECT_ROOT, "stml_specification.md"), 'r', encoding='utf-8') as f:
    stml_rule = f.read() + "\n请学习以上STML规则，然后：\n"


# 加载所有JSON文件
stml_file_list = []
for file_name in os.listdir(input_folder):
    if file_name.endswith(".stml"):
        stml_file_list.append(os.path.join(input_folder, file_name))

print(f"找到 {len(stml_file_list)} 个STML文件")

# 随机挑选20个
if len(stml_file_list) > 20:
    selected_stml_files = random.sample(stml_file_list, 20)
else:
    selected_stml_files = stml_file_list

print(f"随机挑选 {len(selected_stml_files)} 个文件进行处理")

# 构造提示词模板（使用占位符标记）
prompt_template = stml_rule + """

请将下述STML文件转换成对应的JSON代码，使用
```json
(json内容)
```
包裹

输入的STML文件内容如下：
```stml

"""

for idx, file_path in enumerate(selected_stml_files, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        stml_content = f.read()
    
    # 构造完整的提示词（使用字符串替换避免花括号冲突）
    prompt_stml2json = prompt_template + stml_content + "\n```"
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_dp(prompt_stml2json)
    
    # 提取JSON内容
    pattern = rf'```json\s*\n(.*?)\s*```'
    json_content_match = re.search(pattern, response_content, re.DOTALL)
    
    if json_content_match:
        json_content = json_content_match.group(1)
        
        # 确保输出目录存在
        os.makedirs(output_folder, exist_ok=True)
        
        # 使用新的命名格式：stml_json_序号.json
        json_file_name = f"stml_json_{idx}.json"
        stml_file_name = f"stml_json_{idx}.stml"
        
        json_file_path = os.path.join(output_folder, json_file_name)
        stml_output_path = os.path.join(output_folder, stml_file_name)
        
        with open(json_file_path, "w", encoding="utf-8") as f:
            f.write(json_content)
        
        # 复制原来的stml文件到输出文件夹并重命名
        shutil.copy2(file_path, stml_output_path)
        
        print(f"已保存文件：{json_file_path} 和 {stml_output_path}")
    else:
        print(f"未找到JSON内容：{file_path}")
        print(f"LLM回复预览: {response_content[:200]}...")




## JSON->STML
print("\n" + "="*50)
print("开始 JSON->STML 转换")
print("="*50)

# 加载所有JSON文件
json_file_list = []
for file_name in os.listdir(input_folder):
    if file_name.endswith(".json"):
        json_file_list.append(os.path.join(input_folder, file_name))

print(f"找到 {len(json_file_list)} 个JSON文件")

# 随机挑选20个
if len(json_file_list) > 20:
    selected_json_files = random.sample(json_file_list, 20)
else:
    selected_json_files = json_file_list

print(f"随机挑选 {len(selected_json_files)} 个文件进行处理")

# 构造提示词模板（需要STML规范，使用占位符标记）
prompt_json2stml_template = stml_rule + """


请将下述JSON文件转换成对应的STML格式，使用
```stml
(stml内容)
```
包裹

输入的JSON文件内容如下：
```json

"""

for idx, file_path in enumerate(selected_json_files, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        json_content = f.read()
    
    # 构造完整的提示词
    prompt_json2stml = prompt_json2stml_template + json_content + "\n```"
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_dp(prompt_json2stml)
    
    # 提取STML内容
    pattern = rf'```stml\s*\n(.*?)\s*```'
    stml_content_match = re.search(pattern, response_content, re.DOTALL)
    
    if stml_content_match:
        stml_content = stml_content_match.group(1)
        
        # 确保输出目录存在
        os.makedirs(output_folder, exist_ok=True)
        
        # 使用命名格式：json_stml_序号
        stml_file_name = f"json_stml_{idx}.stml"
        json_file_name = f"json_stml_{idx}.json"
        
        stml_file_path = os.path.join(output_folder, stml_file_name)
        json_output_path = os.path.join(output_folder, json_file_name)
        
        with open(stml_file_path, "w", encoding="utf-8") as f:
            f.write(stml_content)
        
        # 复制原来的json文件到输出文件夹
        shutil.copy2(file_path, json_output_path)
        
        print(f"已保存文件：{stml_file_path} 和 {json_output_path}")
    else:
        print(f"未找到STML内容：{file_path}")
        print(f"LLM回复预览: {response_content[:200]}...")


## YAML->JSON
print("\n" + "="*50)
print("开始 YAML->JSON 转换")
print("="*50)

# 加载所有YAML文件
yaml_file_list = []
for file_name in os.listdir(input_folder):
    if file_name.endswith(".yaml") or file_name.endswith(".yml"):
        yaml_file_list.append(os.path.join(input_folder, file_name))

print(f"找到 {len(yaml_file_list)} 个YAML文件")

# 随机挑选20个
if len(yaml_file_list) > 20:
    selected_yaml_files = random.sample(yaml_file_list, 20)
else:
    selected_yaml_files = yaml_file_list

print(f"随机挑选 {len(selected_yaml_files)} 个文件进行处理")

# 构造提示词模板（不需要STML规范，使用占位符标记）
prompt_yaml2json_template = """
请将下述YAML文件转换成对应的JSON代码，使用
```json
(json内容)
```
包裹

输入的YAML文件内容如下：
```yaml

"""

for idx, file_path in enumerate(selected_yaml_files, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        yaml_content = f.read()
    
    # 构造完整的提示词（使用字符串替换）
    prompt_yaml2json = prompt_yaml2json_template + yaml_content + "\n```"
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_dp(prompt_yaml2json)
    
    # 提取JSON内容
    pattern = rf'```json\s*\n(.*?)\s*```'
    json_content_match = re.search(pattern, response_content, re.DOTALL)
    
    if json_content_match:
        json_content = json_content_match.group(1)
        
        # 确保输出目录存在
        os.makedirs(output_folder, exist_ok=True)
        
        # 使用命名格式：yaml_json_序号
        json_file_name = f"yaml_json_{idx}.json"
        yaml_file_name = f"yaml_json_{idx}.yaml"
        
        json_file_path = os.path.join(output_folder, json_file_name)
        yaml_output_path = os.path.join(output_folder, yaml_file_name)
        
        with open(json_file_path, "w", encoding="utf-8") as f:
            f.write(json_content)
        
        # 复制原来的yaml文件到输出文件夹
        shutil.copy2(file_path, yaml_output_path)
        
        print(f"已保存文件：{json_file_path} 和 {yaml_output_path}")
    else:
        print(f"未找到JSON内容：{file_path}")
        print(f"LLM回复预览: {response_content[:200]}...")


## JSON->YAML
print("\n" + "="*50)
print("开始 JSON->YAML 转换")
print("="*50)

# 重新加载JSON文件列表（如果之前没有加载）
if not json_file_list:
    for file_name in os.listdir(input_folder):
        if file_name.endswith(".json"):
            json_file_list.append(os.path.join(input_folder, file_name))

print(f"找到 {len(json_file_list)} 个JSON文件")

# 随机挑选20个
if len(json_file_list) > 20:
    selected_json_files_2 = random.sample(json_file_list, 20)
else:
    selected_json_files_2 = json_file_list

print(f"随机挑选 {len(selected_json_files_2)} 个文件进行处理")

# 构造提示词模板（不需要STML规范）
prompt_json2yaml_template = """
请将下述JSON文件转换成对应的YAML代码，使用
```yaml
(yaml内容)
```
包裹

输入的JSON文件内容如下：
```json

"""

for idx, file_path in enumerate(selected_json_files_2, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        json_content = f.read()
    
    # 构造完整的提示词（使用字符串替换）
    prompt_json2yaml = prompt_json2yaml_template + json_content + "\n```"
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_dp(prompt_json2yaml)
    
    # 提取YAML内容
    pattern = rf'```yaml\s*\n(.*?)\s*```'
    yaml_content_match = re.search(pattern, response_content, re.DOTALL)
    
    if yaml_content_match:
        yaml_content = yaml_content_match.group(1)
        
        # 确保输出目录存在
        os.makedirs(output_folder, exist_ok=True)
        
        # 使用命名格式：json_yaml_序号
        yaml_file_name = f"json_yaml_{idx}.yaml"
        json_file_name = f"json_yaml_{idx}.json"
        
        yaml_file_path = os.path.join(output_folder, yaml_file_name)
        json_output_path = os.path.join(output_folder, json_file_name)
        
        with open(yaml_file_path, "w", encoding="utf-8") as f:
            f.write(yaml_content)
        
        # 复制原来的json文件到输出文件夹
        shutil.copy2(file_path, json_output_path)
        
        print(f"已保存文件：{yaml_file_path} 和 {json_output_path}")
    else:
        print(f"未找到YAML内容：{file_path}")
        print(f"LLM回复预览: {response_content[:200]}...")


## STML->YAML
print("\n" + "="*50)
print("开始 STML->YAML 转换")
print("="*50)

# 重新加载STML文件列表（如果之前没有加载）
if not stml_file_list:
    for file_name in os.listdir(input_folder):
        if file_name.endswith(".stml"):
            stml_file_list.append(os.path.join(input_folder, file_name))

print(f"找到 {len(stml_file_list)} 个STML文件")

# 随机挑选20个
if len(stml_file_list) > 20:
    selected_stml_files_2 = random.sample(stml_file_list, 20)
else:
    selected_stml_files_2 = stml_file_list

print(f"随机挑选 {len(selected_stml_files_2)} 个文件进行处理")

# 构造提示词模板（需要STML规范）
prompt_stml2yaml_template = stml_rule + """


请将下述STML文件转换成对应的YAML代码，使用
```yaml
(yaml内容)
```
包裹

输入的STML文件内容如下：
```stml

"""

for idx, file_path in enumerate(selected_stml_files_2, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        stml_content = f.read()
    
    # 构造完整的提示词（使用字符串替换避免花括号冲突）
    prompt_stml2yaml = prompt_stml2yaml_template + stml_content + "\n```"
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_dp(prompt_stml2yaml)
    
    # 提取YAML内容
    pattern = rf'```yaml\s*\n(.*?)\s*```'
    yaml_content_match = re.search(pattern, response_content, re.DOTALL)
    
    if yaml_content_match:
        yaml_content = yaml_content_match.group(1)
        
        # 确保输出目录存在
        os.makedirs(output_folder, exist_ok=True)
        
        # 使用命名格式：stml_yaml_序号
        yaml_file_name = f"stml_yaml_{idx}.yaml"
        stml_file_name = f"stml_yaml_{idx}.stml"
        
        yaml_file_path = os.path.join(output_folder, yaml_file_name)
        stml_output_path = os.path.join(output_folder, stml_file_name)
        
        with open(yaml_file_path, "w", encoding="utf-8") as f:
            f.write(yaml_content)
        
        # 复制原来的stml文件到输出文件夹
        shutil.copy2(file_path, stml_output_path)
        
        print(f"已保存文件：{yaml_file_path} 和 {stml_output_path}")
    else:
        print(f"未找到YAML内容：{file_path}")
        print(f"LLM回复预览: {response_content[:200]}...")


## YAML->STML
print("\n" + "="*50)
print("开始 YAML->STML 转换")
print("="*50)

# 重新加载YAML文件列表（如果之前没有加载）
if not yaml_file_list:
    for file_name in os.listdir(input_folder):
        if file_name.endswith(".yaml") or file_name.endswith(".yml"):
            yaml_file_list.append(os.path.join(input_folder, file_name))

print(f"找到 {len(yaml_file_list)} 个YAML文件")

# 随机挑选20个
if len(yaml_file_list) > 20:
    selected_yaml_files_2 = random.sample(yaml_file_list, 20)
else:
    selected_yaml_files_2 = yaml_file_list

print(f"随机挑选 {len(selected_yaml_files_2)} 个文件进行处理")

# 构造提示词模板（需要STML规范）
prompt_yaml2stml_template = stml_rule + """


请将下述YAML文件转换成对应的STML格式，使用
```stml
(stml内容)
```
包裹

输入的YAML文件内容如下：
```yaml

"""

for idx, file_path in enumerate(selected_yaml_files_2, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        yaml_content = f.read()
    
    # 构造完整的提示词（使用字符串替换避免花括号冲突）
    prompt_yaml2stml = prompt_yaml2stml_template + yaml_content + "\n```"
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_dp(prompt_yaml2stml)
    
    # 提取STML内容
    pattern = rf'```stml\s*\n(.*?)\s*```'
    stml_content_match = re.search(pattern, response_content, re.DOTALL)
    
    if stml_content_match:
        stml_content = stml_content_match.group(1)
        
        # 确保输出目录存在
        os.makedirs(output_folder, exist_ok=True)
        
        # 使用命名格式：yaml_stml_序号
        stml_file_name = f"yaml_stml_{idx}.stml"
        yaml_file_name = f"yaml_stml_{idx}.yaml"
        
        stml_file_path = os.path.join(output_folder, stml_file_name)
        yaml_output_path = os.path.join(output_folder, yaml_file_name)
        
        with open(stml_file_path, "w", encoding="utf-8") as f:
            f.write(stml_content)
        
        # 复制原来的yaml文件到输出文件夹
        shutil.copy2(file_path, yaml_output_path)
        
        print(f"已保存文件：{stml_file_path} 和 {yaml_output_path}")
    else:
        print(f"未找到STML内容：{file_path}")
        print(f"LLM回复预览: {response_content[:200]}...")

print("\n" + "="*50)
print("所有转换任务完成！")
print("="*50)