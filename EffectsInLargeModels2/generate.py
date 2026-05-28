
import llmChat
import time
import re
import os
import pandas as pd
from typing import Optional, Tuple, Dict, Any
from datetime import datetime

# 路径配置
BASE_TEST_FOLDER = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2\test_data1"
# LOG_FILE_PATH = os.path.join(BASE_TEST_FOLDER, "generation_log.csv")

# 读取STML规范
stml_specification = """
stml是一种类似yaml，但不完全相同的消息格式，同样采用缩进与换行区分内容，支持的类型只有字符串、一维列表、字典、null。
1. 缩进必须为偶数个；
2. 键与值全部使用半角双引号包裹，如果键/值内有特殊字符，需要转义，比如：引号和换行；
3. 键与值之间使用半角冒号;
4. 多行的值使用 "键": {\n内容\n}\n包裹；
5. 支持行内列表，使用逗号分隔，值使用 ["内容1", "内容2", ...] 包裹
6. 支持使用 - 行创建列表；
7. 使用 `---` 分隔多文档；
8. 使用 `null` 表示 null 值；
9. 使用 `#` 开头表示注释，注释必须单独一行；
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
  "刚刚用注释了": "只要是 # 开头的，都会被当作注释"
  # 不能将键值和列表混用
---
"这就是第二个文档了": 
  "再看一下多行文本": {
文本都是定格开始的
  如果文本前面有空格也会被保留  
  这里面不管是什么都会被当作值
多行值得结束要求右花括号缩进小于等于对应键的缩进
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
以上是标准的STML格式说明
---
在STML中，禁止将列表和字典混合使用，以下是错误示例，不属于标准STML，请勿使用:
```stml
"这是不支持的":
  - 将列表
  "与字典混合使用": "这是不支持的"
"请不要使用这种格式":
  - "即便是缩进": "也是不可以的"
    "这是yaml支持的": "不是stml支持的"
  - "请不要使用列表加字典的错误形式"
    "支持嵌套，不支持并列": "否则是错误的"
```
---
"""



def extract_code_block(llm_response: str, language: str) -> Optional[str]:
    """
    从LLM响应中提取指定语言的代码块
    
    Args:
        llm_response: LLM的完整响应文本
        language: 代码块语言标识（如'stml', 'json', 'yaml'）
    
    Returns:
        提取到的代码内容，如果未找到则返回None
    """
    pattern = rf'```{language}\s*\n(.*?)\s*```'
    match = re.search(pattern, llm_response, re.DOTALL)
    return match.group(1) if match else None


def save_content_to_file(content: str, file_path: str):
    """将内容保存到文件"""
    with open(file_path, "w", encoding='utf-8') as output_file:
        output_file.write(content)


def generate_stml_and_json() -> Optional[Tuple[str, str]]:
    """
    生成STML和对应的JSON文件
    
    Args:
        group_index: 组内序号
    
    Returns:
        成功时返回(stml文件路径, json文件路径)元组，失败或文件已存在时返回None
    """
    generation_prompt = stml_specification + """
请阅读上方的STML规范，自行生成一个内容非常丰富、总字符很多的标准STML，并给出对应的JSON，分别使用
```stml
(stml内容)
```
和
```json
(json内容)
```
包裹

    """

    print(f"[STML&JSON] 开始生成")
    start_timestamp = time.time()

    try:
        # llm_responses = llmChat.chat_ccmc(generation_prompt, 30)
        llm_responses = []
        for group_index in range(1, 31):
            print(f"{group_index}/30")
            llm_response = llmChat.chat_dp(generation_prompt)
            llm_responses.append(llm_response)
        
        # for group_index, llm_response in enumerate(llm_responses):
            # 提取代码块
            stml_content = extract_code_block(llm_response, 'stml')
            json_content = extract_code_block(llm_response, 'json')

            # 构建文件路径
            stml_file_path = os.path.join(BASE_TEST_FOLDER, f"group_1_{group_index}.stml")
            json_file_path = os.path.join(BASE_TEST_FOLDER, f"group_1_{group_index}.json")

            # 保存提取的内容
            if stml_content and json_content:
                save_content_to_file(stml_content, stml_file_path)
                save_content_to_file(json_content, json_file_path)

            else:
                print(f"[STML&JSON] 未能提取完整内容")
                continue
        elapsed_time = round(time.time() - start_timestamp, 2)
        print(f"[STML&JSON] 生成完成，耗时: {elapsed_time}秒")
        print(f"[STML&JSON] 输入长度: {len(generation_prompt)}, 输出长度: {len(llm_response)}")

    except Exception as e:
        error_message = f"{type(e).__name__}: {str(e)}"
        print(f"[STML&JSON] 生成过程出错: {error_message}")
        return None


def generate_yaml_and_json() -> Optional[Tuple[str, str]]:
    """
    生成YAML和对应的JSON文件
    
    Args:
        group_index: 组内序号
    
    Returns:
        成功时返回(yaml文件路径, json文件路径)元组，失败或文件已存在时返回None
    """
    generation_prompt = """

自行生成一个内容非常丰富、总字符很多的标准YAML，并给出对应的JSON，分别使用
```yaml
(yaml内容)
```
和
```json
(json内容)
```
包裹

    """

    print(f"[YAML&JSON] 开始生成")
    start_timestamp = time.time()

    try:
        # llm_responses = llmChat.chat_ccmc(generation_prompt, 30)
        llm_responses = []
        for group_index in range(1, 31):
            print(f"{group_index}/30")
            llm_response = llmChat.chat_dp(generation_prompt)
            llm_responses.append(llm_response)

        # for group_index, llm_response in enumerate(llm_responses):
            # 提取代码块
            yaml_content = extract_code_block(llm_response, 'yaml')
            json_content = extract_code_block(llm_response, 'json')

            # 构建文件路径
            yaml_file_path = os.path.join(BASE_TEST_FOLDER, f"group_2_{group_index}.yaml")
            json_file_path = os.path.join(BASE_TEST_FOLDER, f"group_2_{group_index}.json")

            # 保存提取的内容
            if yaml_content and json_content:
                save_content_to_file(yaml_content, yaml_file_path)
                save_content_to_file(json_content, json_file_path)
            else:
                print(f"[YAML&JSON] 未能提取完整内容")
                continue
        elapsed_time = round(time.time() - start_timestamp, 2)
        print(f"[YAML&JSON] 生成完成，耗时: {elapsed_time}秒")
        print(f"[YAML&JSON] 输入长度: {len(generation_prompt)}, 输出长度: {len(llm_response)}")

    except Exception as e:
        error_message = f"{type(e).__name__}: {str(e)}"
        print(f"[YAML&JSON] 生成过程出错: {error_message}")
        return None


def generate_stml_and_yaml() -> Optional[Tuple[str, str]]:
    """
    生成STML和对应的YAML文件
    
    Args:
        group_index: 组内序号
    
    Returns:
        成功时返回(stml文件路径, yaml文件路径)元组，失败或文件已存在时返回None
    """
    generation_prompt =  stml_specification + """
请阅读上方的STML规范，自行生成一个内容非常丰富、总字符很多的标准STML，并给出对应的YAML，分别使用
```stml
(stml内容)
```
和
```yaml
(yaml内容)
```
包裹

    """

    print(f"[STML&YAML] 开始生成")
    start_timestamp = time.time()

    try:
        # llm_responses = llmChat.chat_ccmc(generation_prompt, 30)
        llm_responses = []
        for group_index in range(1, 31):
            print(f"{group_index}/30")
            llm_response = llmChat.chat_dp(generation_prompt)
            llm_responses.append(llm_response)
        
        # for group_index, llm_response in enumerate(llm_responses):
            # 提取代码块
            stml_content = extract_code_block(llm_response, 'stml')
            yaml_content = extract_code_block(llm_response, 'yaml')

            # 构建文件路径
            stml_file_path = os.path.join(BASE_TEST_FOLDER, f"group_3_{group_index}.stml")
            yaml_file_path = os.path.join(BASE_TEST_FOLDER, f"group_3_{group_index}.yaml")
            # 保存提取的内容
            if stml_content and yaml_content:
                save_content_to_file(stml_content, stml_file_path)
                save_content_to_file(yaml_content, yaml_file_path)
            else:
                print(f"[STML&YAML] 未能提取完整内容")
                continue
        elapsed_time = round(time.time() - start_timestamp, 2)
        print(f"[STML&YAML] 生成完成，耗时: {elapsed_time}秒")
        print(f"[STML&YAML] 输入长度: {len(generation_prompt)}, 输出长度: {len(llm_response)}")

    except Exception as e:
        error_message = f"{type(e).__name__}: {str(e)}"
        print(f"[STML&YAML] 生成过程出错: {error_message}")
        return None

def main():
    """主函数：批量生成测试数据"""
    
    print("=" * 50)
    print("开始批量生成测试数据")
    print("=" * 50)
    
    # 生成STML和YAML配对
    print("\n【阶段1】生成STML和YAML配对")
    generate_stml_and_yaml()
    
    # 生成STML和JSON配对
    print("\n【阶段2】生成STML和JSON配对")
    generate_stml_and_json()
    
    # 生成YAML和JSON配对
    print("\n【阶段3】生成YAML和JSON配对")
    generate_yaml_and_json()
    
    print("\n" + "=" * 50)
    print("所有生成任务完成")
    print("=" * 50)


if __name__ == '__main__':
    main()
