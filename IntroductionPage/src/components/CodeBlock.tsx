interface CodeBlockProps {
  code: string
  lang?: 'stml' | 'yaml' | 'json'
  stml?: boolean
  style?: React.CSSProperties
  errLine?: number
  errMsg?: string
  showLineNumbers?: boolean
  showPointer?: boolean
  noHeader?: boolean
  forceLastBraceOrange?: boolean
  variant?: string
}

/* 计算一行中花括号的净深度变化（忽略字符串内的） */
function braceDelta(line: string): number {
  let d = 0
  let inStr = false
  for (let i = 0; i < line.length; i++) {
    if (line[i] === '"') { inStr = !inStr; continue }
    if (inStr) continue
    if (line[i] === '{') d++
    else if (line[i] === '}') d--
  }
  return d
}

/* ═══ JSON 高亮 ═══ */
function findEndQuote(text: string, startIdx: number): number {
  // 跳过转义引号，找到真正的结束引号
  let i = startIdx + 1
  while (i < text.length) {
    if (text[i] === '\\') { i += 2; continue }
    if (text[i] === '"') return i
    i++
  }
  return -1
}

function highlightJsonValue(text: string): string {
  let result = ''
  let i = 0
  while (i < text.length) {
    if (text[i] === ' ' || text[i] === '\t') {
      result += text[i]
      i++
      continue
    }
    if (text[i] === '"') {
      const endQuote = findEndQuote(text, i)
      if (endQuote !== -1) {
        result += `<span class="j-v">${escHtml(text.slice(i, endQuote + 1))}</span>`
        i = endQuote + 1
        continue
      }
    }
    if (text.slice(i).match(/^(true|false)\b/)) {
      const len = text.slice(i).startsWith('true') ? 4 : 5
      result += `<span class="j-kw">${text.slice(i, i + len)}</span>`
      i += len
      continue
    }
    if (text.slice(i).match(/^null\b/)) {
      result += `<span class="j-kw">null</span>`
      i += 4
      continue
    }
    const numMatch = text.slice(i).match(/^-?\d+\.?\d*([eE][+-]?\d+)?/)
    if (numMatch) {
      result += `<span class="j-n">${numMatch[0]}</span>`
      i += numMatch[0].length
      continue
    }
    if (text[i] === '{' || text[i] === '}') {
      result += `<span class="j-br">${text[i]}</span>`
      i++
      continue
    }
    if (text[i] === '[' || text[i] === ']') {
      result += `<span class="j-br">${text[i]}</span>`
      i++
      continue
    }
    if (text[i] === ':' || text[i] === ',') {
      result += `<span class="j-p">${text[i]}</span>`
      i++
      continue
    }
    result += escHtml(text[i])
    i++
  }
  return result
}

function highlightJsonLine(line: string): string {
  let i = 0
  let leading = ''
  while (i < line.length && (line[i] === ' ' || line[i] === '\t')) {
    leading += line[i]
    i++
  }

  // 检测 "key": value 行
  if (i < line.length && line[i] === '"') {
    const endQuote = findEndQuote(line, i)
    if (endQuote !== -1 && endQuote + 1 < line.length && line[endQuote + 1] === ':') {
      const keyText = line.slice(i, endQuote + 1)
      const afterColon = line.slice(endQuote + 2)
      return `${escHtml(leading)}<span class="j-k">${escHtml(keyText)}</span><span class="j-p">:</span>${highlightJsonValue(afterColon)}`
    }
  }

  return escHtml(leading) + highlightJsonValue(line.slice(i))
}

/* ═══ YAML 高亮 ═══ */
function findEndSingleQuote(text: string, startIdx: number): number {
  // YAML 单引号字符串中 '' 表示转义
  let i = startIdx + 1
  while (i < text.length) {
    if (text[i] === "'" && text[i + 1] === "'") { i += 2; continue }
    if (text[i] === "'") return i
    i++
  }
  return -1
}

function highlightYamlValue(text: string): string {
  let result = ''
  let i = 0
  let flowDepth = 0  // 跟踪流式集合 [ ] { } 的嵌套深度
  while (i < text.length) {
    if (text[i] === ' ' || text[i] === '\t') {
      result += text[i]
      i++
      continue
    }
    // 流式集合外的 # → 注释
    if (text[i] === '#' && flowDepth === 0) {
      result += `<span class="y-c">${escHtml(text.slice(i))}</span>`
      break
    }
    // 双引号字符串
    if (text[i] === '"') {
      const endQuote = findEndQuote(text, i)
      if (endQuote !== -1) {
        result += `<span class="y-v">${escHtml(text.slice(i, endQuote + 1))}</span>`
        i = endQuote + 1
        continue
      }
    }
    // 单引号字符串
    if (text[i] === "'") {
      const endQuote = findEndSingleQuote(text, i)
      if (endQuote !== -1) {
        result += `<span class="y-v">${escHtml(text.slice(i, endQuote + 1))}</span>`
        i = endQuote + 1
        continue
      }
    }
    // 流式集合开始
    if (text[i] === '[' || text[i] === '{') {
      result += `<span class="y-br">${text[i]}</span>`
      flowDepth++
      i++
      continue
    }
    // 流式集合结束
    if (text[i] === ']' || text[i] === '}') {
      flowDepth = Math.max(0, flowDepth - 1)
      result += `<span class="y-br">${text[i]}</span>`
      i++
      continue
    }
    // 流式集合内的 : 和 , → 分隔符
    if (flowDepth > 0 && (text[i] === ':' || text[i] === ',')) {
      result += `<span class="y-p">${text[i]}</span>`
      i++
      continue
    }
    // 流式集合内的键名检测: word: 后跟空格或 ] }
    if (flowDepth > 0 && /[a-zA-Z_\u4e00-\u9fff]/.test(text[i])) {
      const keyMatch = text.slice(i).match(/^([a-zA-Z_\u4e00-\u9fff][\w\u4e00-\u9fff/ -]*?):\s/)
      if (keyMatch) {
        result += `<span class="y-k">${escHtml(keyMatch[1])}</span><span class="y-p">:</span> `
        i += keyMatch[0].length
        continue
      }
    }
    // true/false/null 关键字
    if (text.slice(i).match(/^(true|false|null)\b/)) {
      const kw = text.slice(i).match(/^(true|false|null)\b/)![1]
      result += `<span class="y-kw">${kw}</span>`
      i += kw.length
      continue
    }
    // 数字
    const numMatch = text.slice(i).match(/^-?\d+\.?\d*([eE][+-]?\d+)?/)
    if (numMatch && flowDepth > 0) {
      result += `<span class="y-n">${numMatch[0]}</span>`
      i += numMatch[0].length
      continue
    }
    // 流式集合外的裸字符串值（整行到注释或行尾都是值）
    if (flowDepth === 0) {
      const bareEnd = text.indexOf('#', i)
      const bareText = bareEnd === -1 ? text.slice(i) : text.slice(i, bareEnd)
      const trimmed = bareText.trimEnd()
      if (trimmed) {
        result += `<span class="y-v">${escHtml(trimmed)}</span>`
        i += trimmed.length
        // 后面可能有空格和注释
        while (i < text.length && text[i] === ' ') { result += ' '; i++ }
        if (i < text.length && text[i] === '#') {
          result += `<span class="y-c">${escHtml(text.slice(i))}</span>`
          break
        }
        continue
      }
    }
    // 流式集合内的普通文本
    const bareMatch = text.slice(i).match(/^[^"'\[\]{}:,\s#]+/)
    if (bareMatch) {
      result += `<span class="y-v">${escHtml(bareMatch[0])}</span>`
      i += bareMatch[0].length
      continue
    }
    result += escHtml(text[i])
    i++
  }
  return result
}

function highlightYamlLine(line: string): string {
  let i = 0
  let leading = ''
  while (i < line.length && (line[i] === ' ' || line[i] === '\t')) {
    leading += line[i]
    i++
  }

  // 列表项 - key: value
  if (i < line.length && line[i] === '-') {
    const afterDash = line.slice(i + 1)
    if (afterDash.startsWith(' ')) {
      const rest = afterDash.slice(1)
      let liKey = ''
      let liAfter = ''
      let isLiKey = false

      if (rest[0] === '"') {
        const eq = findEndQuote(rest, 0)
        if (eq !== -1 && rest[eq + 1] === ':') {
          isLiKey = true; liKey = rest.slice(0, eq + 1); liAfter = rest.slice(eq + 2)
        }
      } else if (/[a-zA-Z_\u4e00-\u9fff]/.test(rest[0] || '')) {
        const m = rest.match(/^([a-zA-Z_\u4e00-\u9fff][\w\u4e00-\u9fff/ \-]*?):\s?/)
        if (m) { isLiKey = true; liKey = m[1]; liAfter = rest.slice(m[0].length) }
      }

      if (isLiKey) {
        return `${escHtml(leading)}<span class="y-li">- </span><span class="y-k">${escHtml(liKey)}</span><span class="y-p">: </span>${highlightYamlValue(liAfter)}`
      }
    }
    // 普通 - value
    return `${escHtml(leading)}<span class="y-li">- </span>${highlightYamlValue(line.slice(i + 2))}`
  }

  // key: value
  if (i < line.length && line[i] === '"') {
    const eq = findEndQuote(line, i)
    if (eq !== -1 && line[eq + 1] === ':') {
      const keyText = line.slice(i, eq + 1)
      const afterColon = line.slice(eq + 2)
      return `${escHtml(leading)}<span class="y-k">${escHtml(keyText)}</span><span class="y-p">: </span>${highlightYamlValue(afterColon)}`
    }
  } else if (i < line.length && /[a-zA-Z_\u4e00-\u9fff]/.test(line[i])) {
    const m = line.slice(i).match(/^([a-zA-Z_\u4e00-\u9fff][\w\u4e00-\u9fff/ \-]*?):\s?/)
    if (m) {
      const keyText = m[1]
      const afterColon = line.slice(i + m[0].length)
      return `${escHtml(leading)}<span class="y-k">${escHtml(keyText)}</span><span class="y-p">: </span>${highlightYamlValue(afterColon)}`
    }
  }

  // 纯注释行
  if (i < line.length && line[i] === '#') {
    return `${escHtml(leading)}<span class="y-c">${escHtml(line.slice(i))}</span>`
  }

  return escHtml(leading) + highlightYamlValue(line.slice(i))
}

/* ═══ STML 高亮 ═══ */
function highlightLine(line: string, lang: string, isStml: boolean, braceDepth: number, forceLastBraceOrange: boolean = false): string {
  // JSON 和 YAML 使用独立的高亮函数
  if (lang === 'json') return highlightJsonLine(line)
  if (lang === 'yaml') return highlightYamlLine(line)

  let h = line
  let i = 0

  // 跳过前导空白
  let leading = ''
  while (i < h.length && (h[i] === ' ' || h[i] === '\t')) {
    leading += h[i]
    i++
  }

  // 如果在花括号内部（多行文本），不检测键名，整行作为值处理（蓝色）
  if (braceDepth > 0) {
    return escHtml(leading) + highlightValue(h.slice(i), lang, isStml, braceDepth, false, forceLastBraceOrange)
  }

  // 检测是否是列表项键值行: - xxx: 或 - "xxx":
  if (i < h.length && h[i] === '-') {
    const afterDash = h.slice(i + 1)
    if (afterDash.startsWith(' ')) {
      const rest = afterDash.slice(1)
      let liKeyText = ''
      let liAfterColon = ''
      let isLiKeyLine = false

      if (rest[0] === '"') {
        const endQuote = rest.indexOf('"', 1)
        if (endQuote !== -1 && rest[endQuote + 1] === ':') {
          isLiKeyLine = true
          liKeyText = rest.slice(0, endQuote + 1)
          liAfterColon = rest.slice(endQuote + 2)
        }
      } else if (/[a-zA-Z_\u4e00-\u9fff]/.test(rest[0] || '')) {
        const match = rest.match(/^([a-zA-Z_\u4e00-\u9fff][\w\u4e00-\u9fff/ \-]*?):\s?/)
        if (match) {
          isLiKeyLine = true
          liKeyText = match[1]
          liAfterColon = rest.slice(match[0].length)
        }
      }

      if (isLiKeyLine) {
        const valHtml = highlightValue(liAfterColon, lang, isStml, braceDepth, false, forceLastBraceOrange)
        return `${escHtml(leading)}<span class="s-i">- </span><span class="s-k">${escHtml(liKeyText)}</span><span class="s-p">: </span>${valHtml}`
      }
    }
  }

  // 检测是否是键值行: "xxx": 或 xxx:
  let isKeyLine = false
  let keyText = ''
  let afterColon = ''

  if (i < h.length && h[i] === '"') {
    const endQuote = h.indexOf('"', i + 1)
    if (endQuote !== -1 && endQuote + 1 < h.length && h[endQuote + 1] === ':') {
      isKeyLine = true
      keyText = h.slice(i, endQuote + 1)
      afterColon = h.slice(endQuote + 2)
    }
  } else if (i < h.length && /[a-zA-Z_\u4e00-\u9fff]/.test(h[i])) {
    const match = h.slice(i).match(/^([a-zA-Z_\u4e00-\u9fff][\w\u4e00-\u9fff/ \-]*?):\s?/)
    if (match) {
      isKeyLine = true
      keyText = match[1]
      afterColon = h.slice(i + match[0].length)
    }
  }

  if (isKeyLine) {
    // 所有语言统一使用 highlightValue 处理值部分，确保符号颜色一致
    const valHtml = highlightValue(afterColon, lang, isStml, braceDepth, false, forceLastBraceOrange)
    return `${escHtml(leading)}<span class="s-k">${escHtml(keyText)}</span><span class="s-p">: </span>${valHtml}`
  }

  // 非键值行：整个剩余部分作为值处理
  return escHtml(leading) + highlightValue(h.slice(i), lang, isStml, braceDepth, false, forceLastBraceOrange)
}

/* 检查是否在同一行内存在匹配的右方括号，且中间无嵌套 [ （用于检测真正的"内联列表"如 ["a","b"]） */
function hasInlineCloseBracketNoNesting(text: string, startIdx: number): boolean {
  let depth = 1
  let inStr = false
  for (let i = startIdx; i < text.length; i++) {
    if (text[i] === '"') { inStr = !inStr; continue }
    if (inStr) continue
    if (text[i] === '[') return false  // 有嵌套 [，不是真正的内联列表
    if (text[i] === ']') {
      depth--
      if (depth === 0) return true
    }
  }
  return false
}

/* 查找匹配的右方括号位置 */
function findCloseBracket(text: string, startIdx: number): number {
  let depth = 1
  let inStr = false
  for (let i = startIdx; i < text.length; i++) {
    if (text[i] === '"') { inStr = !inStr; continue }
    if (inStr) continue
    if (text[i] === '[') depth++
    else if (text[i] === ']') {
      depth--
      if (depth === 0) return i
    }
  }
  return text.length - 1
}

/* 值部分高亮（JSON/YAML/STML模式）
 * 
 * 颜色规则（per 颜色纠正.md）：
 * - 花括号 {} → 橙色 #ff993f
 * - 方括号 [] → 黄色 #f7ec24
 * - null → 橙色 #cc5800
 * - 字符串 → 蓝色 #72bbf7
 * - 键名 → 绿色 #87d43a
 * - 注释 # → 灰色 #8B949E
 * - 列表项 - → 白色 #ffffff
 * - 分隔符 --- → 白色 #ffffff
 * 
 * 特殊规则：
 * - 内联字典 {"键": "值"} → 整体蓝色
 * - 多行文本内容 → 蓝色
 * - 嵌套列表 ["外层", ["内层"]] → 仅最外层方括号黄色
 * - 列表项中的括号 - ["元素"] → 全部黄色
 */
function highlightValue(
  text: string,
  lang: string,
  isStml: boolean,
  initialBraceDepth: number = 0,
  allBracketsYellow: boolean = false,  // req 14: list item mode
  forceLastBraceOrange: boolean = false
): string {
  let result = ''
  let i = 0
  let braceDepth = initialBraceDepth  // 跟踪花括号深度
  let bracketDepth = 0                // 跟踪方括号深度
  
  while (i < text.length) {
    // 花括号 {} — 移到最前面处理
    if (text[i] === '{') {
      // 内联字典检测：同一行内有匹配的 }，且中间无嵌套 {
      if (braceDepth === 0 && hasInlineCloseBraceNoNesting(text, i + 1)) {
        const closeIdx = findCloseBrace(text, i + 1)
        result += `<span class="s-s">${escHtml(text.slice(i, closeIdx + 1))}</span>`
        i = closeIdx + 1
        continue
      }
      
      braceDepth++
      if (braceDepth === 1) {
        // 最外层花括号 → 橙色
        result += `<span class="s-b">{</span>`
      } else {
        // 内部花括号 → 蓝色
        result += `<span class="s-s">{</span>`
      }
      i++
      continue
    }
    
    if (text[i] === '}') {
      if (braceDepth > 0) braceDepth--

      const isLastNonSpace = text.slice(i + 1).trim() === ''
      if (braceDepth === 0 || (forceLastBraceOrange && isLastNonSpace && initialBraceDepth > 0)) {
        // 最外层花括号 → 橙色（或被强制设为橙色的多行文本结束符）
        result += `<span class="s-b">}</span>`
      } else {
        // 内部花括号 → 蓝色
        result += `<span class="s-s">}</span>`
      }
      i++
      continue
    }

    // 如果在花括号内部（多行文本），内容作为蓝色处理
    if (braceDepth > 0) {
      if (text[i] === '#') {
        result += `<span class="s-c">${escHtml(text.slice(i))}</span>`
        break
      }
      const textMatch = text.slice(i).match(/^[^{}#]+/)
      if (textMatch) {
        result += `<span class="s-s">${escHtml(textMatch[0])}</span>`
        i += textMatch[0].length
        continue
      }
      result += `<span class="s-s">${escHtml(text[i])}</span>`
      i++
      continue
    }

    // 省略号 ... → 白色
    if (text.slice(i, i + 3) === '...') {
      result += `<span class="s-i">...</span>`
      i += 3
      continue
    }

    // 字符串 "..." → 整体蓝色
    if (text[i] === '"') {
      const endQuote = text.indexOf('"', i + 1)
      if (endQuote !== -1) {
        result += `<span class="s-s">${escHtml(text.slice(i, endQuote + 1))}</span>`
        i = endQuote + 1
        continue
      }
    }

    // 注释 # → 灰色，不使用斜体 (req 10, 11)
    if (text[i] === '#') {
      result += `<span class="s-c">${escHtml(text.slice(i))}</span>`
      break
    }

    // 分隔符 ---
    if (text.slice(i, i + 3) === '---') {
      result += `<span class="s-i">---</span>`
      i += 3
      continue
    }

    // 列表项 - (行首)
    if (text[i] === '-' && (i + 1 >= text.length || text[i + 1] === ' ')) {
      result += `<span class="s-i">- </span>`
      i += 2
      continue
    }

    // null → 红色 #cc5800 (颜色纠正.md 第2项)
    if (text.slice(i).match(/^null\b/)) {
      result += `<span class="s-null">null</span>`
      i += 4
      continue
    }

    // 数字
    const numMatch = text.slice(i).match(/^\d+\.?\d*/)
    if (numMatch) {
      result += `<span class="s-n">${numMatch[0]}</span>`
      i += numMatch[0].length
      continue
    }

    // 方括号处理
    if (text[i] === '[') {
      if (allBracketsYellow) {
        result += `<span class="s-bracket">[</span>`
      } else if (bracketDepth === 0) {
        // 最外层方括号 → 黄色
        result += `<span class="s-bracket">[</span>`
      } else {
        // 内层方括号 → 蓝色
        result += `<span class="s-s">[</span>`
      }
      bracketDepth++
      i++
      continue
    }

    if (text[i] === ']') {
      bracketDepth = Math.max(0, bracketDepth - 1)
      if (allBracketsYellow) {
        result += `<span class="s-bracket">]</span>`
      } else if (bracketDepth === 0) {
        // 最外层方括号 → 黄色
        result += `<span class="s-bracket">]</span>`
      } else {
        // 内层方括号 → 蓝色
        result += `<span class="s-s">]</span>`
      }
      i++
      continue
    }

    // 逗号
    if (text[i] === ',') {
      result += `<span class="s-p">,</span>`
      i++
      continue
    }

    // 普通文本（非空白）→ 蓝色
    const textMatch = text.slice(i).match(/^[^"{}\[\],:#\d\s-]+/)
    if (textMatch) {
      result += `<span class="s-s">${escHtml(textMatch[0])}</span>`
      i += textMatch[0].length
      continue
    }

    // 空白字符 → 蓝色
    if (text[i] === ' ' || text[i] === '\t') {
      result += `<span class="s-s">${escHtml(text[i])}</span>`
      i++
      continue
    }

    // fallback → 蓝色
    result += `<span class="s-s">${escHtml(text[i])}</span>`
    i++
  }
  return result
}

/* 检查是否在同一行内存在匹配的右花括号，且中间无嵌套 { （用于检测真正的"内联字典"） */
function hasInlineCloseBraceNoNesting(text: string, startIdx: number): boolean {
  let depth = 1
  let inStr = false
  for (let i = startIdx; i < text.length; i++) {
    if (text[i] === '"') { inStr = !inStr; continue }
    if (inStr) continue
    if (text[i] === '{') return false  // 有嵌套 {，不是真正的内联字典
    if (text[i] === '}') {
      depth--
      if (depth === 0) return true
    }
  }
  return false
}

/* 查找匹配的右花括号位置 */
function findCloseBrace(text: string, startIdx: number): number {
  let depth = 1
  let inStr = false
  for (let i = startIdx; i < text.length; i++) {
    if (text[i] === '"') { inStr = !inStr; continue }
    if (inStr) continue
    if (text[i] === '{') depth++
    else if (text[i] === '}') {
      depth--
      if (depth === 0) return i
    }
  }
  return text.length - 1
}

function escHtml(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}

export default function CodeBlock({ code, lang = 'stml', stml = false, style, errLine, errMsg, showLineNumbers = false, showPointer = false, noHeader = false, forceLastBraceOrange = false, variant }: CodeBlockProps) {
  const label = lang.toUpperCase()
  const lines = code.split('\n')
  const hasPointer = showPointer && errLine !== undefined

  // 计算每行的花括号深度（用于STML多行文本高亮）
  const isStmlMode = stml || lang === 'stml'
  const lineBraceDepths: number[] = []
  if (isStmlMode) {
    let depth = 0
    for (const line of lines) {
      lineBraceDepths.push(depth)
      depth += braceDelta(line)
      if (depth < 0) depth = 0
    }
  }

  return (
    <div style={style}>
      <div className={`cb-wrap${hasPointer ? ' cb-pointer-wrap' : ''}${variant ? ' cb-' + variant : ''}`}>
        <div className="cb">
          {!noHeader && (
            <div className="cb-hd">
              <span className="cb-lang">{label}</span>
            </div>
          )}
          <div className="cb-bd">
            {lines.map((line, i) => {
              const lineNum = i + 1
              const isErr = errLine !== undefined && lineNum === errLine
              const braceDepth = isStmlMode ? lineBraceDepths[i] : 0
              const isLastLine = i === lines.length - 1
              const h = highlightLine(line, lang, stml, braceDepth, forceLastBraceOrange && isLastLine)
              return (
                <div key={i}>
                  <div className={`cb-ln${isErr ? ' cb-ln-err' : ''}`}>
                    {showLineNumbers && <span className="cb-ln-num">{lineNum}</span>}
                    <span className="cb-c" dangerouslySetInnerHTML={{ __html: h }} />
                  </div>
                  {isErr && errMsg && (
                    <div className="cb-err-inline">{errMsg}</div>
                  )}
                </div>
              )
            })}
          </div>
        </div>
        {hasPointer && (
          <span className="cb-pointer" style={{ top: `calc(33px + 16px + ${(errLine! - 1) * 22.1}px)` }}>
            👈
          </span>
        )}
      </div>
    </div>
  )
}

/* 导出供 Cases 页面使用 */
export { highlightLine, highlightValue, escHtml, braceDelta }
