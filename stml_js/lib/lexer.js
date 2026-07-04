/// STML Lexer — converts STML text into a token stream
/// Ported from stml_cpp/lexer/lexer.cpp

const { nullNode, stringNode, listNode } = require('./ast');

// ---- Token Types ----
const TokenType = {
  NEWLINE:          'NEWLINE',
  INDENT:           'INDENT',
  DEDENT:           'DEDENT',
  DOC_SEPARATOR:    'DOC_SEPARATOR',
  END:              'END',
  KEY:              'KEY',
  BARE_KEY:         'BARE_KEY',
  COLON:            'COLON',
  SCALAR:           'SCALAR',
  NULL_:            'NULL_',
  RAW_STRING:       'RAW_STRING',
  DASH:             'DASH',
  INLINE_LIST:      'INLINE_LIST',
  MULTILINE_STRING: 'MULTILINE_STRING',
};

class Token {
  constructor(type, value, line, column) {
    this.type = type;
    this.value = value;
    this.line = line;
    this.column = column;
  }
}

class Warning {
  constructor(line, column, message) {
    this.line = line;
    this.column = column;
    this.message = message;
  }
}

class ParseError extends Error {
  constructor(line, column, message) {
    super(`${line}:${column}: ${message}`);
    this.line = line;
    this.column = column;
    this.message = message;
  }
}

class STMLLexer {
  constructor(text) {
    // Normalise line endings: \r\n → \n, \r → \n
    let normalised = '';
    for (let i = 0; i < text.length; i++) {
      if (text[i] === '\r') {
        if (i + 1 < text.length && text[i + 1] === '\n') i++;
        normalised += '\n';
      } else {
        normalised += text[i];
      }
    }
    this.lines = normalised.split('\n');
    this.lineCount = this.lines.length;
    this.tokens = [];
    this.warnings = [];
    this.indentStack = [0];
    this.lineIdx = 0;
    this.deferredDedentTo = null;
  }

  tokenize() {
    this.tokens = [];
    this.warnings = [];
    this.indentStack = [0];
    this.lineIdx = 0;
    this.deferredDedentTo = null;

    while (this.lineIdx < this.lineCount) {
      const line = this.lines[this.lineIdx];
      const indent = this._calcIndent(line);
      const content = line.slice(indent);

      // Skip empty lines and whole-line comments
      if (this._isBlankOrComment(content)) {
        this.lineIdx++;
        continue;
      }

      // Document separator (indent must be 0)
      const stripped = content.trim();
      if (indent === 0 && stripped === '---') {
        this._emitDedentsTo(0);
        this._addToken(TokenType.DOC_SEPARATOR, '---', this.lineIdx + 1, indent + 1);
        this.lineIdx++;
        continue;
      }

      // Process indentation changes
      this._processIndent(indent);

      // Dispatch on line type
      if (stripped === '---' && indent > 0) {
        this._lexMappingLine(content, indent);
      } else if (content.length > 0 && content[0] === '-') {
        this._lexSequenceLine(content, indent);
      } else {
        this._lexMappingLine(content, indent);
      }

      this.lineIdx++;

      // Emit deferred DEDENTs from multiline-string closure
      if (this.deferredDedentTo !== null) {
        const target = this.deferredDedentTo;
        this.deferredDedentTo = null;
        this._emitDedentsTo(target);
      }
    }

    // EOF: pop remaining indentation
    this._emitDedentsTo(0);
    this._addToken(TokenType.END, null, this.lineCount, 1);

    return { tokens: this.tokens, warnings: this.warnings };
  }

  // ---- Indentation helpers ----
  _calcIndent(line) {
    let n = 0;
    for (const ch of line) {
      if (ch === ' ') n++;
      else break;
    }
    return n;
  }

  _isBlankOrComment(content) {
    const trimmed = content.trimStart();
    return trimmed.length === 0 || trimmed[0] === '#';
  }

  _processIndent(indent) {
    const top = this.indentStack[this.indentStack.length - 1];
    if (indent > top) {
      this.indentStack.push(indent);
      this._addToken(TokenType.INDENT, String(indent - top), this.lineIdx + 1, 1);
    } else if (indent < top) {
      this._emitDedentsTo(indent);
      if (indent > this.indentStack[this.indentStack.length - 1]) {
        const diff = indent - this.indentStack[this.indentStack.length - 1];
        this.indentStack.push(indent);
        this._addToken(TokenType.INDENT, String(diff), this.lineIdx + 1, 1);
      }
    }
  }

  _emitDedentsTo(target) {
    while (this.indentStack[this.indentStack.length - 1] > target) {
      this.indentStack.pop();
      this._addToken(TokenType.DEDENT, null, this.lineIdx + 1, 1);
    }
  }

  // ---- Token factory ----
  _addToken(type, value, line, column) {
    this.tokens.push(new Token(type, value, line, column));
  }

  _addWarning(line, column, message) {
    this.warnings.push(new Warning(line, column, message));
  }

  _col(indent, offsetInContent) {
    return indent + offsetInContent + 1;
  }

  // ---- Line-level dispatch ----
  _lexSequenceLine(content, indent) {
    const lineNo = this.lineIdx + 1;
    this._addToken(TokenType.DASH, null, lineNo, this._col(indent, 0));

    let rest = content.slice(1);
    if (rest.length > 0 && rest[0] === ' ') rest = rest.slice(1);

    if (rest.length === 0) {
      this._addToken(TokenType.NEWLINE, null, lineNo, this._col(indent, content.length));
      return;
    }

    this._lexValueOrKeyOnLine(rest, indent, true);
    this._addToken(TokenType.NEWLINE, null, lineNo, this._col(indent, content.length));
  }

  _lexMappingLine(content, indent) {
    const lineNo = this.lineIdx + 1;

    // (1) Try quoted-key mode
    if (content.length > 0 && content[0] === '"') {
      const [key, rest, ok, colonPos] = this._tryQuotedKey(content, indent);
      if (ok) {
        this._addToken(TokenType.KEY, key, lineNo, this._col(indent, 0));
        this._addToken(TokenType.COLON, null, lineNo, this._col(indent, colonPos));
        this._lexRemainderAfterColon(rest, indent, lineNo);
        this._addToken(TokenType.NEWLINE, null, lineNo, this._col(indent, content.length));
        return;
      }
    }

    // (2) Scan for structural colon
    const colon = this._findUnquotedColon(content);

    if (colon === -1) {
      // No structural colon → bare-key or null-scalar
      const trimmed = content.trim();
      if (trimmed === 'null' || trimmed === '~') {
        this._addToken(TokenType.NULL_, null, lineNo, this._col(indent, 0));
        this._addToken(TokenType.NEWLINE, null, lineNo, this._col(indent, content.length));
        return;
      }

      const key = this._bareKeyFromContent(content);
      this._addToken(TokenType.BARE_KEY, key, lineNo, this._col(indent, 0));
      this._addToken(TokenType.NEWLINE, null, lineNo, this._col(indent, content.length));
      return;
    }

    // (3) Colon at position 0 → empty key
    if (colon === 0) {
      const key = this._reconstructEmptyKey(content);
      this._addToken(TokenType.BARE_KEY, key, lineNo, this._col(indent, 0));
      this._addToken(TokenType.NEWLINE, null, lineNo, this._col(indent, content.length));
      return;
    }

    // (4) Normal key: content before colon
    const key = content.slice(0, colon);
    const rest = content.slice(colon + 1);

    this._addToken(TokenType.KEY, key, lineNo, this._col(indent, 0));
    this._addToken(TokenType.COLON, null, lineNo, this._col(indent, colon));
    this._lexRemainderAfterColon(rest, indent, lineNo);
    this._addToken(TokenType.NEWLINE, null, lineNo, this._col(indent, content.length));
  }

  // ---- Value parsing ----
  _lexRemainderAfterColon(rest, indent, lineNo) {
    if (rest.length === 0) return;

    let r = rest;
    if (r[0] === ' ' || r[0] === '\t') r = r.slice(1);
    if (r.length === 0) return;

    this._lexValueOrKeyOnLine(r, indent, false);
  }

  _lexValueOrKeyOnLine(text, indent, scanColon) {
    const lineNo = this.lineIdx + 1;
    const trimmed = text.trim();

    // Multiline string trigger: content is exactly '{'
    if (trimmed === '{' && this.lineIdx + 1 < this.lineCount) {
      const [mlValue, consumed, closeIndent] = this._readMultiline(indent);
      this._addToken(TokenType.MULTILINE_STRING, mlValue, lineNo, this._col(indent, indent));
      this.lineIdx = consumed - 1;
      this.deferredDedentTo = closeIndent;
      return;
    }

    // Inline list: only triggers when both '[' and ']' are on the line
    if (text.length > 0 && text[0] === '[') {
      const rtrimmed = text.trimEnd();
      if (rtrimmed.length > 1 && rtrimmed[rtrimmed.length - 1] === ']') {
        const [elems, ok, col] = this._parseInlineList(text, indent);
        if (ok) {
          this._addToken(TokenType.INLINE_LIST, elems, lineNo, this._col(indent, 0));
          return;
        }
        this._addWarning(lineNo, col || this._col(indent, 0), '内联列表缺少结尾 \']\'，降级为原始字符串');
        this._addToken(TokenType.RAW_STRING, text, lineNo, this._col(indent, 0));
        return;
      }
    }

    // Value starts with '[' but no matching ']' → malformed
    if (text.length > 0 && text[0] === '[') {
      if (!text.includes(']')) {
        this._addWarning(lineNo, this._col(indent, 0), '内联列表缺少结尾 \']\'，降级为原始字符串');
        this._addToken(TokenType.RAW_STRING, text, lineNo, this._col(indent, 0));
        return;
      }
    }

    // Quoted content
    if (text.length > 0 && text[0] === '"') {
      if (scanColon) {
        const [key, rest, ok, colonPos] = this._tryQuotedKey(text, indent);
        if (ok) {
          this._addToken(TokenType.KEY, key, lineNo, this._col(indent, 0));
          this._addToken(TokenType.COLON, null, lineNo, this._col(indent, colonPos));
          let r = rest;
          if (r.length > 0 && r[0] === ' ') r = r.slice(1);
          this._lexValueOrKeyOnLine(r, indent + (text.length - rest.length), true);
          return;
        }
      }
      const [val, end, ok] = this._parseQuotedValue(text, 0, indent);
      if (ok) {
        this._addToken(TokenType.SCALAR, val, lineNo, this._col(indent, 0));
      } else {
        this._addWarning(lineNo, this._col(indent, 0), '引号未闭合，降级为原始字符串');
        this._addToken(TokenType.RAW_STRING, text, lineNo, this._col(indent, 0));
      }
      return;
    }

    // Unquoted value → for sequence entries, check for inline mapping key:value
    if (scanColon) {
      const c = this._findUnquotedColon(text);
      if (c >= 0) {
        const inlineKey = text.slice(0, c);
        let inlineRest = text.slice(c + 1);
        if (inlineRest.length > 0 && inlineRest[0] === ' ') inlineRest = inlineRest.slice(1);
        this._addToken(TokenType.KEY, inlineKey, lineNo, this._col(indent, 0));
        this._addToken(TokenType.COLON, null, lineNo, this._col(indent, inlineKey.length));
        this._lexValueOrKeyOnLine(inlineRest, indent + inlineKey.length + 1, true);
        return;
      }
    }

    // Plain scalar
    this._lexScalarOrNull(text, indent);
  }

  _lexScalarOrNull(text, indent) {
    const lineNo = this.lineIdx + 1;
    // Trim only spaces (U+0020), preserve tabs and other whitespace
    const stripped = this._trimSpacesOnly(text);

    if (stripped.length === 0) return;

    if (stripped === 'null' || stripped === '~') {
      this._addToken(TokenType.NULL_, null, lineNo, this._col(indent, 0));
    } else {
      this._addToken(TokenType.SCALAR, stripped, lineNo, this._col(indent, 0));
    }
  }

  // ---- Quoted-key attempt ----
  _tryQuotedKey(content, indent) {
    // 从位置1开始搜索闭合引号+冒号的组合
    let pos = 1;
    while (pos < content.length) {
      const end = this._findFirstUnescapedQuote(content, pos);
      if (end === -1) return ['', '', false, -1];
      // 检查闭合引号后是否紧跟冒号
      if (end + 1 < content.length && content[end + 1] === ':') {
        const rawKey = content.slice(1, end);
        const key = this._unescape(rawKey, indent + 1);
        const rest = content.slice(end + 2);
        return [key, rest, true, end + 1];
      }
      // 不是闭合引号+冒号，继续搜索下一个引号
      pos = end + 1;
    }
    return ['', '', false, -1];
  }

  // ---- Structural colon scan ----
  _findUnquotedColon(s) {
    let inQuote = false;
    let hadClose = false;
    let i = 0;
    while (i < s.length) {
      const ch = s[i];
      if (ch === '\\') {
        i += 2;
        continue;
      }
      if (ch === '"') {
        if (inQuote) hadClose = true;
        inQuote = !inQuote;
      } else if (ch === ':' && !inQuote) {
        return i;
      }
      i++;
    }

    if (inQuote && !hadClose) {
      const pos = s.indexOf(':');
      return pos;
    }
    return -1;
  }

  // ---- Quote matching ----
  _findFirstUnescapedQuote(s, start) {
    let i = start;
    while (i < s.length) {
      if (s[i] === '\\') { i += 2; continue; }
      if (s[i] === '"') return i;
      i++;
    }
    return -1;
  }

  _findLastUnescapedQuote(s, start) {
    let last = -1;
    let i = start;
    while (i < s.length) {
      if (s[i] === '\\') { i += 2; continue; }
      if (s[i] === '"') last = i;
      i++;
    }
    return last;
  }

  // ---- Escape processing ----
  _unescape(s, colOffset) {
    let result = '';
    let i = 0;
    while (i < s.length) {
      if (s[i] === '\\' && i + 1 < s.length) {
        const nxt = s[i + 1];
        switch (nxt) {
          case '"': result += '"'; break;
          case '\\': result += '\\'; break;
          case 'n': result += '\n'; break;
          case 't': result += '\t'; break;
          default:
            result += '\\' + nxt;
            this._addWarning(this.lineIdx + 1, colOffset + i + 1, `非法转义序列 '\\${nxt}'，保留原样`);
            break;
        }
        i += 2;
      } else {
        result += s[i];
        i++;
      }
    }
    return result;
  }

  // ---- Quoted value ----
  _parseQuotedValue(s, start, indent) {
    const end = this._findLastUnescapedQuote(s, start + 1);
    if (end === -1) return ['', start, false];
    const raw = s.slice(start + 1, end);
    const val = this._unescape(raw, indent + start + 1);
    return [val, end + 1, true];
  }

  // ---- Inline list ----
  _parseInlineList(s, indent) {
    const closePos = s.lastIndexOf(']');
    if (closePos === -1) return [[], false, indent + s.length];

    // Check content after ']' is just whitespace
    for (let i = closePos + 1; i < s.length; i++) {
      if (s[i] !== ' ') return [[], false, indent + i];
    }

    const inner = s.slice(1, closePos);
    const elements = [];
    let i = 0;

    while (i < inner.length) {
      while (i < inner.length && inner[i] === ' ') i++;
      if (i >= inner.length) break;

      if (inner[i] === '"') {
        const eq = this._findFirstUnescapedQuote(inner, i + 1);
        if (eq !== -1) {
          const rawElem = inner.slice(i + 1, eq);
          elements.push(this._unescape(rawElem, indent + i + 2));
          i = eq + 1;
        } else {
          elements.push(inner.slice(i));
          i = inner.length;
        }
        while (i < inner.length && (inner[i] === ',' || inner[i] === ' ')) i++;
      } else {
        const startI = i;
        while (i < inner.length && inner[i] !== ',') i++;
        const elem = inner.slice(startI, i).trim();
        if (elem === 'null' || elem === '~' || elem.length === 0) {
          elements.push(null);
        } else {
          elements.push(elem);
        }
        while (i < inner.length && (inner[i] === ',' || inner[i] === ' ')) i++;
      }
    }

    // Trailing comma → extra null
    const trimmed = inner.trimEnd();
    if (trimmed.length > 0 && trimmed[trimmed.length - 1] === ',') {
      elements.push(null);
    }

    return [elements, true, 0];
  }

  // ---- Multiline string ----
  _readMultiline(keyIndent) {
    const parts = [];
    let i = this.lineIdx + 1;
    let closeIndent = keyIndent;

    while (i < this.lineCount) {
      const line = this.lines[i];
      let lineIndent = 0;
      for (const ch of line) {
        if (ch === ' ') lineIndent++;
        else break;
      }
      const content = line.slice(lineIndent);
      const stripped = content.trim();

      if (stripped === '}' && lineIndent <= keyIndent) {
        i++;
        break;
      }

      parts.push(line);
      i++;
    }

    if (i >= this.lineCount) {
      this._addWarning(i, 1, '多行字符串未找到闭合 \'}\'，已由 EOF 自动闭合');
    }

    return [parts.join('\n'), i, closeIndent];
  }

  // ---- Utility helpers ----
  _trimSpacesOnly(str) {
    // Trim only spaces (U+0020), preserving tabs and other whitespace
    let start = 0;
    let end = str.length;
    while (start < end && str.charCodeAt(start) === 0x20) start++;
    while (end > start && str.charCodeAt(end - 1) === 0x20) end--;
    return str.slice(start, end);
  }

  // ---- Bare-key helpers ----
  _bareKeyFromContent(content) {
    const trimmed = content.trimEnd();
    if (trimmed.length >= 2 && trimmed[0] === '"' && trimmed[trimmed.length - 1] === '"') {
      const inner = trimmed.slice(1, -1);
      return inner + content.slice(trimmed.length);
    }
    return trimmed;
  }

  _reconstructEmptyKey(content) {
    let rest = content.slice(1);
    const hadSpace = rest.length > 0 && rest[0] === ' ';
    if (hadSpace) rest = rest.slice(1);

    let processed;
    if (rest.length > 0 && rest[0] === '"') {
      const [val, end, ok] = this._parseQuotedValue(rest, 0, 0);
      processed = ok ? val : rest;
    } else {
      processed = rest.trim();
    }

    if (hadSpace) return ':' + ' ' + processed;
    return ':' + processed;
  }
}

module.exports = { STMLLexer, TokenType, Token, Warning, ParseError };