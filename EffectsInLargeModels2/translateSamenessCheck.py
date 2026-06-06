"""
第二类对比检测脚本：
对比 test_data4 中 LLM 转换后的成对文件（源文件 vs LLM生成的目标文件）

对比方向：
  stml_json:  源STML → LLM生成的JSON
  json_stml:  源JSON → LLM生成的STML
  yaml_json:  源YAML → LLM生成的JSON
  json_yaml:  源JSON → LLM生成的YAML
  stml_yaml:  源STML → LLM生成的YAML
  yaml_stml:  源YAML → LLM生成的STML
"""

PROJECT_ROOT = r"F:\Studio\Project\my_graduation_project2\Language"
import sys
import os
sys.path.insert(0, os.path.join(PROJECT_ROOT, "stml_py", "src"))

import yaml
import json
import stml
from collections import defaultdict


def merge_yaml_documents(yaml_docs):
    """将多文档 YAML 字符串合并为一个字典"""
    merged_dict = {}
    for doc in yaml_docs:
        if isinstance(doc, dict):
            def deep_merge(base, update):
                for key, value in update.items():
                    if key in base and isinstance(base[key], dict) and isinstance(value, dict):
                        deep_merge(base[key], value)
                    else:
                        base[key] = value
            deep_merge(merged_dict, doc)
    return merged_dict


def convert_to_strings(obj):
    """递归地将所有值转换为字符串（用于宽松比较）"""
    if isinstance(obj, dict):
        return {key: convert_to_strings(value) for key, value in obj.items()}
    elif isinstance(obj, list):
        return [convert_to_strings(item) for item in obj]
    elif obj is None:
        return "null"
    elif isinstance(obj, bool):
        return str(obj).lower()
    else:
        return str(obj)


def load_json(file_path):
    """加载JSON，如果是多文档列表则合并为单一字典"""
    with open(file_path, 'r', encoding='utf-8') as f:
        raw = json.load(f)
        return merge_yaml_documents(raw) if isinstance(raw, list) else raw


def load_yaml(file_path):
    """加载YAML，多文档自动合并为单一字典"""
    with open(file_path, 'r', encoding='utf-8') as f:
        return merge_yaml_documents(list(yaml.safe_load_all(f)))


def load_stml(file_path):
    """加载STML，多文档自动合并为单一字典"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content, _warn = stml.loads(f.read())
        if content:
            return merge_yaml_documents(content["docs"])
        return None


LOADERS = {
    'json': load_json,
    'yaml': load_yaml,
    'stml': load_stml,
}


def compare_pair(file_a, file_b, loader_a, loader_b):
    """
    比较两个文件的内容，返回 (严格匹配, 宽松匹配)
    双方均通过 loader 合并为单一字典后比较
    文件不存在或解析失败返回 None
    """
    if not os.path.exists(file_a) or not os.path.exists(file_b):
        return None
    try:
        content_a = loader_a(file_a)
        content_b = loader_b(file_b)
    except Exception:
        return None

    strict_match = (content_a == content_b)
    loose_match = (convert_to_strings(content_a) == convert_to_strings(content_b))
    return strict_match, loose_match


# 六种转换方向：(前缀, 源格式, 目标格式)
CONVERSION_DIRECTIONS = [
    ('stml_json',  'stml', 'json'),
    ('json_stml',  'json', 'stml'),
    ('yaml_json',  'yaml', 'json'),
    ('json_yaml',  'json', 'yaml'),
    ('stml_yaml',  'stml', 'yaml'),
    ('yaml_stml',  'yaml', 'stml'),
]


def check_phase2(folder_path):
    """
    扫描文件夹，按前缀分组、配对文件，进行一致性对比
    """
    # 获取文件夹中所有文件，按前缀分组
    prefix_files = defaultdict(lambda: defaultdict(list))
    for file_name in os.listdir(folder_path):
        for prefix, src_fmt, tgt_fmt in CONVERSION_DIRECTIONS:
            if file_name.startswith(prefix + '_'):
                base = file_name[len(prefix)+1:]  # e.g. "1.stml" or "1.json"
                idx_str, ext = os.path.splitext(base)
                try:
                    idx = int(idx_str)
                except ValueError:
                    continue
                prefix_files[prefix][idx].append((ext.lstrip('.'), file_name))
                break

    results = {}

    for prefix, src_fmt, tgt_fmt in CONVERSION_DIRECTIONS:
        groups = prefix_files.get(prefix, {})
        if not groups:
            print(f"[{prefix}] 未找到匹配文件")
            continue

        valid_count = 0
        strict_count = 0
        loose_count = 0

        for idx in sorted(groups.keys()):
            files = groups[idx]
            ext_map = {ext: name for ext, name in files}

            src_ext = src_fmt  # 'json', 'yaml', 'stml'
            tgt_ext = tgt_fmt

            src_file = ext_map.get(src_ext)
            tgt_file = ext_map.get(tgt_ext)

            if not src_file or not tgt_file:
                continue

            src_path = os.path.join(folder_path, src_file)
            tgt_path = os.path.join(folder_path, tgt_file)

            result = compare_pair(
                src_path, tgt_path,
                LOADERS[src_fmt], LOADERS[tgt_fmt]
            )

            if result is None:
                continue

            valid_count += 1
            if result[0]:
                strict_count += 1
            if result[1]:
                loose_count += 1

        results[prefix] = {
            'valid_count': valid_count,
            'strict_count': strict_count,
            'loose_count': loose_count,
        }

        strict_rate = (strict_count / valid_count * 100) if valid_count > 0 else 0
        loose_rate = (loose_count / valid_count * 100) if valid_count > 0 else 0
        print(f"[{prefix}] 有效对数: {valid_count}, "
              f"严格匹配: {strict_count} ({strict_rate:.1f}%), "
              f"宽松匹配: {loose_count} ({loose_rate:.1f}%)")

    return results


def print_report(results):
    """按报告格式输出两个表格"""
    # 表1: 严格匹配
    print("\n" + "=" * 70)
    print("第二类测试对比结果（严格匹配）")
    print("=" * 70)
    print(f"{'方向':<16} {'有效对数':<10} {'匹配对数':<10} {'匹配比例':<10}")
    print("-" * 46)
    for prefix, _src, _tgt in CONVERSION_DIRECTIONS:
        r = results.get(prefix, {})
        v = r.get('valid_count', 0)
        s = r.get('strict_count', 0)
        rate = f"{s/v*100:.1f}%" if v > 0 else "N/A"
        print(f"{prefix:<16} {v:<10} {s:<10} {rate:<10}")

    # 表2: 宽松匹配（忽略类型差异）
    print("\n" + "=" * 70)
    print("第二类测试对比结果（宽松匹配，忽略类型差异）")
    print("=" * 70)
    print(f"{'方向':<16} {'有效对数':<10} {'匹配对数':<10} {'匹配比例':<10}")
    print("-" * 46)
    for prefix, _src, _tgt in CONVERSION_DIRECTIONS:
        r = results.get(prefix, {})
        v = r.get('valid_count', 0)
        l = r.get('loose_count', 0)
        rate = f"{l/v*100:.1f}%" if v > 0 else "N/A"
        print(f"{prefix:<16} {v:<10} {l:<10} {rate:<10}")


if __name__ == "__main__":
    test_data4 = os.path.join(PROJECT_ROOT, "EffectsInLargeModels2", "test_data4")
    if not os.path.isdir(test_data4):
        print(f"文件夹不存在: {test_data4}")
        print("请先运行 validityCheck.py 将 test_data3 的有效文件复制到 test_data4")
    else:
        results = check_phase2(test_data4)
        print_report(results)
