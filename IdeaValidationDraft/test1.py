import json
from typing import List, Dict, Any, Tuple

# ------------------------------------------------------------
def tokenize(text: str) -> List[Dict[str, Any]]:
    lines = []
    for raw in text.splitlines():
        indent = len(raw) - len(raw.lstrip(' '))
        content = raw[indent:]
        lines.append({'indent': indent, 'content': content, 'raw': raw})
    return lines

def split_docs(lines: List[Dict]) -> List[List[Dict]]:
    docs, current = [], []
    for line in lines:
        if line['content'].strip() == '---':
            if current:
                docs.append(current)
                current = []
        else:
            current.append(line)
    if current or not docs:
        docs.append(current)
    return docs

class SimpleYAMLParser:
    def __init__(self, lines: List[Dict[str, Any]]):
        self.lines = lines
        self.n = len(lines)

    def parse_document(self):
        if self.n == 0:
            return None
        return self._parse_node(0, parent_indent=-1)[0]

    def _parse_node(self, i, parent_indent):
        if i >= self.n:
            return None, i
        line = self.lines[i]
        if line['indent'] <= parent_indent:
            return None, i
        if line['content'].startswith('-'):
            # 根序列：block_indent 为 None，用原始规则
            return self._parse_sequence(i, parent_indent, block_indent=None)
        else:
            return self._parse_mapping(i, parent_indent)

    # ----- 映射 -----
    def _parse_mapping(self, i, parent_indent):
        mapping = {}
        while i < self.n:
            line = self.lines[i]
            if line['indent'] <= parent_indent:
                break
            if line['content'].startswith('-'):
                break
            key, value, next_i = self._parse_mapping_entry(i, parent_indent)
            mapping[key] = value
            i = next_i
        return mapping, i

    def _parse_mapping_entry(self, i, parent_indent):
        line = self.lines[i]
        content = line['content']
        colon = content.find(':')
        if colon == -1:
            return content.rstrip(), None, i + 1
        key = content[:colon].rstrip()
        rest = content[colon + 1:]
        if rest.startswith(' '):
            rest = rest[1:]
        # 多行字符串
        if rest.strip() == '{' and i + 1 < self.n:
            ml, ni = self._parse_multiline(i, line['indent'])
            return key, ml, ni
        if rest.strip() == '':
            if i + 1 < self.n:
                val, ni = self._parse_block_value(i + 1, line['indent'], is_nested=False)
                return key, val, ni
            return key, None, i + 1
        val = self._parse_inline_value(rest)
        return key, val, i + 1

    def _parse_block_value(self, i, key_indent, is_nested):
        """解析缩进块。is_nested 指示是否从序列条目子块进入"""
        if i >= self.n:
            return None, i
        line = self.lines[i]
        if line['content'].startswith('-'):
            # 如果是从序列条目子块调用，需要传递 block_indent 以正确结束
            return self._parse_sequence(i, key_indent,
                                        block_indent=key_indent if is_nested else None)
        if line['indent'] <= key_indent:
            return None, i
        return self._parse_mapping(i, key_indent)

    # ----- 序列（核心） -----
    def _is_complex_sequence(self, i, parent_indent, block_indent):
        cur = i
        while cur < self.n:
            line = self.lines[cur]
            # 遇到缩进过小的行，结束扫描
            if block_indent is not None and line['indent'] <= block_indent:
                break
            if line['indent'] < parent_indent:
                break
            if not line['content'].startswith('-'):
                break
            rest = line['content'][1:]
            if rest.startswith(' '):
                rest = rest[1:]
            if ':' in rest:
                return True
            if rest.strip() == '':
                if cur + 1 < self.n and self.lines[cur + 1]['indent'] > line['indent']:
                    return True
            cur += 1
        return False

    def _parse_sequence(self, i, parent_indent, block_indent):
        seq = []
        complex_mode = self._is_complex_sequence(i, parent_indent, block_indent)
        while i < self.n:
            line = self.lines[i]
            # 终止条件 1：子块边界（只对嵌套序列有效）
            if block_indent is not None and line['indent'] <= block_indent:
                break
            # 终止条件 2：缩进小于父缩进
            if line['indent'] < parent_indent:
                break
            # 终止条件 3：不是 '-' 开头（且上面条件未触发）
            if not line['content'].startswith('-'):
                break
            value, next_i = self._parse_sequence_item(i, parent_indent,
                                                      simple_as_scalar=not complex_mode,
                                                      block_indent=block_indent)
            seq.append(value)
            i = next_i
        return seq, i

    def _parse_sequence_item(self, i, parent_indent, simple_as_scalar, block_indent):
        line = self.lines[i]
        content = line['content']
        assert content.startswith('-')
        rest = content[1:]
        if rest.startswith(' '):
            rest = rest[1:]
        item_indent = line['indent']

        # 情况1：'- ' 或 '-' 无后续文本
        if rest.strip() == '':
            if i + 1 < self.n and self.lines[i + 1]['indent'] > item_indent:
                # 有子块，is_nested=True
                val, ni = self._parse_block_value(i + 1, item_indent, is_nested=True)
                return val, ni
            return None, i + 1

        has_colon = ':' in rest
        # 子块检查：仅当冒号后为空时才有子块
        need_block = False
        if has_colon:
            colon_pos = rest.find(':')
            after_colon = rest[colon_pos+1:]
            if after_colon.startswith(' '):
                after_colon = after_colon[1:]
            if after_colon.strip() == '':
                need_block = True
        has_block = need_block and (i + 1 < self.n and self.lines[i + 1]['indent'] > item_indent)

        if not has_colon and not has_block:
            if simple_as_scalar:
                return self._parse_scalar(rest.strip()), i + 1
            else:
                return {rest.strip(): None}, i + 1

        if not has_colon:
            # 安全回退
            return self._parse_scalar(rest.strip()), i + 1

        # 内联映射
        colon = rest.find(':')
        inline_key = rest[:colon].rstrip()
        inline_rest = rest[colon + 1:]
        if inline_rest.startswith(' '):
            inline_rest = inline_rest[1:]

        # 多行字符串
        if inline_rest.strip() == '{' and i + 1 < self.n:
            ml, ni = self._parse_multiline(i, item_indent)
            return {inline_key: ml}, ni

        # 值在子块
        if inline_rest.strip() == '':
            if has_block:
                sub_val, ni = self._parse_block_value(i + 1, item_indent, is_nested=True)
                return {inline_key: sub_val}, ni
            return {inline_key: None}, i + 1

        # 行内值
        sub_val = self._parse_inline_value(inline_rest)
        return {inline_key: sub_val}, i + 1

    # ----- 标量 & 内联值 -----
    def _parse_scalar(self, s):
        s = s.strip()
        if s == '~':
            return None
        return s

    def _parse_inline_value(self, s):
        s = s.strip()
        if s.startswith('[') and s.endswith(']'):
            return self._parse_inline_list(s)
        return self._parse_scalar(s)

    def _parse_inline_list(self, s):
        inner = s[1:-1]
        elems = []
        i = 0
        n = len(inner)
        while i < n:
            while i < n and inner[i] == ' ':
                i += 1
            if i >= n:
                break
            if inner[i] == '"':
                i += 1
                start = i
                while i < n and inner[i] != '"':
                    i += 1
                elems.append(inner[start:i])
                if i < n:
                    i += 1
            else:
                start = i
                while i < n and inner[i] != ',':
                    i += 1
                elems.append(self._parse_scalar(inner[start:i].strip()))
            while i < n and inner[i] in (',', ' '):
                i += 1
        return elems

    # ----- 多行字符串 -----
    def _parse_multiline(self, i, key_indent):
        parts = []
        i += 1
        while i < self.n:
            line = self.lines[i]
            if line['content'].strip() == '}' and line['indent'] <= key_indent:
                i += 1
                break
            parts.append(line['raw'])
            i += 1
        return '\n'.join(parts), i

# ------------------------------------------------------------
def loads(text):
    lines = tokenize(text)
    doc_blocks = split_docs(lines)
    docs = {}
    for idx, block in enumerate(doc_blocks, start=1):
        parser = SimpleYAMLParser(block)
        docs[f'doc{idx}'] = parser.parse_document()
    return {'docs': docs}

def load(filename):
    with open(filename, 'r', encoding='utf-8') as f:
        return loads(f.read())

if __name__ == '__main__':
    input_file = './测试1.txt'
    output_file = './测试1输出.json'
    with open(input_file, encoding='utf-8') as f:
        text = f.read()
    result = loads(text)
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=4)
    print(f"解析完成，结果已写入 {output_file}")