
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

SUB_PROJECT_ROOT = r"F:\Studio\Project\my_graduation_project2\Language\EffectsInLargeModels2"
# 读取STML规范
with open(os.path.join(SUB_PROJECT_ROOT, "stml_specification.md"), 'r', encoding='utf-8') as f:
    stml_specification = f.read()




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
            llm_response = llmChat.chat_ccmc(generation_prompt)
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
            llm_response = llmChat.chat_ccmc(generation_prompt)
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
            llm_response = llmChat.chat_ccmc(generation_prompt)
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
