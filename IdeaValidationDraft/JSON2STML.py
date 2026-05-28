import json
from typing import Any, Dict

def escape_string(s: str) -> str:
    """转义双引号、反斜杠、换行、制表符"""
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    s = s.replace('\n', '\\n')
    s = s.replace('\t', '\\t')
    return s

def quote_key(key: str) -> str:
    """键永远用双引号包裹"""
    return '"' + escape_string(key) + '"'

def format_scalar(value: Any) -> str:
    """标量值：null 输出 'null'，其他输出带双引号的字符串"""
    if value is None:
        return 'null'
    if isinstance(value, bool):
        return '"' + ('true' if value else 'false') + '"'
    if isinstance(value, (int, float)):
        return '"' + str(value) + '"'
    if isinstance(value, str):
        return '"' + escape_string(value) + '"'
    raise TypeError(f"不支持的类型: {type(value)}")

def serialize_node(node: Any, indent_level: int = 0) -> str:
    """
    将 AST 节点转换为 SYML 字符串（可多行，已含缩进）。
    缩进：每级 2 空格。
    """
    indent = '  ' * indent_level
    next_indent = '  ' * (indent_level + 1)

    # 1. 空值 / 标量
    if node is None or isinstance(node, (bool, int, float, str)):
        return indent + format_scalar(node)

    # 2. 列表（序列）
    if isinstance(node, list):
        if not node:                 # 空列表 → null
            return indent + 'null'
        lines = []
        for item in node:
            if isinstance(item, (type(None), bool, int, float, str)):
                # 标量条目：- "值"
                lines.append(indent + '- ' + format_scalar(item))
            elif isinstance(item, dict):
                if not item:         # 空字典条目 → null
                    lines.append(indent + '- null')
                    continue
                if len(item) > 1:
                    raise ValueError(
                        "SYML 列表条目不支持多键字典，请使用单键映射包装: "
                        + str(item)
                    )
                key, val = next(iter(item.items()))
                if isinstance(val, (dict, list)):
                    # 值复合：- "键":
                    lines.append(indent + '- ' + quote_key(key) + ':')
                    sub = serialize_node(val, indent_level + 1)
                    lines.append(sub)
                else:
                    # 值标量：- "键": "值"
                    lines.append(indent + '- ' + quote_key(key) + ': ' + format_scalar(val))
            else:
                raise TypeError(f"列表包含不支持的类型: {type(item)}")
        return '\n'.join(lines)

    # 3. 字典（映射）
    if isinstance(node, dict):
        if not node:                 # 空字典 → null
            return indent + 'null'
        lines = []
        for key, val in node.items():
            k_str = quote_key(key)
            if isinstance(val, (dict, list)):
                # 复合值：换行缩进
                sub = serialize_node(val, indent_level + 1)
                # 如果子结果以换行开头，直接换行即可
                if sub.strip() == '':    # 空复合体（如空列表/空字典已转为 null）
                    lines.append(indent + k_str + ': null')
                else:
                    lines.append(indent + k_str + ':')
                    lines.append(sub)
            else:
                # 标量值：同行
                lines.append(indent + k_str + ': ' + format_scalar(val))
        return '\n'.join(lines)

    raise TypeError(f"不支持的节点类型: {type(node)}")

def dumps(obj: Dict[str, Any]) -> str:
    """
    将带有 docs 包装的 AST 转换为标准 SYML 文本。
    格式：{"docs": {"doc1": ..., "doc2": ...}}
    """
    if isinstance(obj, dict) and 'docs' in obj and isinstance(obj['docs'], dict):
        docs = obj['docs']
        # 按键名排序（doc1, doc2 ...）
        sorted_keys = sorted(docs.keys(), key=lambda x: int(x.replace('doc', '')))
        parts = []
        for k in sorted_keys:
            parts.append(serialize_node(docs[k], indent_level=0))
        return '\n---\n'.join(parts) + '\n'
    else:
        # 无包装的单文档
        return serialize_node(obj, indent_level=0) + '\n'

def json_to_syml(json_str: str) -> str:
    """JSON 字符串 -> SYML 文本"""
    obj = json.loads(json_str)
    return dumps(obj)


# ------------------------------------------------------------
# 使用示例
# ------------------------------------------------------------
if __name__ == '__main__':
    with open('input.json', 'r', encoding='utf-8') as f:
        json_str = f.read()
    syml_output = json_to_syml(json_str)
    with open('output.stml', 'w', encoding='utf-8') as f:
        f.write(syml_output)