import os
import llmChat
import re
import random
import shutil


input_folder = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2\test_data2"
output_folder = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2\test_data3"

stml_rule = """
stml是一种类似yaml，但不完全相同的消息格式，同样采用缩进与换行区分内容，支持的类型只有字符串、一维列表、字典、null。
1. 缩进必须为偶数个；
2. 键与值全部使用半角双引号包裹，如果键/值内有特殊字符，需要转义，比如：引号和换行；
3. 键与值之间使用半角冒号;
4. 多行的值使用 "键": {\n内容\n}\n包裹；
5. 支持行内列表，使用逗号分隔，值使用 ["内容1", "内容2", ...] 包裹
6. 支持使用 - 行创建列表；
7. 使用 `---` 分隔多文档；
8. 使用 `null` 表示 null 值；
9. 使用 `#` 开头表示注释；
10. 支持字典嵌套；
11. 不支持类型混用
这是一个标准的STML文档示例:
```示例stml
"这是一个标准的": "stml文档"
"多行文本一定要注意": {
左花括号跟着换行
右花括号前面是换行，右边也是换行
比如这一行的}就不会被识别为多行值的结束
因为}的前后有其他内容，
只要被包裹，里面的内容都会是多行文本值
  }
但是上一行的}也不会被识别为多行值的结束，因为}前面有2个空格，大于键“多行文本一定要注意”的缩进
}
"相当于": "左花括号跟着换行\n右花括号前面是换行，右边也是换行\n..."
"所以说": "多行值的右花括号要≤键的缩进"
"这是一个列表": ["这是行内列表", "使用,分割", "成对的引号内可以出现:、,或者是其他的\n都可以"]
"然后这是一个多行列表":
  - "如果不是键值对的形式"
  - "这些都会被当作值"
"就等于": ["如果不是键值对的形式", "这些都会被当作值"]
"列表内是可以使用字典的":
  - "这是一个字典": "字典的值"
  - "这是一个字典"
  - "有一个字典没有值": "它的值会被自动设置为null"
"然后就是可以嵌套字典":
  "缩进为2":
    "缩进为3": "可以吗"
"不支持类型混用":
  "比如这是一个子字典": "不能出现列表"
  # 不能将键值和列表混用
  "刚刚用注释了": "只要是 # 开头的，都会被当作注释"
---
"这就是第二个文档了": 
  "再看一下多行文本": {
文本都是定格开始的
  如果文本前面有空格也会被保留  
}
"等同于": "文本都是定格开始的\n  如果文本前面有空格也会被保留  "
"理论上支持列表嵌套":
  - ["像这样", "把列表当成值"]
  - "这样会被解析"
  - "但是实际上是不支持的"
"会被识别为":
  - "":
    - "像这样"
    - "把列表当成值"
  - "这样会被解析": null
  - "但是实际上是不支持的": null
"建议不要使用嵌套列表": ~
"出现了~": "这是为了兼容yaml"
"就介绍到这吧": "应该够了?"
```
对应的JSON是:
```对应JSON
[
  {
    "这是一个标准的": "stml文档",
    "多行文本一定要注意": "左花括号跟着换行\n右花括号前面是换行，右边也是换行\n比如这一行的}就不会被识别为多行值的结束\n因为}的前后有其他内容，\n只要被包裹，里面的内容都会是多行文本值\n  }\n但是上一行的}也不会被识别为多行值的结束，因为}前面有2个空格，大于键“多行文本一定要注意”的缩进,
    "相当于": "左花括号跟着换行\n右花括号前面是换行，右边也是换行\n...",
    "所以说": "多行值的右花括号要≤键的缩进",
    "这是一个列表": [
      "这是行内列表",
      "使用,分割",
      "成对的引号内可以出现:、,或者是其他的\n都可以"
    ],
    "然后这是一个多行列表": [
      "如果不是键值对的形式",
      "这些都会被当作值"
    ],
    "就等于": [
      "如果不是键值对的形式",
      "这些都会被当作值"
    ],
    "列表内是可以使用字典的": [
      {"这是一个字典": "字典的值"},
      {"这是一个字典": null},
      {"有一个字典没有值": "它的值会被自动设置为null"}
    ],
    "然后就是可以嵌套字典": {
      "缩进为2": {
        "缩进为3": "可以吗"
      }
    },
    "不支持类型混用": {
      "比如这是一个子字典": "不能出现列表",
      "刚刚用注释了": "只要是 # 开头的，都会被当作注释"
    }
  },
  {
    "这就是第二个文档了": {
    "再看一下多行文本": "文本都是定格开始的\n  如果文本前面有空格也会被保留  "
    },
    "等同于": "文本都是定格开始的\n  如果文本前面有空格也会被保留  ",
    "理论上支持列表嵌套": [
    {"": ["像这样", "把列表当成值"]},
    {"这样会被解析": null},
    {"但是实际上是不支持的": null}
    ]
    "会被识别为": [
    {"": ["像这样", "把列表当成值"]},
    {"这样会被解析": null},
    {"但是实际上是不支持的": null}
    ],
    "建议不要使用嵌套列表": null,
    "出现了~": "这是为了兼容yaml",
    "就介绍到这吧": "应该够了?"
  }
]
```
---
在STML中，禁止将列表和字典混合使用，以下是错误示例，不属于标准STML，请勿使用:
```stml
"这是不支持的":
  - 将列表
  "与字典混合使用": "这是不支持的"
"请不要使用这种格式":
  - "即便是缩进": "也是不可以的"
    "yaml支持": "但是stml不支持"
  - "请不要犯这种错误"
```
---
"""



# 第二类测试

## STML->JSON
### 加载所有STML文件
stml_file_list = []
for file_name in os.listdir(input_folder):
    if file_name.endswith(".stml"):
        stml_file_list.append(os.path.join(input_folder, file_name))

print(f"找到 {len(stml_file_list)} 个STML文件")

### 随机挑选20个
if len(stml_file_list) > 20:
    selected_files = random.sample(stml_file_list, 20)
else:
    selected_files = stml_file_list

print(f"随机挑选 {len(selected_files)} 个文件进行处理")

### 交给大模型
# 构造提示词模板（使用占位符标记）
prompt_template = stml_rule + """


请将下述STML文件转换成对应的JSON代码，使用
```json
(json内容)
```
包裹

输入的STML文件内容如下：
```stml
{{STML_CONTENT_PLACEHOLDER}}
```
"""

for idx, file_path in enumerate(selected_files, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        stml_content = f.read()
    
    # 构造完整的提示词（使用字符串替换避免花括号冲突）
    prompt_stml2json = prompt_template.replace("{{STML_CONTENT_PLACEHOLDER}}", stml_content)
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_ccmc(prompt_stml2json)
    
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
{{JSON_CONTENT_PLACEHOLDER}}
```
"""

for idx, file_path in enumerate(selected_json_files, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        json_content = f.read()
    
    # 构造完整的提示词（使用字符串替换避免花括号冲突）
    prompt_json2stml = prompt_json2stml_template.replace("{{JSON_CONTENT_PLACEHOLDER}}", json_content)
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_ccmc(prompt_json2stml)
    
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
{{YAML_CONTENT_PLACEHOLDER}}
```
"""

for idx, file_path in enumerate(selected_yaml_files, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        yaml_content = f.read()
    
    # 构造完整的提示词（使用字符串替换）
    prompt_yaml2json = prompt_yaml2json_template.replace("{{YAML_CONTENT_PLACEHOLDER}}", yaml_content)
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_ccmc(prompt_yaml2json)
    
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
{{JSON_CONTENT_PLACEHOLDER}}
```
"""

for idx, file_path in enumerate(selected_json_files_2, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        json_content = f.read()
    
    # 构造完整的提示词（使用字符串替换）
    prompt_json2yaml = prompt_json2yaml_template.replace("{{JSON_CONTENT_PLACEHOLDER}}", json_content)
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_ccmc(prompt_json2yaml)
    
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
{{STML_CONTENT_PLACEHOLDER}}
```
"""

for idx, file_path in enumerate(selected_stml_files_2, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        stml_content = f.read()
    
    # 构造完整的提示词（使用字符串替换避免花括号冲突）
    prompt_stml2yaml = prompt_stml2yaml_template.replace("{{STML_CONTENT_PLACEHOLDER}}", stml_content)
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_ccmc(prompt_stml2yaml)
    
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
{{YAML_CONTENT_PLACEHOLDER}}
```
"""

for idx, file_path in enumerate(selected_yaml_files_2, start=1):
    with open(file_path, "r", encoding="utf-8") as f:
        yaml_content = f.read()
    
    # 构造完整的提示词（使用字符串替换避免花括号冲突）
    prompt_yaml2stml = prompt_yaml2stml_template.replace("{{YAML_CONTENT_PLACEHOLDER}}", yaml_content)
    
    # 调用LLM
    print(f"正在处理: {os.path.basename(file_path)}")
    response_content = llmChat.chat_ccmc(prompt_yaml2stml)
    
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