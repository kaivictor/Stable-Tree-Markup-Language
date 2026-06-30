/// STML Serializer — AST → canonical STML / JSON
/// Ported from stml_cpp/serializer/to_stml.cpp and to_json.cpp

import { AstNode, mapFind } from './ast';

// =========================================================================
// Escape helpers
// =========================================================================

function escapeString(s: string): string {
  let result = '';
  for (const ch of s) {
    switch (ch) {
      case '\\': result += '\\\\'; break;
      case '"':  result += '\\"'; break;
      case '\n': result += '\\n'; break;
      case '\t': result += '\\t'; break;
      default:   result += ch; break;
    }
  }
  return result;
}

function jsonEscape(s: string): string {
  let result = '';
  for (const ch of s) {
    switch (ch) {
      case '\\': result += '\\\\'; break;
      case '"':  result += '\\"'; break;
      case '\n': result += '\\n'; break;
      case '\r': result += '\\r'; break;
      case '\t': result += '\\t'; break;
      case '\b': result += '\\b'; break;
      case '\f': result += '\\f'; break;
      default:
        if (ch.charCodeAt(0) < 0x20) {
          result += '\\u00' + '0123456789abcdef'[ch.charCodeAt(0) >> 4]
                              + '0123456789abcdef'[ch.charCodeAt(0) & 0xf];
        } else {
          result += ch;
        }
        break;
    }
  }
  return result;
}

function quoteKey(key: string): string {
  return '"' + escapeString(key) + '"';
}

// =========================================================================
// formatScalar
// =========================================================================

export function formatScalar(node: AstNode): string {
  if (node.isNull()) return 'null';
  if (node.isString()) return '"' + escapeString(node.asString()!) + '"';
  if (node.isMap() && node.asMap()!.length === 0) return 'null';
  if (node.isList() && node.asList()!.length === 0) return 'null';
  throw new Error('formatScalar called on non-scalar node');
}

// =========================================================================
// serializeNode
// =========================================================================

function valuesAreAllScalars(lst: AstNode[]): boolean {
  for (const v of lst) {
    if (v.isList() || v.isMap()) return false;
  }
  return true;
}

function formatInlineList(lst: AstNode[]): string {
  let result = '[';
  for (let i = 0; i < lst.length; i++) {
    if (i > 0) result += ', ';
    result += formatScalar(lst[i]);
  }
  result += ']';
  return result;
}

export function serializeNode(node: AstNode, indentLevel: number = 0): string {
  const indent = ' '.repeat(indentLevel * 2);
  const nextIndent = ' '.repeat((indentLevel + 1) * 2);

  if (node.isNull()) {
    return indent + 'null';
  }

  if (node.isString()) {
    return indent + '"' + escapeString(node.asString()!) + '"';
  }

  if (node.isList()) {
    const lst = node.asList()!;
    if (lst.length === 0) {
      return indent + '- null';
    }

    const lines: string[] = [];
    for (const item of lst) {
      if (item.isNull()) {
        lines.push(indent + '-');
      } else if (item.isString()) {
        lines.push(indent + '- ' + formatScalar(item));
      } else if (item.isList()) {
        const innerLst = item.asList()!;
        if (innerLst.length === 0) {
          lines.push(indent + '- null');
        } else if (valuesAreAllScalars(innerLst)) {
          lines.push(indent + '- ' + formatInlineList(innerLst));
        } else {
          lines.push(indent + '- ');
          lines.push(serializeNode(item, indentLevel + 1));
        }
      } else if (item.isMap()) {
        const itemMap = item.asMap()!;
        if (itemMap.length === 0) {
          lines.push(indent + '- null');
          continue;
        }
        let first = true;
        for (const [key, val] of itemMap) {
          const entryLevel = first ? indentLevel : indentLevel + 1;
          const entryIndent = ' '.repeat(entryLevel * 2);
          const prefix = first ? '- ' : '';
          if (val.isList() || val.isMap()) {
            lines.push(entryIndent + prefix + quoteKey(key) + ':');
            lines.push(serializeNode(val, entryLevel + 1));
          } else if (val.isNull()) {
            lines.push(entryIndent + prefix + quoteKey(key));
          } else {
            lines.push(entryIndent + prefix + quoteKey(key) + ': ' + formatScalar(val));
          }
          first = false;
        }
      }
    }
    return lines.join('\n');
  }

  if (node.isMap()) {
    const map = node.asMap()!;
    if (map.length === 0) {
      return indent + 'null';
    }

    const lines: string[] = [];
    for (const [key, val] of map) {
      const kStr = quoteKey(key);
      if (val.isList() || val.isMap()) {
        const sub = serializeNode(val, indentLevel + 1);
        if (sub.trim().length === 0) {
          lines.push(indent + kStr + ': null');
        } else {
          lines.push(indent + kStr + ':');
          lines.push(sub);
        }
      } else {
        lines.push(indent + kStr + ': ' + formatScalar(val));
      }
    }
    return lines.join('\n');
  }

  throw new Error('Unsupported node type in serializeNode');
}

// =========================================================================
// dumps — top-level serialization
// =========================================================================

export function dumps(node: AstNode): string {
  if (node.isMap()) {
    const map = node.asMap()!;
    const docsNode = mapFind(map, 'docs');
    if (docsNode !== null && docsNode.isList()) {
      const docs = docsNode.asList()!;
      const parts = docs.map(doc => serializeNode(doc, 0));
      let result = '';
      for (let i = 0; i < parts.length; i++) {
        result += parts[i];
        if (i + 1 < parts.length) result += '\n---';
        result += '\n';
      }
      return result;
    }
  }

  return serializeNode(node, 0) + '\n';
}

// =========================================================================
// to_json — AST → canonical JSON text
// =========================================================================

function toJsonImpl(node: AstNode, indentLevel: number = 0): string {
  const indent = ' '.repeat(indentLevel * 2);
  const nextIndent = ' '.repeat((indentLevel + 1) * 2);

  if (node.isNull()) {
    return 'null';
  } else if (node.isString()) {
    return '"' + jsonEscape(node.asString()!) + '"';
  } else if (node.isList()) {
    const lst = node.asList()!;
    if (lst.length === 0) {
      return '[]';
    } else {
      let result = '[\n';
      for (let i = 0; i < lst.length; i++) {
        result += nextIndent + toJsonImpl(lst[i], indentLevel + 1);
        if (i + 1 < lst.length) result += ',';
        result += '\n';
      }
      result += indent + ']';
      return result;
    }
  } else if (node.isMap()) {
    const map = node.asMap()!;
    if (map.length === 0) {
      return '{}';
    } else {
      let result = '{\n';
      for (let i = 0; i < map.length; i++) {
        const [k, v] = map[i];
        result += nextIndent + '"' + jsonEscape(k) + '": ' + toJsonImpl(v, indentLevel + 1);
        if (i + 1 < map.length) result += ',';
        result += '\n';
      }
      result += indent + '}';
      return result;
    }
  }
  return 'null';
}

export function toJson(node: AstNode): string {
  return toJsonImpl(node, 0) + '\n';
}