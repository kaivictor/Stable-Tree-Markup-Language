import sys

sys.path.insert(0, 'F:/Studio/Project/my_graduation_project2/语法设计')

import stml_py_
import os.path
import json

dataFolder = 'F:/Studio/Project/my_graduation_project2/语法设计/TestData'

# ast, warnings = stml.load("test1.stml")


with open(os.path.join(dataFolder, "test4.stml"), 'r', encoding='utf-8') as f:
    text = f.read()

ast, warnings = stml_py_.loads(text)

# AST 结构总览
for doc_name, doc_value in ast['docs'].items():
    if isinstance(doc_value, dict):
        print(f'{doc_name}: 映射，{len(doc_value)} 个键')
    elif isinstance(doc_value, list):
        print(f'{doc_name}: 序列，{len(doc_value)} 项')
    else:
        print(f'{doc_name}: {doc_value}')   # None 或其他

# 检查警告（行列号 + 中文描述）
for w in warnings:
    print(f'警告 L{w.line}:{w.column} — {w.message}')

# AST → JSON 字符串
json_str = json.dumps(ast, ensure_ascii=False, indent=2)

  # 或写入文件
with open(os.path.join(dataFolder, 'temp_output.json'), 'w', encoding='utf-8') as f:
    json.dump(ast, f, ensure_ascii=False, indent=2)