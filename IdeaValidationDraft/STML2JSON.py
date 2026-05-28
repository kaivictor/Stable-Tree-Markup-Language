import json
from typing import List, Dict, Any, Tuple

# ------------------------------------------------------------
# 预处理
# ------------------------------------------------------------
def tokenize(text: str) -> List[Dict[str, Any]]:
    lines = []
    for raw in text.splitlines():
        indent = len(raw) - len(raw.lstrip(' '))
        content = raw[indent:]
        lines.append({'indent': indent, 'content': content, 'raw': raw})
    return lines

def split_docs(lines: List[Dict]) -> List[List[Dict]]:
    """
    按文档分隔符切割，但在多行字符串内部不识别分隔符。
    状态：None 表示不在多行字符串中，否则为进入多行字符串的 key_indent。
    """
    docs, current = [], []
    multiline_key_indent = None  # 非 None 表示正在多行字符串内
    multiline_parts = []          # 收集多行字符串的行（包括起始行？不，起始行已处理）
    # 实际上我们需要从外部知道多行字符串的起始行号，但这里简化：遍历行，当遇到 "键: {" 时进入多行状态，
    # 直到遇到独立 '}' 且缩进 <= key_indent 时退出。
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        # 如果在多行字符串内，检查是否为结束行
        if multiline_key_indent is not None:
            # 结束条件：内容去除缩进和尾空白后为 '}'，且缩进 <= key_indent
            if line['content'].strip() == '}' and line['indent'] <= multiline_key_indent:
                # 结束多行字符串，将当前行也加入 current（作为多行字符串的一部分？不对，结束行不属于内容，但为了文档结构，我们应保持 current 包含完整的映射条目）
                # current 已经包含了 '键: {' 这一行，后续我们收集内容直到结束行，将内容行加入 current 并标记。
                # 实际上在 split_docs 阶段我们无法简单地将多行字符串的行保留在 current 中，因为 split_docs 只是粗粒度的文档切割，
                # 多行字符串的内容行也是文档的一部分，不应该被过滤掉。所以更简单的方法：在遇到 "键: {" 时进入多行状态，
                # 跳过内部的 '---' 分割，并确保结束行 '}' 之后才恢复文档分隔符检查。
                # 当前行就是结束行，我们将其添加到 current，然后退出多行状态。
                current.append(line)
                multiline_key_indent = None
                i += 1
                continue
            else:
                # 内容行，直接加入 current
                current.append(line)
                i += 1
                continue
        # 不在多行字符串内，检查文档分隔符
        if line['content'].strip() == '---':
            if current:
                docs.append(current)
                current = []
            i += 1
            continue
        # 检查是否进入多行字符串：映射条目值部分为 '{' 且下一行存在
        # 这里我们需要快速判断：当前行是否以 ':' 结尾且值为 '{'？
        # 简便方法：检查 content 是否匹配 pattern: key: { 或 key:{ 等
        content = line['content']
        stripped = content.strip()
        if stripped.endswith('{') and ':' in stripped:
            # 可能触发多行字符串，需要更精确的判断：冒号后的部分去除空格后恰为 '{'
            colon_pos = content.find(':')
            if colon_pos != -1:
                after_colon = content[colon_pos+1:].strip()
                if after_colon == '{':
                    # 进入多行字符串，记录 key_indent
                    multiline_key_indent = line['indent']
                    current.append(line)
                    i += 1
                    continue
        # 普通行，加入当前文档
        current.append(line)
        i += 1
    if current or not docs:
        docs.append(current)
    return docs

# ------------------------------------------------------------
# 解析器
# ------------------------------------------------------------
class STMLParser:
    def __init__(self, lines: List[Dict[str, Any]]):
        self.lines = lines
        self.n = len(lines)

    # ----- 注释 -----
    def _is_comment(self, line: Dict) -> bool:
        return line['content'].lstrip(' ').startswith('#')

    def _skip_comments(self, i: int) -> int:
        while i < self.n and self._is_comment(self.lines[i]):
            i += 1
        return i

    # ----- 转义 -----
    def _unescape(self, s: str) -> str:
        result = []
        i = 0
        while i < len(s):
            if s[i] == '\\' and i + 1 < len(s):
                nxt = s[i + 1]
                if nxt == '"':   result.append('"')
                elif nxt == '\\': result.append('\\')
                elif nxt == 'n': result.append('\n')
                elif nxt == 't': result.append('\t')
                else:            result.append('\\' + nxt)
                i += 2
            else:
                result.append(s[i])
                i += 1
        return ''.join(result)

    def _find_first_unescaped_quote(self, s: str, start: int = 0) -> int:
        i = start
        while i < len(s):
            if s[i] == '\\':
                i += 2
                continue
            if s[i] == '"':
                return i
            i += 1
        return -1

    def _find_last_unescaped_quote(self, s: str, start: int = 0) -> int:
        last = -1
        i = start
        while i < len(s):
            if s[i] == '\\':
                i += 2
                continue
            if s[i] == '"':
                last = i
            i += 1
        return last

    def _find_unquoted_colon(self, s: str) -> int:
        in_quote = False
        i = 0
        while i < len(s):
            ch = s[i]
            if ch == '\\':
                i += 2
                continue
            if ch == '"':
                in_quote = not in_quote
            elif ch == ':' and not in_quote:
                return i
            i += 1
        if in_quote:
            return s.find(':')
        return -1

    # 引号键尝试（非贪婪，闭合后紧跟冒号）
    def _try_quoted_key(self, s: str) -> Tuple[str, int, bool]:
        assert s.startswith('"')
        end = self._find_first_unescaped_quote(s, 1)
        if end == -1:
            return "", 0, False
        if end + 1 >= len(s) or s[end + 1] != ':':
            return "", 0, False
        raw = s[1:end]
        key = self._unescape(raw)
        return key, end + 2, True

    # 值的贪婪匹配
    def _parse_quoted_value(self, s: str, start: int) -> Tuple[str, int]:
        """解析双引号字符串（贪婪），返回 (反转义后的字符串, 结束位置)"""
        assert s[start] == '"'
        end = self._find_last_unescaped_quote(s, start + 1)
        if end == -1:
            return "", start
        raw = s[start + 1:end]
        return self._unescape(raw), end + 1

    # 尝试解析可能被引号包裹的标量（用于序列条目）
    def _parse_possible_quoted_scalar(self, s: str) -> Any:
        """若 s 以引号开头且成功匹配，返回反转义字符串；否则按标量处理"""
        if s.startswith('"'):
            val, _ = self._parse_quoted_value(s, 0)
            if _ != 0:    # 成功闭合
                return val
            # 降级为 raw scalar
            return self._parse_scalar(s)
        return self._parse_scalar(s)

    # ----- 顶层 -----
    def parse_document(self):
        if self.n == 0:
            return None
        i = self._skip_comments(0)
        if i >= self.n:
            return None
        return self._parse_node(i, parent_indent=-1)[0]

    def _parse_node(self, i, parent_indent):
        i = self._skip_comments(i)
        if i >= self.n:
            return None, i
        line = self.lines[i]
        if line['indent'] <= parent_indent:
            return None, i
        if line['content'].startswith('-'):
            return self._parse_sequence(i, parent_indent, block_indent=None)
        else:
            return self._parse_mapping(i, parent_indent)

    # ---------- 映射 ----------
    def _parse_mapping(self, i, parent_indent):
        mapping = {}
        while i < self.n:
            i = self._skip_comments(i)
            if i >= self.n:
                break
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

        if content.startswith('"'):
            key, after, ok = self._try_quoted_key(content)
            if ok:
                rest = content[after:]
                if rest.startswith(' '):
                    rest = rest[1:]
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
            return self._parse_regular_entry(content, i, line['indent'])

        return self._parse_regular_entry(content, i, line['indent'])

    def _parse_regular_entry(self, content: str, i: int, key_indent: int):
        colon = self._find_unquoted_colon(content)
        if colon == -1:
            stripped = content.rstrip()
            if len(stripped) >= 2 and stripped[0] == '"' and stripped[-1] == '"':
                key = stripped[1:-1] + content[len(stripped):]
            else:
                key = content.rstrip()
            return key, None, i + 1

        key = content[:colon]          # 保留原样，包括可能的空格
        rest = content[colon + 1:]
        if rest.startswith(' '):
            rest = rest[1:]
        return self._finish_key_value(key, rest, i, key_indent)

    def _finish_key_value(self, key: str, rest: str, i: int, key_indent: int):
        if rest.startswith('"'):
            val, _ = self._parse_quoted_value(rest, 0)
            if _ != 0:
                return key, val, i + 1
            return key, self._parse_scalar(rest), i + 1

        if rest.strip() == '{' and i + 1 < self.n:
            ml, ni = self._parse_multiline(i, key_indent)
            return key, ml, ni

        if rest.strip() == '':
            if i + 1 < self.n:
                val, ni = self._parse_block_value(i + 1, key_indent, is_nested=False)
                return key, val, ni
            return key, None, i + 1

        val = self._parse_inline_value(rest)
        return key, val, i + 1

    def _parse_block_value(self, i, key_indent, is_nested):
        i = self._skip_comments(i)
        if i >= self.n:
            return None, i
        line = self.lines[i]
        if line['content'].startswith('-'):
            return self._parse_sequence(i, key_indent,
                                        block_indent=key_indent if is_nested else None)
        if line['indent'] <= key_indent:
            return None, i
        return self._parse_mapping(i, key_indent)

    # ---------- 序列 ----------
    def _is_complex_sequence(self, i, parent_indent, block_indent):
        cur = i
        while cur < self.n:
            cur = self._skip_comments(cur)
            if cur >= self.n:
                break
            line = self.lines[cur]
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
            i = self._skip_comments(i)
            if i >= self.n:
                break
            line = self.lines[i]
            if block_indent is not None and line['indent'] <= block_indent:
                break
            if line['indent'] < parent_indent:
                break
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

        # 空条目
        if rest.strip() == '':
            if i + 1 < self.n and self.lines[i + 1]['indent'] > item_indent:
                val, ni = self._parse_block_value(i + 1, item_indent, is_nested=True)
                return val, ni
            return None, i + 1

        has_colon = ':' in rest
        need_block = False
        if has_colon:
            colon_pos = rest.find(':')
            after_colon = rest[colon_pos + 1:]
            if after_colon.startswith(' '):
                after_colon = after_colon[1:]
            if after_colon.strip() == '':
                need_block = True
        has_block = need_block and (i + 1 < self.n and self.lines[i + 1]['indent'] > item_indent)

        # 无冒号且无子块
        if not has_colon and not has_block:
            if simple_as_scalar:
                # 简单序列：解析为标量（先尝试引号值）
                return self._parse_possible_quoted_scalar(rest.strip()), i + 1
            else:
                # 复杂序列：作为单键映射，键需剥离引号
                key = rest.strip()
                if key.startswith('"'):
                    val, _ = self._parse_quoted_value(key, 0)
                    if _ != 0:
                        key = val
                return {key: None}, i + 1

        if not has_colon:
            # 无冒号但有子块？理论上不会进入这里，回退为标量
            return self._parse_possible_quoted_scalar(rest.strip()), i + 1

        # 内联映射
        colon = rest.find(':')
        inline_key = rest[:colon].rstrip()
        inline_rest = rest[colon + 1:]
        if inline_rest.startswith(' '):
            inline_rest = inline_rest[1:]

        # 键可能是引号包裹的，需要剥离
        if inline_key.startswith('"'):
            key_val, end = self._parse_quoted_value(inline_key, 0)
            if end != 0:
                inline_key = key_val

        if inline_rest.strip() == '{' and i + 1 < self.n:
            ml, ni = self._parse_multiline(i, item_indent)
            return {inline_key: ml}, ni

        if inline_rest.strip() == '':
            if has_block:
                sub_val, ni = self._parse_block_value(i + 1, item_indent, is_nested=True)
                return {inline_key: sub_val}, ni
            return {inline_key: None}, i + 1

        # 行内值，复用值的解析（会处理引号）
        sub_val = self._parse_inline_value(inline_rest)
        return {inline_key: sub_val}, i + 1

    # ---------- 标量 / 内联值 ----------
    def _parse_scalar(self, s: str) -> Any:
        s = s.strip()
        if s == '~' or s == 'null':
            return None
        return s

    def _parse_inline_value(self, s: str) -> Any:
        s = s.strip()
        if s.startswith('"'):
            val, _ = self._parse_quoted_value(s, 0)
            if _ != 0:
                return val
            return self._parse_scalar(s)
        if s.startswith('[') and s.endswith(']'):
            return self._parse_inline_list(s)
        return self._parse_scalar(s)

    def _parse_inline_list(self, s: str) -> List[Any]:
        inner = s[1:-1]
        elements = []
        i = 0
        n = len(inner)
        while i < n:
            while i < n and inner[i] == ' ':
                i += 1
            if i >= n:
                break
            if inner[i] == '"':
                end = self._find_first_unescaped_quote(inner, i + 1)
                if end != -1:
                    raw = inner[i + 1:end]
                    elements.append(self._unescape(raw))
                    i = end + 1
                else:
                    start = i
                    while i < n and inner[i] != ',':
                        i += 1
                    elements.append(self._parse_scalar(inner[start:i].strip()))
                while i < n and inner[i] in (',', ' '):
                    i += 1
            else:
                start = i
                while i < n and inner[i] != ',':
                    i += 1
                elements.append(self._parse_scalar(inner[start:i].strip()))
                while i < n and inner[i] in (',', ' '):
                    i += 1
        return elements

    # ---------- 多行字符串 ----------
    def _parse_multiline(self, i: int, key_indent: int) -> Tuple[str, int]:
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
# 接口
# ------------------------------------------------------------
def loads(text: str) -> Dict[str, Any]:
    lines = tokenize(text)
    doc_blocks = split_docs(lines)
    docs = {}
    for idx, block in enumerate(doc_blocks, start=1):
        parser = STMLParser(block)
        node = parser.parse_document()
        docs[f'doc{idx}'] = node
    return {'docs': docs}

def load(filename: str) -> Dict[str, Any]:
    with open(filename, 'r', encoding='utf-8') as f:
        return loads(f.read())

# ------------------------------------------------------------
if __name__ == '__main__':
    # input_file = './测试1.txt'
    # output_file = './测试1输出.json'
    input_file = './PreValidation/测试4_error.txt'
    output_file = './PreValidation/测试4_error输出.json'
    # input_file = './测试3.stml'
    # output_file = './测试3输出.json'
    with open(input_file, encoding='utf-8') as f:
        text = f.read()
    result = loads(text)
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=4)
    print(f"解析完成，结果已写入 {output_file}")