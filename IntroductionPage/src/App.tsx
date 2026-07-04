import React, { useState, useEffect, useCallback, useRef } from 'react'
import { R, RZ } from './hooks/useScrollReveal'
import CodeBlock, { highlightLine, braceDelta } from './components/CodeBlock'
import ASTTree, { type ASTNode } from './components/ASTTree'
import { parseSTML } from './stml-browser'

/* ═══ 章节定义 ═══ */
const SECTIONS = [
  { id: 'hero', label: '首页' },
  { id: 'demo', label: 'STML' },
  { id: 'json', label: 'JSON' },
  { id: 'yaml', label: 'YAML' },
  { id: 'types', label: '类型' },
  { id: 'compat', label: '兼容' },
  { id: 'scenario', label: '场景' },
  { id: 'gen-test', label: '测试' },
  { id: 'limitations', label: '限制' },
  { id: 'cases', label: '案例' },
]

/* ═══ 主题/语言 Hook ═══ */
function useTheme() {
  const [theme, setTheme] = useState<'dark' | 'light'>(() => {
    try { const s = localStorage.getItem('stml-theme'); if (s === 'light' || s === 'dark') return s; return 'dark' } catch { return 'dark' }
  })
  useEffect(() => { document.documentElement.setAttribute('data-theme', theme); try { localStorage.setItem('stml-theme', theme) } catch { } }, [theme])
  return { theme, toggle: useCallback(() => setTheme(t => t === 'dark' ? 'light' : 'dark'), []) }
}
function useLang() {
  const [lang, setLang] = useState<'cn' | 'en'>(() => {
    try { const s = localStorage.getItem('stml-lang'); if (s === 'cn' || s === 'en') return s; return navigator.language.startsWith('en') ? 'en' : 'cn' } catch { return 'cn' }
  })
  useEffect(() => { try { localStorage.setItem('stml-lang', lang) } catch { } }, [lang])
  return { lang, toggle: useCallback(() => setLang(l => l === 'cn' ? 'en' : 'cn'), []) }
}

/* ═══ TopBar ═══ */
function TopBar({ onToggleLang, onToggleTheme, lang, theme }: { onToggleLang: () => void; onToggleTheme: () => void; lang: string; theme: string }) {
  return (
    <div className="topbar">
      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <button className="top-btn" onClick={onToggleLang}>{lang === 'cn' ? 'EN' : '中文'}</button>
        <button className="top-btn" onClick={onToggleTheme} aria-label="切换主题">
          {theme === 'dark' ? '☀' : '🌙'}
        </button>
      </div>
    </div>
  )
}

/* ═══ SideNav ═══ */
function SideNav({ active }: { active: string }) {
  const go = (id: string) => document.getElementById(id)?.scrollIntoView({ behavior: 'smooth', block: 'start' })
  return (
    <nav className="side-nav">
      {SECTIONS.map(s => <button key={s.id} className={'side-dot' + (active === s.id ? ' active' : '')} onClick={() => go(s.id)} aria-label={s.label} />)}
    </nav>
  )
}

/* ═══ GitHub Icon ═══ */
function GitHubIcon() {
  return (
    <svg width="16" height="16" fill="currentColor" viewBox="0 0 16 16"><path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82a7.62 7.62 0 014 0c1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.012 8.012 0 0016 8c0-4.42-3.58-8-8-8z" /></svg>
  )
}

/* ═══ Bot Icon ═══ */
function BotIcon() {
  return (
    <svg width="28" height="28" viewBox="0 0 28 28" fill="none">
      <rect x="4" y="8" width="20" height="14" rx="3" stroke="var(--fg)" strokeWidth="1.5" />
      <circle cx="10" cy="15" r="2" fill="var(--accent)" />
      <circle cx="18" cy="15" r="2" fill="var(--accent)" />
      <line x1="14" y1="4" x2="14" y2="8" stroke="var(--fg)" strokeWidth="1.5" />
      <circle cx="14" cy="3" r="1.5" fill="var(--fg)" />
    </svg>
  )
}

/* ═══ Mini Bot Icon (for cases page) ═══ */
function MiniBotIcon() {
  return (
    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
      <rect x="2" y="5" width="16" height="11" rx="2" stroke="var(--fg)" strokeWidth="1.2" />
      <circle cx="7" cy="10.5" r="1.5" fill="var(--accent)" />
      <circle cx="13" cy="10.5" r="1.5" fill="var(--accent)" />
      <line x1="10" y1="2" x2="10" y2="5" stroke="var(--fg)" strokeWidth="1.2" />
      <circle cx="10" cy="1.5" r="1.2" fill="var(--fg)" />
    </svg>
  )
}

/* 渲染带语法高亮的STML代码（供Cases页面使用） */
function renderColoredCode(code: string): string {
  const lines = code.split('\n')
  const depths: number[] = []
  let depth = 0
  for (const line of lines) {
    depths.push(depth)
    depth += braceDelta(line)
    if (depth < 0) depth = 0
  }
  return lines.map((line, i) => highlightLine(line, 'stml', true, depths[i])).join('\n')
}

/* ================================================================
   主应用
   ================================================================ */
export default function App() {
  const { lang, toggle: toggleLang } = useLang()
  const { theme, toggle: toggleTheme } = useTheme()
  const [activeSection, setActiveSection] = useState('hero')

  useEffect(() => {
    const obs = SECTIONS.map(s => {
      const el = document.getElementById(s.id); if (!el) return null;
      const o = new IntersectionObserver(([e]) => { if (e.isIntersecting) setActiveSection(s.id) }, { threshold: 0.15, rootMargin: '-10% 0px -40% 0px' })
      o.observe(el); return o
    })
    return () => obs.forEach(o => o?.disconnect())
  }, [])

  return (
    <div>
      <TopBar onToggleLang={toggleLang} onToggleTheme={toggleTheme} lang={lang} theme={theme} />
      <SideNav active={activeSection} />
      <Hero lang={lang} />
      <STMLDemo lang={lang} />
      <CompareJSON lang={lang} />
      <CompareYAML lang={lang} />
      <TypesFeatures lang={lang} />
      <YAMLCompat lang={lang} />
      <ScenarioSuggestions lang={lang} />
      <GenerationTest lang={lang} />
      <ConversionTest lang={lang} />
      <Limitations lang={lang} />
      <Cases lang={lang} />
      <CTA lang={lang} />
      <Footer lang={lang} />
    </div>
  )
}

/* ================================================================
   P1 Hero
   ================================================================ */
interface MousePos { x: number; y: number }

function Hero({ lang }: { lang: 'cn' | 'en' }) {
  const [mousePos, setMousePos] = useState<MousePos | null>(null)
  const hRef = useRef<HTMLDivElement>(null)

  const desc = lang === 'cn'
    ? <>依靠<span className="kw">换行</span>、<span className="kw">状态</span>、<span className="kw">相对缩进</span>进行识别</>
    : <>Identified by <span className="kw">line breaks</span>, <span className="kw">state</span>, and <span className="kw">relative indentation</span></>

  const features = lang === 'cn'
    ? ['支持流式解析', '适用于LLM', '有限兼容YAML']
    : ['Stream Parse', 'LLM-Friendly', 'YAML Compatible']

  const onMouseMove = useCallback((e: React.MouseEvent) => {
    if (!hRef.current) return
    const r = hRef.current.getBoundingClientRect()
    setMousePos({ x: e.clientX - r.left, y: e.clientY - r.top })
  }, [])
  const onMouseLeave = useCallback(() => setMousePos(null), [])

  const heroCode = lang === 'cn'
    ? `"STML":
  "语言特性": ["容错", "易读"]
  "支持的类型":
    - "行内列表"
    - "行内字符串"
    - "多行列表"
    - "多行字符串"
    - "null"
    - "字典"
  "特别之处": "依靠行、状态和相对缩进进行判别"
  "应用场景": {
    适合人类阅读、适合大语言模型生成;
    也适合作为一些场景的数据交换格式
  }
---
"针对YAML": "对于常见的格式进行了兼容"`
    : `"STML":
  "features": ["fault-tolerant", "readable"]
  "supported types":
    - "inline list"
    - "inline string"
    - "multiline list"
    - "multiline string"
    - "null"
    - "dict"
  "special": "identified by line, state, relative indent"
  "use cases": {
    human-readable, LLM-generation-friendly;
    also suitable as data exchange format
  }
---
"yaml compat": "compatible with common YAML formats"`

  return (
    <section className="hero" id="hero">
      <div className="wrap">
        <div className="hero-main">
          <div className="hero-left">
            <R>
              <h1 className="hero-bilingual" ref={hRef}
                onMouseMove={onMouseMove} onMouseLeave={onMouseLeave}
                aria-label={lang === 'cn' ? 'STML 容错的结构语言' : 'Stable Tree Markup Language'}>
                {/* placeholder: 始终使用英文标题撑开容器，确保两种语言都能完整显示 */}
                <span className="hb-placeholder" aria-hidden="true">
                  Stable Tree Markup Language
                </span>
                {/* back层：另一种语言，accent色，默认被front完全遮盖 */}
                <span className="hb-back">{lang === 'cn' ? 'Stable Tree Markup Language' : 'STML 容错的结构语言'}</span>
                {/* front层：当前语言，默认完全可见；hover时圆形镂空露出back */}
                <span className="hb-front" style={
                  mousePos ? { '--mx': mousePos.x + 'px', '--my': mousePos.y + 'px' } as React.CSSProperties : undefined
                }>
                  {lang === 'cn' ? 'STML 容错的结构语言' : 'Stable Tree Markup Language'}
                </span>
              </h1>
            </R>

            <R style={{ transitionDelay: '80ms' }}><p className="hero-desc" style={{ marginTop: 32 }}>{desc}</p></R>

            <R style={{ transitionDelay: '160ms' }}><div className="hero-pills">{features.map(f => <span key={f} className="pill">{f}</span>)}</div></R>

            <R style={{ transitionDelay: '240ms' }}>
              <div className="lang-row">
                <span className="lang-label">{lang === 'cn' ? '现已支持' : 'Supported'}</span>
                <div className="lang-chips">
                  <span className="lang-chip">C++</span>
                  <span className="lang-chip">
                    {/* Python 官方 logo */}
                    <svg width="20" height="20" viewBox="0 0 110 110" fill="none">
                      <path d="M54.91 0C26.47 0 28.37 12.32 28.37 12.32l.03 12.72h26.97v3.82H17.11S0 26.74 0 55.01s14.82 27.59 14.82 27.59h8.86V69.19s-.48-14.82 14.51-14.82h24.99s13.87.22 13.87-13.5V18.28S79.45 0 54.91 0zm-14.46 8.68c2.51 0 4.54 2.03 4.54 4.54s-2.03 4.54-4.54 4.54-4.54-2.03-4.54-4.54 2.03-4.54 4.54-4.54z" fill="#3776ab"/>
                      <path d="M55.09 110c28.44 0 26.54-12.32 26.54-12.32l-.03-12.72H54.63v-3.82h38.26S110 83.26 110 54.99s-14.82-27.59-14.82-27.59h-8.86v13.41s.48 14.82-14.51 14.82H46.82s-13.87-.22-13.87 13.5v22.59S30.55 110 55.09 110zm14.46-8.68c-2.51 0-4.54-2.03-4.54-4.54s2.03-4.54 4.54-4.54 4.54 2.03 4.54 4.54-2.03 4.54-4.54 4.54z" fill="#ffd43b"/>
                    </svg>
                    Python
                  </span>
                  <span className="lang-chip">
                    {/* Java 官方 logo */}
                    <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
                      <path d="M8.851 18.56s-.917.534.653.714c1.902.218 2.874.187 4.969-.211 0 0 .552.346 1.321.646-4.699 2.013-10.594-.118-6.943-1.149M8.276 15.933s-1.028.761.542.924c2.032.209 3.636.227 6.413-.308 0 0 .384.389.987.602-5.679 1.661-12.007.13-7.942-1.218M13.116 14.011c1.158 1.332-.304 2.533-.304 2.533s2.939-1.518 1.589-3.418c-1.261-1.772-2.228-2.652 3.007-5.688 0-.001-8.216 2.051-4.292 6.573" fill="#E76F00"/>
                      <path d="M17.925 7.242s2.484 2.484-2.358 6.306c-3.868 3.055-.881 4.802-.001 6.792-2.274-2.053-3.943-3.868-2.825-5.539 1.644-2.461 6.196-3.655 5.184-7.559M16.497 0s1.687 1.687-1.603 4.292c-2.606 2.072-.589 3.259-.001 4.878-1.818-1.643-3.152-3.095-2.259-4.432 1.315-1.969 4.956-2.924 3.863-4.738" fill="#E76F00"/>
                    </svg>
                    Java
                  </span>
                </div>
                <span className="lang-label" style={{ marginLeft: 24 }}>{lang === 'cn' ? '计划中' : 'Planned'}</span>
                <div className="lang-chips">
                  <span className="lang-chip planned">
                    <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
                      <path d="M10 2l7 4v8l-7 4-7-4V6l7-4z" stroke="#f7df1e" strokeWidth="1.5"/>
                      <text x="10" y="13" textAnchor="middle" fill="#f7df1e" fontSize="7" fontWeight="bold">JS</text>
                    </svg>
                    JS
                  </span>
                </div>
              </div>
            </R>
          </div>

          <RZ style={{ flex: '0 0 auto' }}>
            <div className="hero-code-wrap">
              <CodeBlock code={heroCode} lang="stml" stml showLineNumbers variant="hero" />
            </div>
          </RZ>
        </div>

        <R style={{ transitionDelay: '320ms' }}>
          <div className="hero-links">
            <a href="#" className="hero-link"><GitHubIcon /> Github</a>
            <a href="#" className="hero-link">{lang === 'cn' ? '规范文档 >' : 'Specification >'}</a>
          </div>
        </R>
      </div>
    </section>
  )
}

/* ================================================================
   P2 STML Demo — 代码框 + AST树 + 4个tab
   ================================================================ */

/* AST 树数据（通过 STML 解析器生成） */

const _codeSnippets: Record<number, string> = {
  0: `"STML": "Stable Tree Markup Language"
"特色":
  - "解析确定性高"
  - "容错性强"
  - "输出严格"
---
"映射":
  "键": "值"
"字符串": "文本"
"空值": null
"内联列表": ["a", "b"]
# 注释
"多行文本": {
  内容
}`,
  1: `"多行文本":
  "缩进情况": {
    内容
  }
  "键": "值"`,
  2: `"多行文本":
  "缩进情况": {
    内容
  }
  "键": 值值"
  列表: ["元素1","元素2"]`,
  3: `"编辑":
  "键1": "值1"
  "键2": "值2"`,
}

const _precomputedAsts: Record<number, ASTNode> = {}
for (const k of [0, 1, 2, 3]) {
  _precomputedAsts[k] = parseSTML(_codeSnippets[k]).ast
}


function STMLDemo({ lang }: { lang: 'cn' | 'en' }) {
  const tabs = lang === 'cn' ? ['缩进变化', '多行文本', '符号缺失', '编辑'] : ['Indent', 'Multiline', 'Missing Symbol', 'Edit']
  const editLineNumRef = useRef<HTMLDivElement>(null)
  const textareaRef = useRef<HTMLTextAreaElement>(null)
  const measureRef = useRef<HTMLDivElement>(null)
  const [lineHeights, setLineHeights] = useState<number[]>([])

  const codes: Record<number, string> = {
    0: `"STML": "Stable Tree Markup Language"
"特色":
  - "解析确定性高"
  - "容错性强"
  - "输出严格"
---
"映射":
  "键": "值"
"字符串": "文本"
"空值": null
"内联列表": ["a", "b"]
# 注释
"多行文本": {
  内容
}`,
    1: `"多行文本":
  "缩进情况": {
    内容
  }
  "键": "值"`,
    2: `"多行文本":
  "缩进情况": {
    内容
  }
  "键": 值值"
  列表: ["元素1","元素2"]`,
    3: `"编辑":
  "键1": "值1"
  "键2": "值2"`,
  }

  const asts = _precomputedAsts

  const [tab, setTab] = useState(0)
  const [editCode, setEditCode] = useState(codes[3])
  const [editAst, setEditAst] = useState<ASTNode>(() => _precomputedAsts[3])
  const debounceRef = useRef<ReturnType<typeof setTimeout> | null>(null)

  const handleEditChange = (val: string) => {
    setEditCode(val)
    if (debounceRef.current) clearTimeout(debounceRef.current)
    debounceRef.current = setTimeout(() => {
      const { ast, warnings } = parseSTML(val)
      if (warnings.length) console.log('STML warnings:', warnings)
      setEditAst(ast)
    }, 300)
  }

  // 测量每个逻辑行的视觉高度（处理自动换行）
  useEffect(() => {
    if (tab !== 3 || !measureRef.current || !textareaRef.current) return
    const mEl = measureRef.current
    const ta = textareaRef.current
    const taStyle = getComputedStyle(ta)
    mEl.style.font = taStyle.font
    mEl.style.lineHeight = taStyle.lineHeight
    mEl.style.letterSpacing = taStyle.letterSpacing
    mEl.style.whiteSpace = 'pre-wrap'
    mEl.style.wordWrap = 'break-word'
    // 计算可用宽度（textarea内容区宽度）
    const taPaddingLeft = parseFloat(taStyle.paddingLeft)
    const taPaddingRight = parseFloat(taStyle.paddingRight)
    const taBorderLeft = parseFloat(taStyle.borderLeftWidth)
    const taBorderRight = parseFloat(taStyle.borderRightWidth)
    const contentWidth = ta.clientWidth - taPaddingLeft - taPaddingRight - taBorderLeft - taBorderRight
    mEl.style.width = contentWidth + 'px'

    const lines = editCode.split('\n')
    const heights: number[] = []
    const singleLineH = parseFloat(taStyle.lineHeight)
    for (const line of lines) {
      // 空行仍占一行高度
      mEl.textContent = line || ' '
      const h = mEl.offsetHeight
      heights.push(Math.max(h, singleLineH))
    }
    mEl.textContent = ''
    setLineHeights(heights)
  }, [editCode, tab])

  const currentCode = tab === 3 ? editCode : codes[tab]
  const currentAst = tab === 3 ? editAst : asts[tab]

  return (
    <section className="sec" id="demo">
      <div className="wrap">
        <h2 className="sec-title">{lang === 'cn' ? '示例' : 'Demo'}</h2>
        <div className="demo-grid">
          <div style={{ display: 'flex', flexDirection: 'column', minWidth: 0 }}>
            <R>
              <div className="tab-content-fade" key={tab}>
              {tab === 3 ? (
                <div className="cb cb-demo">
                  <div className="cb-hd">
                    <span className="cb-lang">STML</span>
                  </div>
                  <div className="cb-bd" style={{ padding: 0 }}>
                    <div className="edit-wrap">
                      <div className="edit-line-numbers" ref={editLineNumRef}>
                        {editCode.split('\n').map((line, i) => (
                          <div key={i} style={{ height: lineHeights[i] || undefined }}>{i + 1}</div>
                        ))}
                      </div>
                      <textarea
                        ref={textareaRef}
                        className="edit-textarea"
                        value={editCode}
                        onChange={e => handleEditChange(e.target.value)}
                        onScroll={e => {
                          if (editLineNumRef.current) editLineNumRef.current.scrollTop = (e.target as HTMLTextAreaElement).scrollTop
                        }}
                        spellCheck={false}
                        placeholder={lang === 'cn' ? '在此输入 STML 文本...' : 'Type STML here...'}
                      />
                    </div>
                    {/* 隐藏测量元素，与 textarea 相同字体/宽度/内边距 */}
                    <div ref={measureRef} className="edit-measure" aria-hidden="true" />
                  </div>
                </div>
              ) : (
                <CodeBlock code={currentCode} lang="stml" stml showLineNumbers variant="demo" />
              )}
              </div>
            </R>
            <R style={{ marginTop: 16 }}>
              <div className="demo-tabs">
                {tabs.map((t, i) => <button key={i} className={'demo-tab' + (tab === i ? ' active' : '')} onClick={() => setTab(i)}>{t}</button>)}
              </div>
            </R>
          </div>
          <RZ style={{ transitionDelay: '100ms', display: 'flex', flexDirection: 'column' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 16 }}>
              <span style={{ fontSize: 36, flexShrink: 0 }}>👉</span>
              <div style={{ flex: 1, overflow: 'auto' }}>
                <ASTTree key={JSON.stringify(currentAst)} data={currentAst} />
              </div>
            </div>
          </RZ>
        </div>
      </div>
    </section>
  )
}

/* ================================================================
   P3 Compare JSON — 左侧代码框 + 右侧7个错误按钮
   ================================================================ */
function CompareJSON({ lang }: { lang: 'cn' | 'en' }) {
  const baseCode = lang === 'cn' ? `{
  "JSON": {
    "数据格式": "必须使用键值对表示数据，键名必须用双引号括起来。",
    "字符串": "字符串值必须用双引号括起来，不能使用单引号。",
    "数值": "数值可以是整数或浮点数，不需要引号。",
    "布尔值": "布尔值必须为小写的 true 或 false。",
    "空值": "空值必须使用 null（小写）。",
    "数组": "数组使用方括号 []，元素之间用逗号分隔。",
    "对象": "对象使用花括号 {}，包含多个键值对，键值对之间用逗号分隔。",
    "嵌套结构": "对象和数组可以互相嵌套，形成复杂结构。",
    "无注释": "JSON 标准格式不支持注释。",
    "无尾随逗号": "最后一个键值对或数组元素后不能有逗号。",
    "编码": "必须使用 UTF-8 编码。",
    "唯一键名": "同一对象内的键名应保持唯一（重复键名可能导致覆盖）。"
  }
}` : `{
  "JSON": {
    "format": "Must use key-value pairs, keys must be double-quoted.",
    "string": "String values must use double quotes, not single quotes.",
    "number": "Numbers can be integers or floats, no quotes needed.",
    "boolean": "Booleans must be lowercase true or false.",
    "null": "Null must be lowercase null.",
    "array": "Arrays use square brackets [], elements separated by commas.",
    "object": "Objects use curly braces {}, containing key-value pairs separated by commas.",
    "nesting": "Objects and arrays can be nested to form complex structures.",
    "no comments": "JSON standard does not support comments.",
    "no trailing comma": "No trailing comma after last key-value pair or array element.",
    "encoding": "Must use UTF-8 encoding.",
    "unique keys": "Keys within the same object should be unique."
  }
}`

  const errors = lang === 'cn' ? [
    { label: '引号未闭合/未使用引号', code: `{
  "JSON": {
    "数据格式": "必须使用键值对表示数据，键名必须用双引号括起来。",
    "字符串": "字符串值必须用双引号括起来，不能使用单引号。,
    "数值": "数值可以是整数或浮点数，不需要引号。",
    "布尔值": "布尔值必须为小写的 true 或 false。",
    "空值": "空值必须使用 null（小写）。",
    "数组": "数组使用方括号 []，元素之间用逗号分隔。",
    "对象": "对象使用花括号 {}，包含多个键值对，键值对之间用逗号分隔。",
    "嵌套结构": "对象和数组可以互相嵌套，形成复杂结构。",
    "无注释": "JSON 标准格式不支持注释。",
    "无尾随逗号": "最后一个键值对或数组元素后不能有逗号。",
    "编码": "必须使用 UTF-8 编码。",
    "唯一键名": "同一对象内的键名应保持唯一（重复键名可能导致覆盖）。"
  }
}`, errLine: 4, errMsg: 'json.decoder.JSONDecodeError: Invalid control character' },
    { label: '逗号缺失', code: `{
  "JSON": {
    "数据格式": "必须使用键值对表示数据，键名必须用双引号括起来。",
    "字符串": "字符串值必须用双引号括起来，不能使用单引号。"
    "数值": "数值可以是整数或浮点数，不需要引号。",
    "布尔值": "布尔值必须为小写的 true 或 false。",
    "空值": "空值必须使用 null（小写）。",
    "数组": "数组使用方括号 []，元素之间用逗号分隔。",
    "对象": "对象使用花括号 {}，包含多个键值对，键值对之间用逗号分隔。",
    "嵌套结构": "对象和数组可以互相嵌套，形成复杂结构。",
    "无注释": "JSON 标准格式不支持注释。",
    "无尾随逗号": "最后一个键值对或数组元素后不能有逗号。",
    "编码": "必须使用 UTF-8 编码。",
    "唯一键名": "同一对象内的键名应保持唯一（重复键名可能导致覆盖）。"
  }
}`, errLine: 5, errMsg: "json.decoder.JSONDecodeError: Expecting ',' delimiter" },
    { label: '括号不匹配', code: `{
  "JSON": {
    "数据格式": "必须使用键值对表示数据，键名必须用双引号括起来。",
    "字符串": "字符串值必须用双引号括起来，不能使用单引号。",
    "数值": "数值可以是整数或浮点数，不需要引号。",
    "布尔值": "布尔值必须为小写的 true 或 false。",
    "空值": "空值必须使用 null（小写）。",
    "数组": "数组使用方括号 []，元素之间用逗号分隔。",
    "对象": "对象使用花括号 {}，包含多个键值对，键值对之间用逗号分隔。",
    "嵌套结构": "对象和数组可以互相嵌套，形成复杂结构。",
    "无注释": "JSON 标准格式不支持注释。",
    "无尾随逗号": "最后一个键值对或数组元素后不能有逗号。",
    "编码": "必须使用 UTF-8 编码。",
    "唯一键名": "同一对象内的键名应保持唯一（重复键名可能导致覆盖）。"
  }`, errLine: 15, errMsg: "json.decoder.JSONDecodeError: Expecting ',' delimiter" },
    { label: '使用注释', code: `{
  "JSON": {
    "数据格式": "必须使用键值对表示数据，键名必须用双引号括起来。",
    "字符串": "字符串值必须用双引号括起来，不能使用单引号。",
    "数值": "数值可以是整数或浮点数，不需要引号。",
    "布尔值": "布尔值必须为小写的 true 或 false。",
    "空值": "空值必须使用 null（小写）。",
    "数组": "数组使用方括号 []，元素之间用逗号分隔。",
    "对象": "对象使用花括号 {}，包含多个键值对，键值对之间用逗号分隔。",
    "嵌套结构": "对象和数组可以互相嵌套，形成复杂结构。",
    "无注释": "JSON 标准格式不支持注释。",
    // 试图注释
    "无尾随逗号": "最后一个键值对或数组元素后不能有逗号。",
    "编码": "必须使用 UTF-8 编码。",
    "唯一键名": "同一对象内的键名应保持唯一（重复键名可能导致覆盖）。"
  }
}`, errLine: 12, errMsg: 'json.decoder.JSONDecodeError: Expecting property name enclosed in double quotes' },
    { label: '逗号多余', code: `{
  "JSON": {
    "数据格式": "必须使用键值对表示数据，键名必须用双引号括起来。",
    "字符串": "字符串值必须用双引号括起来，不能使用单引号。",
    "数值": "数值可以是整数或浮点数，不需要引号。",
    "布尔值": "布尔值必须为小写的 true 或 false。",
    "空值": "空值必须使用 null（小写）。",
    "数组": "数组使用方括号 []，元素之间用逗号分隔。",
    "对象": "对象使用花括号 {}，包含多个键值对，键值对之间用逗号分隔。",
    "嵌套结构": "对象和数组可以互相嵌套，形成复杂结构。",
    "无注释": "JSON 标准格式不支持注释。",
    "无尾随逗号": "最后一个键值对或数组元素后不能有逗号。",
    "编码": "必须使用 UTF-8 编码。",
    "唯一键名": "同一对象内的键名应保持唯一（重复键名可能导致覆盖）。",
  }
}`, errLine: 15, errMsg: 'json.decoder.JSONDecodeError: Illegal trailing comma before end of object' },
    { label: '未转义错误', code: `{
  "JSON": {
    "数据格式": "必须使用键值对表示数据，键名必须用双引号括起来。",
    "字符串": "字符串值必须用双引号括起来，不能使用单引号。",
    "数值": "数值可以是整数或浮点数，不需要"引号"。",
    "布尔值": "布尔值必须为小写的 true 或 false。",
    "空值": "空值必须使用 null（小写）。",
    "数组": "数组使用方括号 []，元素之间用逗号分隔。",
    "对象": "对象使用花括号 {}，包含多个键值对，键值对之间用逗号分隔。",
    "嵌套结构": "对象和数组可以互相嵌套，形成复杂结构。",
    "无注释": "JSON 标准格式不支持注释。",
    "无尾随逗号": "最后一个键值对或数组元素后不能有逗号。",
    "编码": "必须使用 UTF-8 编码。",
    "唯一键名": "同一对象内的键名应保持唯一（重复键名可能导致覆盖）。"
  }
}`, errLine: 5, errMsg: "json.decoder.JSONDecodeError: Expecting ',' delimiter" },
    { label: '意外换行', code: `{
  "JSON": {
    "数据格式": "必须使用键值对表示数据，
键名必须用双引号括起来。",
    "字符串": "字符串值必须用双引号括起来，不能使用单引号。",
    "数值": "数值可以是整数或浮点数，不需要引号。",
    "布尔值": "布尔值必须为小写的 true 或 false。",
    "空值": "空值必须使用 null（小写）。",
    "数组": "数组使用方括号 []，元素之间用逗号分隔。",
    "对象": "对象使用花括号 {}，包含多个键值对，键值对之间用逗号分隔。",
    "嵌套结构": "对象和数组可以互相嵌套，形成复杂结构。",
    "无注释": "JSON 标准格式不支持注释。",
    "无尾随逗号": "最后一个键值对或数组元素后不能有逗号。",
    "编码": "必须使用 UTF-8 编码。",
    "唯一键名": "同一对象内的键名应保持唯一（重复键名可能导致覆盖）。"
  }
}`, errLine: 3, errMsg: 'json.decoder.JSONDecodeError: Invalid control character' },
  ] : [
    { label: 'Unclosed Quote', code: `{
  "JSON": {
    "format": "Must use key-value pairs, keys must be double-quoted.",
    "string": "String values must use double quotes, not single,
    "number": "Numbers can be integers or floats, no quotes needed.",
    "boolean": "Booleans must be lowercase true or false.",
    "null": "Null must be lowercase null.",
    "array": "Arrays use square brackets [], elements separated by commas.",
    "object": "Objects use curly braces {}, containing key-value pairs separated by commas.",
    "nesting": "Objects and arrays can be nested to form complex structures.",
    "no comments": "JSON standard does not support comments.",
    "no trailing comma": "No trailing comma after last key-value pair or array element.",
    "encoding": "Must use UTF-8 encoding.",
    "unique keys": "Keys within the same object should be unique."
  }
}`, errLine: 4, errMsg: 'json.decoder.JSONDecodeError: Invalid control character' },
    { label: 'Missing Comma', code: `{
  "JSON": {
    "format": "Must use key-value pairs, keys must be double-quoted.",
    "string": "String values must use double quotes, not single quotes."
    "number": "Numbers can be integers or floats, no quotes needed.",
    "boolean": "Booleans must be lowercase true or false.",
    "null": "Null must be lowercase null.",
    "array": "Arrays use square brackets [], elements separated by commas.",
    "object": "Objects use curly braces {}, containing key-value pairs separated by commas.",
    "nesting": "Objects and arrays can be nested to form complex structures.",
    "no comments": "JSON standard does not support comments.",
    "no trailing comma": "No trailing comma after last key-value pair or array element.",
    "encoding": "Must use UTF-8 encoding.",
    "unique keys": "Keys within the same object should be unique."
  }
}`, errLine: 5, errMsg: "json.decoder.JSONDecodeError: Expecting ',' delimiter" },
    { label: 'Bracket Mismatch', code: `{
  "JSON": {
    "format": "Must use key-value pairs, keys must be double-quoted.",
    "string": "String values must use double quotes, not single quotes.",
    "number": "Numbers can be integers or floats, no quotes needed.",
    "boolean": "Booleans must be lowercase true or false.",
    "null": "Null must be lowercase null.",
    "array": "Arrays use square brackets [], elements separated by commas.",
    "object": "Objects use curly braces {}, containing key-value pairs separated by commas.",
    "nesting": "Objects and arrays can be nested to form complex structures.",
    "no comments": "JSON standard does not support comments.",
    "no trailing comma": "No trailing comma after last key-value pair or array element.",
    "encoding": "Must use UTF-8 encoding.",
    "unique keys": "Keys within the same object should be unique."
  }`, errLine: 15, errMsg: "json.decoder.JSONDecodeError: Expecting ',' delimiter" },
    { label: 'Using Comments', code: `{
  "JSON": {
    "format": "Must use key-value pairs, keys must be double-quoted.",
    "string": "String values must use double quotes, not single quotes.",
    "number": "Numbers can be integers or floats, no quotes needed.",
    "boolean": "Booleans must be lowercase true or false.",
    "null": "Null must be lowercase null.",
    "array": "Arrays use square brackets [], elements separated by commas.",
    "object": "Objects use curly braces {}, containing key-value pairs separated by commas.",
    "nesting": "Objects and arrays can be nested to form complex structures.",
    "no comments": "JSON standard does not support comments.",
    // trying a comment
    "no trailing comma": "No trailing comma after last key-value pair or array element.",
    "encoding": "Must use UTF-8 encoding.",
    "unique keys": "Keys within the same object should be unique."
  }
}`, errLine: 12, errMsg: 'json.decoder.JSONDecodeError: Expecting property name enclosed in double quotes' },
    { label: 'Trailing Comma', code: `{
  "JSON": {
    "format": "Must use key-value pairs, keys must be double-quoted.",
    "string": "String values must use double quotes, not single quotes.",
    "number": "Numbers can be integers or floats, no quotes needed.",
    "boolean": "Booleans must be lowercase true or false.",
    "null": "Null must be lowercase null.",
    "array": "Arrays use square brackets [], elements separated by commas.",
    "object": "Objects use curly braces {}, containing key-value pairs separated by commas.",
    "nesting": "Objects and arrays can be nested to form complex structures.",
    "no comments": "JSON standard does not support comments.",
    "no trailing comma": "No trailing comma after last key-value pair or array element.",
    "encoding": "Must use UTF-8 encoding.",
    "unique keys": "Keys within the same object should be unique.",
  }
}`, errLine: 15, errMsg: 'json.decoder.JSONDecodeError: Illegal trailing comma before end of object' },
    { label: 'Escape Error', code: `{
  "JSON": {
    "format": "Must use key-value pairs, keys must be double-quoted.",
    "string": "String values must use double quotes, not "quotes".",
    "number": "Numbers can be integers or floats, no quotes needed.",
    "boolean": "Booleans must be lowercase true or false.",
    "null": "Null must be lowercase null.",
    "array": "Arrays use square brackets [], elements separated by commas.",
    "object": "Objects use curly braces {}, containing key-value pairs separated by commas.",
    "nesting": "Objects and arrays can be nested to form complex structures.",
    "no comments": "JSON standard does not support comments.",
    "no trailing comma": "No trailing comma after last key-value pair or array element.",
    "encoding": "Must use UTF-8 encoding.",
    "unique keys": "Keys within the same object should be unique."
  }
}`, errLine: 4, errMsg: "json.decoder.JSONDecodeError: Expecting ',' delimiter" },
    { label: 'Unexpected Newline', code: `{
  "JSON": {
    "format": "Must use key-value pairs,
keys must be double-quoted.",
    "string": "String values must use double quotes, not single quotes.",
    "number": "Numbers can be integers or floats, no quotes needed.",
    "boolean": "Booleans must be lowercase true or false.",
    "null": "Null must be lowercase null.",
    "array": "Arrays use square brackets [], elements separated by commas.",
    "object": "Objects use curly braces {}, containing key-value pairs separated by commas.",
    "nesting": "Objects and arrays can be nested to form complex structures.",
    "no comments": "JSON standard does not support comments.",
    "no trailing comma": "No trailing comma after last key-value pair or array element.",
    "encoding": "Must use UTF-8 encoding.",
    "unique keys": "Keys within the same object should be unique."
  }
}`, errLine: 3, errMsg: 'json.decoder.JSONDecodeError: Invalid control character' },
  ]

  const [activeErr, setActiveErr] = useState(-1)
  const currentCode = activeErr >= 0 ? errors[activeErr].code : baseCode
  const currentErrLine = activeErr >= 0 ? errors[activeErr].errLine : undefined
  const currentErrMsg = activeErr >= 0 ? errors[activeErr].errMsg : undefined

  return (
    <section className="sec" id="json">
      <div className="wrap">
        <h2 className="sec-title">{lang === 'cn' ? '对比JSON' : 'Compare JSON'}</h2>
        <div className="compare-layout">
          <R>
            <div className="tab-content-fade" key={`json-${activeErr}`}>
              <CodeBlock code={currentCode} lang="json" errLine={currentErrLine} errMsg={currentErrMsg} showLineNumbers showPointer variant="compare" />
            </div>
          </R>
          <RZ style={{ transitionDelay: '100ms' }}>
            <div>
              <div className="err-section-label">{lang === 'cn' ? '常见错误' : 'Common Errors'}</div>
              <div className="err-btn-group">
                {errors.map((e, i) => (
                  <button key={i} className={'err-btn' + (activeErr === i ? ' active' : '')} onClick={() => setActiveErr(activeErr === i ? -1 : i)}>
                    {e.label}
                  </button>
                ))}
              </div>
            </div>
          </RZ>
        </div>
        <R style={{ marginTop: 32 }}>
          <p style={{ fontSize: 13, color: 'var(--fg-dim)' }}>
            {lang === 'cn' ? '* 不同解析器解析不同，存在差异，这里只介绍常见的错误' : '* Different parsers behave differently; only common errors shown here'}
          </p>
        </R>
      </div>
    </section>
  )
}

/* ================================================================
   P4 Compare YAML — 左侧代码框 + 右侧6个错误按钮
   ================================================================ */
function CompareYAML({ lang }: { lang: 'cn' | 'en' }) {
  const baseCode = lang === 'cn' ? `缩进: 使用空格缩进，不能用Tab。同一层级缩进必须一致，推荐2个空格。
键值对: 键和值之间用冒号加空格分隔。
字符串: 一般不加引号; 包含特殊字符时需加单引号或双引号。单引号内的双引号无需转义，反之亦然。
多行字符串:
  折叠块: > 将换行转为空格，最后加一个换行。
  字面块: | 保留原有换行。
数值: 整数、浮点数、科学计数法直接写，如 42、3.14、1e10。
布尔值: true、false、yes、no、on、off 均可（大小写敏感）。
空值: null、~ 或不写值（仅写键名）。
列表: 以短横线加空格开头，如 - item1; 也可用行内格式 [item1, item2]。
对象/映射: 键值对集合，可嵌套。
注释: 以 # 开头，只能单行，不能写在行内中间。
特殊字符处理: |
  字符串若以特定字符开头（如 {、[、!、&、*、?、|、>、%、@、\`），需加引号避免被误解为YAML特殊语法。` : `indent: Use spaces, not tabs. Same level must be consistent, 2 spaces recommended.
key-value: Key and value separated by colon + space.
string: Generally unquoted; quote with single or double quotes when containing special characters.
multiline:
  folded: > converts newlines to spaces, adds a final newline.
  literal: | preserves original newlines.
number: Integers, floats, scientific notation written directly, e.g. 42, 3.14, 1e10.
boolean: true, false, yes, no, on, off (case-sensitive).
null: null, ~ or empty value (key only).
list: Dash + space prefix, e.g. - item1; or inline [item1, item2].
object/mapping: Collection of key-value pairs, nestable.
comment: "Starts with #, single line only, not inline."
special chars: |
  Strings starting with special chars ({, [, !, &, *, ?, |, >, %, @, \`) need quotes.`

  const errors = lang === 'cn' ? [
    { label: '混用Tab和空格', code: `缩进: 使用空格缩进，不能用Tab。同一层级缩进必须一致，推荐2个空格。
键值对:\t键和值之间用冒号加空格分隔。
字符串: 一般不加引号; 包含特殊字符时需加单引号或双引号。单引号内的双引号无需转义，反之亦然。
多行字符串:
  折叠块: > 将换行转为空格，最后加一个换行。
  字面块: | 保留原有换行。
数值: 整数、浮点数、科学计数法直接写，如 42、3.14、1e10。
布尔值: true、false、yes、no、on、off 均可（大小写敏感）。
空值: null、~ 或不写值（仅写键名）。
列表: 以短横线加空格开头，如 - item1; 也可用行内格式 [item1, item2]。
对象/映射: 键值对集合，可嵌套。
注释: 以 # 开头，只能单行，不能写在行内中间。
特殊字符处理: |
  字符串若以特定字符开头（如 {、[、!、&、*、?、|、>、%、@、\`），需加引号避免被误解为YAML特殊语法。`, errLine: 2, errMsg: 'yaml.scanner.ScannerError: while scanning for the next token\nfound character \'\\t\' that cannot start any token' },
    { label: '缩进不一致', code: `缩进: 使用空格缩进，不能用Tab。同一层级缩进必须一致，推荐2个空格。
键值对: 键和值之间用冒号加空格分隔。
字符串: 一般不加引号; 包含特殊字符时需加单引号或双引号。单引号内的双引号无需转义，反之亦然。
多行字符串:
   折叠块: > 将换行转为空格，最后加一个换行。
  字面块: | 保留原有换行。
数值: 整数、浮点数、科学计数法直接写，如 42、3.14、1e10。
布尔值: true、false、yes、no、on、off 均可（大小写敏感）。
空值: null、~ 或不写值（仅写键名）。
列表: 以短横线加空格开头，如 - item1; 也可用行内格式 [item1, item2]。
对象/映射: 键值对集合，可嵌套。
注释: 以 # 开头，只能单行，不能写在行内中间。
特殊字符处理: |
  字符串若以特定字符开头（如 {、[、!、&、*、?、|、>、%、@、\`），需加引号避免被误解为YAML特殊语法。`, errLine: 5, errMsg: 'yaml.scanner.ScannerError: mapping values are not allowed here' },
    { label: '特殊字符未加引号', code: `缩进: 使用空格缩进，不能用Tab。同一层级缩进必须一致，推荐2个空格。
键值对: 键和值之间用冒号加空格分隔。
字符串: 一般不加引号; 包含特殊字符时需加单引号或双引号。单引号内的双引号无需转义，反之亦然。
多行字符串:
  折叠块: > 将换行转为空格，最后加一个换行。
  字面块: | 保留原有换行。
数值: 整数、浮点数、科学计数法直接写，如: 42、3.14、1e10。
布尔值: true、false、yes、no、on、off 均可（大小写敏感）。
空值: null、~ 或不写值（仅写键名）。
列表: 以短横线加空格开头，如 - item1; 也可用行内格式 [item1, item2]。
对象/映射: 键值对集合，可嵌套。
注释: 以 # 开头，只能单行，不能写在行内中间。
特殊字符处理: |
  字符串若以特定字符开头（如 {、[、!、&、*、?、|、>、%、@、\`），需加引号避免被误解为YAML特殊语法。`, errLine: 8, errMsg: 'yaml.scanner.ScannerError: mapping values are not allowed here' },
    { label: '引号未闭合', code: `缩进: 使用空格缩进，不能用Tab。同一层级缩进必须一致，推荐2个空格。
键值对: 键和值之间用冒号加空格分隔。
字符串: "一般不加引号; 包含特殊字符时需加单引号或双引号。
多行字符串:
  折叠块: > 将换行转为空格，最后加一个换行。
  字面块: | 保留原有换行。
数值: 整数、浮点数、科学计数法直接写，如 42、3.14、1e10。
布尔值: true、false、yes、no、on、off 均可（大小写敏感）。
空值: null、~ 或不写值（仅写键名）。
列表: 以短横线加空格开头，如 - item1; 也可用行内格式 [item1, item2]。
对象/映射: 键值对集合，可嵌套。
注释: 以 # 开头，只能单行，不能写在行内中间。
特殊字符处理: |
  字符串若以特定字符开头（如 {、[、!、&、*、?、|、>、%、@、\`），需加引号避免被误解为YAML特殊语法。`, errLine: 3, errMsg: 'yaml.scanner.ScannerError: while scanning a quoted scalar' },
    { label: '键值对冒号后没有空格', code: `缩进: 使用空格缩进，不能用Tab。同一层级缩进必须一致，推荐2个空格。
键值对:键和值之间用冒号加空格分隔。
字符串: 一般不加引号; 包含特殊字符时需加单引号或双引号。单引号内的双引号无需转义，反之亦然。
多行字符串:
  折叠块: > 将换行转为空格，最后加一个换行。
  字面块: | 保留原有换行。
数值: 整数、浮点数、科学计数法直接写，如 42、3.14、1e10。
布尔值: true、false、yes、no、on、off 均可（大小写敏感）。
空值: null、~ 或不写值（仅写键名）。
列表: 以短横线加空格开头，如 - item1; 也可用行内格式 [item1, item2]。
对象/映射: 键值对集合，可嵌套。
注释: 以 # 开头，只能单行，不能写在行内中间。
特殊字符处理: |
  字符串若以特定字符开头（如 {、[、!、&、*、?、|、>、%、@、\`），需加引号避免被误解为YAML特殊语法。`, errLine: 2, errMsg: "yaml.scanner.ScannerError: while scanning a simple key could not find expected ':'" },
    { label: '其他错误', code: `缩进: 使用空格缩进，不能用Tab。同一层级缩进必须一致，推荐2个空格。
键值对: 键和值之间用冒号加空格分隔。
字符串: 一般不加引号; 包含特殊字符时需加单引号或双引号。单引号内的双引号无需转义，反之亦然。
多行字符串:
  折叠块: > 将换行转为空格，最后加一个换行。
  字面块: | 保留原有换行。
数值: 整数、浮点数、科学计数法直接写，如 42、3.14、1e10。
布尔值: true、false、yes、no、on、off 均可（大小写敏感）。
空值: null、~ 或不写值（仅写键名）。
列表: 以短横线加空格开头，如 - item1; 也可用行内格式 [item1, item2]。
对象/映射: 键值对集合，可嵌套。
注释: 以 # 开头，只能单行，不能写在行内中间。
特殊字符处理: |
  字符串若以特定字符开头（如 {、[、!、&、*、?、|、>、%、@、\`），需加引号避免被误解为YAML特殊语法。`, errLine: 3, errMsg: 'yaml.scanner.ScannerError: mapping values are not allowed here' },
  ] : [
    { label: 'Mixed Tabs & Spaces', code: `indent: Use spaces, not tabs. Same level must be consistent, 2 spaces recommended.
key-value:\tKey and value separated by colon + space.
string: Generally unquoted; quote with single or double quotes when containing special characters.
multiline:
  folded: > converts newlines to spaces, adds a final newline.
  literal: | preserves original newlines.
number: Integers, floats, scientific notation written directly, e.g. 42, 3.14, 1e10.
boolean: true, false, yes, no, on, off (case-sensitive).
null: null, ~ or empty value (key only).
list: Dash + space prefix, e.g. - item1; or inline [item1, item2].
object/mapping: Collection of key-value pairs, nestable.
comment: "Starts with #, single line only, not inline."
special chars: |
  Strings starting with special chars ({, [, !, &, *, ?, |, >, %, @, \`) need quotes.`, errLine: 2, errMsg: 'yaml.scanner.ScannerError: while scanning for the next token\nfound character \'\\t\' that cannot start any token' },
    { label: 'Inconsistent Indent', code: `indent: Use spaces, not tabs. Same level must be consistent, 2 spaces recommended.
key-value: Key and value separated by colon + space.
string: Generally unquoted; quote with single or double quotes when containing special characters.
multiline:
   folded: > converts newlines to spaces, adds a final newline.
  literal: | preserves original newlines.
number: Integers, floats, scientific notation written directly, e.g. 42, 3.14, 1e10.
boolean: true, false, yes, no, on, off (case-sensitive).
null: null, ~ or empty value (key only).
list: Dash + space prefix, e.g. - item1; or inline [item1, item2].
object/mapping: Collection of key-value pairs, nestable.
comment: "Starts with #, single line only, not inline."
special chars: |
  Strings starting with special chars ({, [, !, &, *, ?, |, >, %, @, \`) need quotes.`, errLine: 5, errMsg: 'yaml.scanner.ScannerError: mapping values are not allowed here' },
    { label: 'Unquoted Special Chars', code: `indent: Use spaces, not tabs. Same level must be consistent, 2 spaces recommended.
key-value: Key and value separated by colon + space.
string: Generally unquoted; quote with single or double quotes when containing special characters.
multiline:
  folded: > converts newlines to spaces, adds a final newline.
  literal: | preserves original newlines.
number: Integers, floats, scientific notation: e.g. 42, 3.14, 1e10.
boolean: true, false, yes, no, on, off (case-sensitive).
null: null, ~ or empty value (key only).
list: Dash + space prefix, e.g. - item1; or inline [item1, item2].
object/mapping: Collection of key-value pairs, nestable.
comment: "Starts with #, single line only, not inline."
special chars: |
  Strings starting with special chars ({, [, !, &, *, ?, |, >, %, @, \`) need quotes.`, errLine: 8, errMsg: 'yaml.scanner.ScannerError: mapping values are not allowed here' },
    { label: 'Unclosed Quote', code: `indent: Use spaces, not tabs. Same level must be consistent, 2 spaces recommended.
key-value: Key and value separated by colon + space.
string: "Generally unquoted; quote when needed.
multiline:
  folded: > converts newlines to spaces, adds a final newline.
  literal: | preserves original newlines.
number: Integers, floats, scientific notation written directly, e.g. 42, 3.14, 1e10.
boolean: true, false, yes, no, on, off (case-sensitive).
null: null, ~ or empty value (key only).
list: Dash + space prefix, e.g. - item1; or inline [item1, item2].
object/mapping: Collection of key-value pairs, nestable.
comment: "Starts with #, single line only, not inline."
special chars: |
  Strings starting with special chars ({, [, !, &, *, ?, |, >, %, @, \`) need quotes.`, errLine: 3, errMsg: 'yaml.scanner.ScannerError: while scanning a quoted scalar' },
    { label: 'No Space After Colon', code: `indent: Use spaces, not tabs. Same level must be consistent, 2 spaces recommended.
key-value:Key and value separated by colon + space.
string: Generally unquoted; quote with single or double quotes when containing special characters.
multiline:
  folded: > converts newlines to spaces, adds a final newline.
  literal: | preserves original newlines.
number: Integers, floats, scientific notation written directly, e.g. 42, 3.14, 1e10.
boolean: true, false, yes, no, on, off (case-sensitive).
null: null, ~ or empty value (key only).
list: Dash + space prefix, e.g. - item1; or inline [item1, item2].
object/mapping: Collection of key-value pairs, nestable.
comment: "Starts with #, single line only, not inline."
special chars: |
  Strings starting with special chars ({, [, !, &, *, ?, |, >, %, @, \`) need quotes.`, errLine: 2, errMsg: "yaml.scanner.ScannerError: while scanning a simple key could not find expected ':'" },
    { label: 'Other Errors', code: `indent: Use spaces, not tabs. Same level must be consistent, 2 spaces recommended.
key-value: Key and value separated by colon + space.
string: Generally unquoted; quote with single or double quotes when containing special characters.
multiline:
  folded: > converts newlines to spaces, adds a final newline.
  literal: | preserves original newlines.
number: Integers, floats, scientific notation written directly, e.g. 42, 3.14, 1e10.
boolean: true, false, yes, no, on, off (case-sensitive).
null: null, ~ or empty value (key only).
list: Dash + space prefix, e.g. - item1; or inline [item1, item2].
object/mapping: Collection of key-value pairs, nestable.
comment: "Starts with #, single line only, not inline."
special chars: |
  Strings starting with special chars ({, [, !, &, *, ?, |, >, %, @, \`) need quotes.`, errLine: 3, errMsg: 'yaml.scanner.ScannerError: mapping values are not allowed here' },
  ]

  const [activeErr, setActiveErr] = useState(-1)
  const currentCode = activeErr >= 0 ? errors[activeErr].code : baseCode
  const currentErrLine = activeErr >= 0 ? errors[activeErr].errLine : undefined
  const currentErrMsg = activeErr >= 0 ? errors[activeErr].errMsg : undefined

  return (
    <section className="sec" id="yaml">
      <div className="wrap">
        <h2 className="sec-title">{lang === 'cn' ? '对比YAML' : 'Compare YAML'}</h2>
        <div className="compare-layout">
          <R>
            <div className="tab-content-fade" key={`yaml-${activeErr}`}>
              <CodeBlock code={currentCode} lang="yaml" errLine={currentErrLine} errMsg={currentErrMsg} showLineNumbers showPointer variant="compare" />
            </div>
          </R>
          <RZ style={{ transitionDelay: '100ms' }}>
            <div>
              <div className="err-section-label">{lang === 'cn' ? '常见错误' : 'Common Errors'}</div>
              <div className="err-btn-group">
                {errors.map((e, i) => (
                  <button key={i} className={'err-btn' + (activeErr === i ? ' active' : '')} onClick={() => setActiveErr(activeErr === i ? -1 : i)}>
                    {e.label}
                  </button>
                ))}
              </div>
            </div>
          </RZ>
        </div>
        <R style={{ marginTop: 32 }}>
          <p style={{ fontSize: 13, color: 'var(--fg-dim)' }}>
            {lang === 'cn' ? '* 不同解析器解析不同，存在差异，这里只介绍常见的错误' : '* Different parsers behave differently; only common errors shown here'}
          </p>
        </R>
      </div>
    </section>
  )
}

/* ================================================================
   P5 Types & Features — 5种支持类型 + 2种额外功能
   ================================================================ */
/* 类型卡片图标组件 — per 颜色纠正.md */
function TypeIconKV({ lang }: { lang: 'cn' | 'en' }) {
  const k = lang === 'cn' ? '键' : 'Key'
  const v = lang === 'cn' ? '值' : 'Val'
  return (
    <div className="type-card-icon" style={{ fontFamily: 'var(--font-mono)', fontSize: 13 }}>
      <span style={{ color: '#87d43a' }}>{k}</span>
      <span style={{ color: 'var(--fg)' }}>{': '}</span>
      <span style={{ color: '#72bbf7' }}>{v}</span>
    </div>
  )
}
function TypeIconString() {
  return (
    <div className="type-card-icon" style={{ fontFamily: 'var(--font-mono)', fontSize: 13, display: 'flex', alignItems: 'center', gap: 4 }}>
      <span style={{ color: '#72bbf7' }}>{'"'}</span>
      <span style={{ display: 'inline-block', width: 28, height: 12, background: 'var(--fg-dim)', borderRadius: 2 }} />
      <span style={{ color: '#72bbf7' }}>{'"'}</span>
    </div>
  )
}
function TypeIconInlineList({ lang }: { lang: 'cn' | 'en' }) {
  const k = lang === 'cn' ? '键' : 'Key'
  return (
    <div className="type-card-icon" style={{ fontFamily: 'var(--font-mono)', fontSize: 13 }}>
      <span style={{ color: '#87d43a' }}>{k}</span>
      <span style={{ color: 'var(--fg)' }}>{': '}</span>
      <span style={{ color: '#f7ec24' }}>{'['}</span>
      <span style={{ display: 'inline-block', width: 20, height: 12, background: 'var(--fg-dim)', borderRadius: 2, margin: '0 2px', verticalAlign: 'middle' }} />
      <span style={{ color: '#f7ec24' }}>{']'}</span>
    </div>
  )
}
function TypeIconMultilineText() {
  return (
    <div className="type-card-icon" style={{ fontFamily: 'var(--font-mono)', fontSize: 13, display: 'flex', flexDirection: 'column', gap: 2, lineHeight: 1.3, padding: '8px 10px' }}>
      <div style={{ display: 'flex', justifyContent: 'flex-end', width: '100%' }}>
        <span style={{ color: '#ff993f' }}>{'{'}</span>
      </div>
      <span style={{ display: 'inline-block', width: 36, height: 12, background: 'var(--fg-dim)', borderRadius: 2 }} />
      <div style={{ display: 'flex', justifyContent: 'flex-start', width: '100%' }}>
        <span style={{ color: '#ff993f' }}>{'}'}</span>
      </div>
    </div>
  )
}
function TypeIconMultilineList() {
  return (
    <div className="type-card-icon" style={{ fontFamily: 'var(--font-mono)', fontSize: 13, display: 'flex', flexDirection: 'column', gap: 3, lineHeight: 1.3 }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
        <span style={{ color: 'var(--fg)' }}>{'-'}</span>
        <span style={{ display: 'inline-block', width: 28, height: 10, background: 'var(--fg-dim)', borderRadius: 2 }} />
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
        <span style={{ color: 'var(--fg)' }}>{'-'}</span>
        <span style={{ display: 'inline-block', width: 28, height: 10, background: 'var(--fg-dim)', borderRadius: 2 }} />
      </div>
    </div>
  )
}
function TypeIconMultiDoc() {
  return (
    <div className="type-card-icon" style={{ fontFamily: 'var(--font-mono)', fontSize: 13 }}>
      <span style={{ color: 'var(--fg)' }}>{'---'}</span>
    </div>
  )
}
function TypeIconComment() {
  return (
    <div className="type-card-icon" style={{ fontFamily: 'var(--font-mono)', fontSize: 13, display: 'flex', alignItems: 'center', gap: 4 }}>
      <span style={{ color: '#8B949E' }}>{'#'}</span>
      <span style={{ display: 'inline-block', width: 28, height: 12, background: 'var(--fg-dim)', borderRadius: 2 }} />
    </div>
  )
}

function TypesFeatures({ lang }: { lang: 'cn' | 'en' }) {
  const types = lang === 'cn' ? [
    { icon: <TypeIconKV lang="cn" />, label: '键值对' },
    { icon: <TypeIconString />, label: '字符串' },
    { icon: <TypeIconInlineList lang="cn" />, label: '内联列表' },
    { icon: <TypeIconMultilineText />, label: '多行文本' },
    { icon: <TypeIconMultilineList />, label: '多行列表' },
  ] : [
    { icon: <TypeIconKV lang="en" />, label: 'Key-Value' },
    { icon: <TypeIconString />, label: 'String' },
    { icon: <TypeIconInlineList lang="en" />, label: 'Inline List' },
    { icon: <TypeIconMultilineText />, label: 'Multiline Text' },
    { icon: <TypeIconMultilineList />, label: 'Multiline List' },
  ]

  const features = lang === 'cn' ? [
    { icon: <TypeIconMultiDoc />, label: '多文档' },
    { icon: <TypeIconComment />, label: '孤行注释' },
  ] : [
    { icon: <TypeIconMultiDoc />, label: 'Multi-Document' },
    { icon: <TypeIconComment />, label: 'Line Comment' },
  ]

  return (
    <section className="sec" id="types">
      <div className="wrap">
        <div className="types-grid">
          <R>
            <div className="type-col-title">{lang === 'cn' ? '支持类型' : 'Supported Types'}</div>
            {types.map((t, i) => (
              <div key={i} className="type-card">
                {t.icon}
                <div className="type-card-label">{t.label}</div>
              </div>
            ))}
          </R>
          <RZ style={{ transitionDelay: '100ms' }}>
            <div className="type-col-title">{lang === 'cn' ? '额外功能' : 'Extra Features'}</div>
            {features.map((f, i) => (
              <div key={i} className="type-card">
                {f.icon}
                <div className="type-card-label">{f.label}</div>
              </div>
            ))}
          </RZ>
        </div>
      </div>
    </section>
  )
}

/* ================================================================
   P6 YAML Compatibility — STML vs YAML 对比表
   ================================================================ */
function YAMLCompat({ lang }: { lang: 'cn' | 'en' }) {
  const rows = lang === 'cn' ? [
    ['边界', '基于缩进、冒号与换行', '基于缩进、冒号与换行'],
    ['Null支持', '支持null、~为Null', '支持null、~为Null'],
    ['字典嵌套', '支持', '支持'],
    ['内联列表', '支持', '支持'],
    ['注释', '独行注释', '支持'],
    ['多文档', '支持', '支持'],
  ] : [
    ['Boundary', 'Based on indent, colon & newline', 'Based on indent, colon & newline'],
    ['Null Support', 'Supports null, ~ as Null', 'Supports null, ~ as Null'],
    ['Dict Nesting', 'Supported', 'Supported'],
    ['Inline List', 'Supported', 'Supported'],
    ['Comments', 'Line-only comments', 'Supported'],
    ['Multi-Document', 'Supported', 'Supported'],
  ]

  return (
    <section className="sec" id="compat">
      <div className="wrap">
        <h2 className="sec-title">{lang === 'cn' ? '部分兼容YAML' : 'Partial YAML Compatibility'}</h2>
        <R>
          <div className="tbl-scroll">
          <table className="simple-tbl">
            <thead>
              <tr>
                <th></th>
                <th>STML</th>
                <th>YAML</th>
              </tr>
            </thead>
            <tbody>
              {rows.map((r, i) => (
                <tr key={i}>
                  <td style={{ fontWeight: 600, color: 'var(--fg)' }}>{r[0]}</td>
                  <td>{r[1]}</td>
                  <td>{r[2]}</td>
                </tr>
              ))}
            </tbody>
          </table>
          </div>
        </R>
      </div>
    </section>
  )
}

/* ================================================================
   P7 Scenario Suggestions — STML/YAML/JSON 三列对比表
   ================================================================ */
function ScenarioSuggestions({ lang }: { lang: 'cn' | 'en' }) {
  const rows = lang === 'cn' ? [
    ['流式传输', '✓', '×', '✓'],
    ['人类阅读', '✓', '✓', '相对较差'],
    ['模型生成', '✓', '容错不足', '依靠代码约束'],
    ['配置文件', '不建议', '✓ 支持引用等功能', '✓'],
    ['自修复', '仅容错', '×', '较多工具'],
    ['注释', '✓', '✓', '×'],
    ['LLM支持', '× 需学习', '✓', '✓'],
    ['类型解析', '不解析，程序根据需要转换', '解析', '解析'],
    ['字典嵌套', '✓', '✓', '✓'],
    ['语言生态', '待扩展', '标准不一', '✓'],
  ] : [
    ['Stream Transfer', '✓', '×', '✓'],
    ['Human Reading', '✓', '✓', 'Relatively poor'],
    ['Model Generation', '✓', 'Insufficient fault tolerance', 'Code-constrained'],
    ['Config Files', 'Not recommended', '✓ Supports references etc.', '✓'],
    ['Self-Repair', 'Fault tolerance only', '×', 'Many tools'],
    ['Comments', '✓', '✓', '×'],
    ['LLM Support', '× Needs learning', '✓', '✓'],
    ['Type Parsing', 'No parsing, convert as needed', 'Parsed', 'Parsed'],
    ['Dict Nesting', '✓', '✓', '✓'],
    ['Language Ecosystem', 'To be expanded', 'Inconsistent standards', '✓'],
  ]

  return (
    <section className="sec" id="scenario">
      <div className="wrap">
        <h2 className="sec-title">{lang === 'cn' ? '场景建议' : 'Scenario Suggestions'}</h2>
        <R>
          <div className="tbl-scroll">
          <table className="simple-tbl">
            <thead>
              <tr>
                <th></th>
                <th>STML</th>
                <th>YAML</th>
                <th>JSON</th>
              </tr>
            </thead>
            <tbody>
              {rows.map((r, i) => (
                <tr key={i}>
                  <td style={{ fontWeight: 600, color: 'var(--fg)' }}>{r[0]}</td>
                  <td>{r[1]}</td>
                  <td>{r[2]}</td>
                  <td>{r[3]}</td>
                </tr>
              ))}
            </tbody>
          </table>
          </div>
          <p className="tbl-note">{lang === 'cn' ? '* 仅供参考' : '* For reference only'}</p>
        </R>
      </div>
    </section>
  )
}

/* ================================================================
   P8 Generation Test — 生成测试数据表
   ================================================================ */
function GenerationTest({ lang }: { lang: 'cn' | 'en' }) {
  const rowLabels = lang === 'cn'
    ? ['预期数量', '实际数量', '平均长度', '有效数量', '有效比例', '有效对数', '匹配对数', '匹配比例']
    : ['Expected', 'Actual', 'Avg Length', 'Valid Count', 'Valid Ratio', 'Valid Pairs', 'Matched Pairs', 'Match Ratio']

  /* 3组数据: [STML, JSON] / [YAML, JSON] / [STML, YAML] */
  const groups: [string, string, (string | number)[][]][] = [
    ['STML', 'JSON', [
      [30, 30], [29, 29], [2133, 3040], [29, 21], ['100.0%', '72.4%'],
      [21], [2], ['9.5%'],
    ]],
    ['YAML', 'JSON', [
      [30, 30], [30, 30], [5213, 6695], [30, 30], ['100.0%', '100.0%'],
      [30], [12], ['40.0%'],
    ]],
    ['STML', 'YAML', [
      [30, 30], [29, 29], [1599, 1635], [29, 27], ['100.0%', '100.0%'],
      [27], [3], ['11.1%'],
    ]],
  ]

  const desc = lang === 'cn'
    ? '让 DeepSeek V4 flash 模型分别同时生成三组数据，STML和JSON、YAML和JSON、STML和YAML。可以看出LLM对三种语言的认识和理解，JSON出现了一些格式上的错误，这里有效数量为72.4%，每组进行对比，就连YAML和JSON组匹配比例也才40.0%，可见模型对三种语言的理解都不足，对STML的学习理解能力也不足。'
    : 'DeepSeek V4 flash model generated three groups of data simultaneously: STML&JSON, YAML&JSON, STML&YAML. JSON had formatting errors with only 72.4% validity. Even YAML&JSON match ratio was only 40.0%, showing the model\'s insufficient understanding of all three languages, including STML.'

  return (
    <section className="sec" id="gen-test">
      <div className="wrap">
        <h2 className="sec-title">{lang === 'cn' ? '生成测试' : 'Generation Test'}</h2>
        <R>
          <div className="gen-tbl-container">
            <div className="gen-tbl-row">
              {groups.map(([h1, h2, rows], gi) => (
                <table key={gi} className="gen-tbl">
                  <thead>
                    <tr>
                      <th></th>
                      <th>{h1}</th>
                      <th>{h2}</th>
                    </tr>
                  </thead>
                  <tbody>
                    {rowLabels.map((label, ri) => {
                      const isSingle = rows[ri].length === 1
                      return (
                        <tr key={ri}>
                          <td className="gen-tbl-label">{label}</td>
                          {isSingle ? (
                            <td colSpan={2} className="gen-tbl-val gen-tbl-single">{rows[ri][0]}</td>
                          ) : (
                            <>
                              <td className="gen-tbl-val">{rows[ri][0]}</td>
                              <td className="gen-tbl-val">{rows[ri][1]}</td>
                            </>
                          )}
                        </tr>
                      )
                    })}
                  </tbody>
                </table>
              ))}
            </div>
          </div>
        </R>
        <R style={{ marginTop: 32 }}>
          <p style={{ fontSize: 14, color: 'var(--fg-muted)', lineHeight: 1.8, maxWidth: 900, margin: '0 auto' }}>{desc}</p>
          <p className="tbl-note" style={{ marginTop: 16 }}>{lang === 'cn' ? '* 具体信息请阅读测试报告，测试结果不稳定，仅供参考' : '* See test report for details. Results may vary, for reference only.'}</p>
        </R>
      </div>
    </section>
  )
}

/* ================================================================
   P9 Conversion Test — 转换测试数据表
   ================================================================ */
function ConversionTest({ lang }: { lang: 'cn' | 'en' }) {
  const t = lang === 'cn'
    ? { expected: '预期数量', actual: '实际数量', matchCount: '匹配数量', matchRatio: '匹配比例', matchCountIgnore: '匹配对数（忽略类型）', matchRatioIgnore: '匹配比例（忽略类型）' }
    : { expected: 'Expected', actual: 'Actual', matchCount: 'Matched', matchRatio: 'Match Ratio', matchCountIgnore: 'Matched (ignore type)', matchRatioIgnore: 'Ratio (ignore type)' }

  const desc = lang === 'cn'
    ? '我们是给定了一种语言的文本，让大模型转换为另一种语言，依旧是YAML和JSON的转换准确率要高，JSON和STML组排第二，这应该和提示词也有关系。最后的结论和上轮一样，大模型对三种语言的认识约束均不够强，理解也不足，对STML的学习更是不行。'
    : 'Given text in one language, the model converts to another. YAML↔JSON conversion accuracy is highest, JSON↔STML ranks second, likely related to prompts. Same conclusion as before: the model\'s understanding of all three languages is insufficient, especially STML.'

  const cols = lang === 'cn'
    ? ['YAML→JSON', 'YAML→STML', 'JSON→YAML', 'JSON→STML', 'STML→YAML', 'STML→JSON']
    : ['YAML→JSON', 'YAML→STML', 'JSON→YAML', 'JSON→STML', 'STML→YAML', 'STML→JSON']

  const data = [
    [20, 20, 20, 20, 20, 20],
    [20, 20, 20, 20, 20, 19],
    [11, 2, 8, 0, 4, 4],
    ['55.0%', '10.0%', '40.0%', '0.0%', '20.0%', '21.1%'],
    [11, 3, 9, 7, 4, 5],
    ['55.5%', '15.0%', '45.0%', '35.0%', '20.0%', '26.3%'],
  ]
  const rowLabels = [t.expected, t.actual, t.matchCount, t.matchRatio, t.matchCountIgnore, t.matchRatioIgnore]

  return (
    <section className="sec" id="convert-test">
      <div className="wrap">
        <h2 className="sec-title">{lang === 'cn' ? '转换测试（部分）' : 'Conversion Test (Partial)'}</h2>
        <R>
          <div className="tbl-scroll">
          <table className="simple-tbl">
            <thead>
              <tr>
                <th></th>
                {cols.map((c, i) => <th key={i}>{c}</th>)}
              </tr>
            </thead>
            <tbody>
              {data.map((row, ri) => (
                <tr key={ri}>
                  <td style={{ fontWeight: 600, color: 'var(--fg)', whiteSpace: 'nowrap' }}>{rowLabels[ri]}</td>
                  {row.map((cell, ci) => <td key={ci}>{cell}</td>)}
                </tr>
              ))}
            </tbody>
          </table>
          </div>
        </R>
        <R style={{ marginTop: 32 }}>
          <p style={{ fontSize: 14, color: 'var(--fg-muted)', lineHeight: 1.8, maxWidth: 900, margin: '0 auto' }}>{desc}</p>
          <p className="tbl-note" style={{ marginTop: 16 }}>{lang === 'cn' ? '* 具体信息请阅读测试报告，测试结果不稳定，仅供参考' : '* See test report for details. Results may vary, for reference only.'}</p>
        </R>
      </div>
    </section>
  )
}

/* ================================================================
   P10 Limitations — 不支持 + 应避免
   ================================================================ */
function Limitations({ lang }: { lang: 'cn' | 'en' }) {
  const notSupported = lang === 'cn'
    ? `"嵌套列表": ["外层列表", ["内层列表"]]
"内联字典": {"键": "值"}
"嵌套列表":
  - ["元素1"]
  - ["元素2"]`
    : `"nested list": ["outer", ["inner"]]
"inline dict": {"key": "value"}
"nested list":
  - ["element1"]
  - ["element2"]`

  const avoid = lang === 'cn'
    ? `"支持": ["Markdown", "代码", "JSON"]
"例如": {
{
  "JSON": "也用到了右花括号"
}`
    : `"support": ["Markdown", "code", "JSON"]
"example": {
{
  "JSON": "also uses right brace"
}`

  const notSupportedNote = lang === 'cn'
    ? 'STML不支持二维及更高维的列表，也不会将花括号识别为字典'
    : 'STML does not support 2D or higher-dimensional lists, and does not treat braces as dictionaries'

  const avoidNote = lang === 'cn'
    ? '使用JSON时，无比使用行内JSON，或者使用换行转义，避免与多行文本的终结符冲突。'
    : 'When using JSON, avoid inline JSON or use newline escaping to avoid conflicts with multiline text terminators.'

  return (
    <section className="sec" id="limitations">
      <div className="wrap">
        <div className="limit-grid">
          <R>
            <div className="limit-title">{lang === 'cn' ? '不支持' : 'Not Supported'}</div>
            <CodeBlock code={notSupported} lang="stml" stml showLineNumbers />
            <p className="limit-note">{notSupportedNote}</p>
          </R>
          <RZ style={{ transitionDelay: '100ms' }}>
            <div className="limit-title">{lang === 'cn' ? '应避免' : 'Should Avoid'}</div>
            <CodeBlock code={avoid} lang="stml" stml showLineNumbers forceLastBraceOrange />
            <p className="limit-note">{avoidNote}</p>
          </RZ>
        </div>
      </div>
    </section>
  )
}

/* ================================================================
   P11 Cases — Agent交互案例 + VS Code截图对比
   ================================================================ */

function Cases({ lang }: { lang: 'cn' | 'en' }) {
  const [lightbox, setLightbox] = useState<string | null>(null)
  const desc = lang === 'cn'
    ? '之前使用的是YAML，测试过程中容易出现全角引号误用、多行文本格式错误、引号嵌套等问题。在改用STML之后，在近似的环境下输入近似的内容，就没再发生问题。'
    : 'Previously using YAML, issues like full-width quote misuse, multiline format errors, and quote nesting were common. After switching to STML, these problems no longer occur in similar environments.'

  // Page 24: Agent interaction cases — 6-column grid layout per design
  const casesGroups = [
    {
      code: `info_agent: {
  find user task, key word: "graduation"
}`,
      bot: true,
      finger: true,
      agent: 'Info Agent',
      loading: true,
      type: lang === 'cn' ? '自然语言' : 'Natural Lang',
    },
    {
      code: `---
info_agent:
  - model: deepseek-v4 flash`,
      bot: false,
      finger: false,
      agent: '',
      loading: false,
      type: lang === 'cn' ? '命令语言' : 'Command Lang',
    },
    {
      code: `---
file_agent: {
  find file, key word of title: "graduation"
}`,
      bot: true,
      finger: true,
      agent: 'File Agent',
      loading: true,
      type: lang === 'cn' ? '自然语言' : 'Natural Lang',
    },
    {
      code: `---
file_agent:
  - model: deepseek-v4 flash`,
      bot: false,
      finger: false,
      agent: '',
      loading: false,
      type: lang === 'cn' ? '命令语言' : 'Command Lang',
    },
    {
      code: `---
user: {
  I'm finding your task, please wait.
}`,
      bot: true,
      finger: true,
      agent: lang === 'cn' ? '回复用户' : 'Reply User',
      loading: false,
      type: lang === 'cn' ? '自然语言' : 'Natural Lang',
    },
    {
      code: `---
tool:
  - how to use: reminder`,
      bot: true,
      finger: true,
      agent: lang === 'cn' ? '系统操作' : 'System Action',
      loading: false,
      type: lang === 'cn' ? '命令语言' : 'Command Lang',
    },
    {
      code: `---
msg_gateway:
  - wait: [system, info_agent]
  - timeout: 10`,
      bot: false,
      finger: false,
      agent: '',
      loading: false,
      type: lang === 'cn' ? '命令语言' : 'Command Lang',
    },
    {
      code: `---
campus_mcp: ...`,
      bot: false,
      finger: false,
      agent: '',
      loading: false,
      type: lang === 'cn' ? '命令语言' : 'Command Lang',
    },
  ]

  return (
    <section className="sec" id="cases">
      <div className="wrap">
        <h2 className="sec-title">{lang === 'cn' ? '案例' : 'Cases'}</h2>

        {/* Page 24: Agent interaction cases — 6-column grid per design稿 */}
        <RZ>
          <div className="cases-p24">
            {casesGroups.map((g, idx) => (
              <div key={idx} className="cases-p24-row">
                {/* Col 1: Main bot icon — only one, at the first row */}
                <div className="cases-p24-col1">
                  {idx === 0 && <MiniBotIcon />}
                </div>

                {/* Col 2: STML code output (no code box styling) */}
                <div className="cases-p24-col2">
                  <pre
                    className="cases-p24-pre"
                    dangerouslySetInnerHTML={{ __html: renderColoredCode(g.code) }}
                  />
                </div>

                {/* Col 3: Finger emoji */}
                <div className="cases-p24-col3">
                  {g.finger ? '👈' : null}
                </div>

                {/* Col 4: Agent name + optional bot icon */}
                <div className="cases-p24-col4">
                  {g.agent && (
                    <div className={`cases-p24-agent${g.agent === 'Info Agent' || g.agent === 'File Agent' ? ' cases-p24-agent-with-bot' : ''}`}>
                      {(g.agent === 'Info Agent' || g.agent === 'File Agent') && <MiniBotIcon />}
                      <span>{g.agent}</span>
                    </div>
                  )}
                </div>

                {/* Col 5: Loading indicator */}
                <div className="cases-p24-col5">
                  {g.loading && (
                    <div className="cases-p24-loading">
                      {lang === 'cn' ? '搜索中' : 'Searching'}
                      <span className="loading-dots">
                        <span>.</span><span>.</span><span>.</span>
                      </span>
                    </div>
                  )}
                </div>

                {/* Col 6: Language type with left arrow */}
                <div className="cases-p24-col6">
                  <span className="cases-p24-arrow">◀</span>
                  <span>{g.type}</span>
                </div>
              </div>
            ))}
          </div>
        </RZ>

        {/* Page 25: Screenshot comparison */}
        <div style={{ marginTop: 80 }}>
          {/* Note */}
          <R>
            <p style={{ fontSize: 13, color: 'var(--fg-dim)', textAlign: 'right', marginBottom: 32 }}>
              {lang === 'cn' ? '* 大模型的回复存在不确定性，测试结果仅供参考' : '* LLM responses have uncertainty, test results for reference only'}
            </p>
          </R>

          {/* Effect screenshots */}
          <RZ>
            <div className="cases-compare-row">
              <div className="cases-compare-label">{lang === 'cn' ? '使用STML解析' : 'Using STML Parser'}</div>
              <img
                src="/STML/案例效果-使用STML解析.png"
                alt="STML Parser Result"
                className="cases-compare-img cases-compare-img-tap"
                onClick={() => setLightbox('/STML/案例效果-使用STML解析.png')}
              />
            </div>
            <div className="cases-compare-row">
              <div className="cases-compare-label">{lang === 'cn' ? '使用YAML解析' : 'Using YAML Parser'}</div>
              <img
                src="/STML/案例效果-使用YAML解析.png"
                alt="YAML Parser Error"
                className="cases-compare-img cases-compare-img-tap"
                onClick={() => setLightbox('/STML/案例效果-使用YAML解析.png')}
              />
            </div>
          </RZ>

          {/* Lightbox overlay for small screens */}
          {lightbox && (
            <div className="lightbox-overlay" onClick={() => setLightbox(null)}>
              <img src={lightbox} alt="" className="lightbox-img" />
            </div>
          )}

          <R style={{ marginTop: 32 }}>
            <div style={{ maxWidth: 900, margin: '0 auto', textAlign: 'left' }}>
              <p style={{ fontSize: 14, color: 'var(--fg-muted)', lineHeight: 1.8 }}>
                {desc}
              </p>
            </div>
          </R>
        </div>
      </div>
    </section>
  )
}

/* ================================================================
   CTA
   ================================================================ */
function CTA({ lang }: { lang: 'cn' | 'en' }) {
  return (
    <section className="cta-section">
      <div className="wrap">
        <R>
          <h2 className="cta-ttl">
            {lang === 'cn'
              ? <>如 YAML <span className="ac">易读</span> + 如 JSON 般<span className="ac">确定</span> + 前所未有的<span className="ac">容错理解能力</span></>
              : <>As <span className="ac">readable</span> as YAML + as <span className="ac">deterministic</span> as JSON + unprecedented <span className="ac">fault tolerance</span></>
            }
          </h2>
        </R>
        <R style={{ transitionDelay: '140ms' }}>
          <p className="cta-sub">
            {lang === 'cn' ? '一种适用于人类阅读和机器生成的文本格式' : 'A text format for both human reading and machine generation'}
          </p>
        </R>
        <R style={{ transitionDelay: '280ms' }}>
          <div className="cta-links">
            <a href="#" className="cta-link cta-link-primary"><GitHubIcon /> Github</a>
            <a href="#" className="cta-link">{lang === 'cn' ? '规范文档 >' : 'Specification >'}</a>
          </div>
        </R>
      </div>
    </section>
  )
}

/* ═══ Footer ═══ */
function Footer({ lang }: { lang: 'cn' | 'en' }) {
  return (
    <footer className="ft">
      <p className="ft-version">Stable Tree Markup Language v1.6</p>
      <div className="ft-links">
        <a href="https://github.com" target="_blank" rel="noopener noreferrer">GitHub</a>
        <a href="#">Document</a>
        <a href="https://bilibili.com" target="_blank" rel="noopener noreferrer">Bilibili</a>
      </div>
      <div className="ft-copyright">
        <span>© 2026 kai {lang === 'cn' ? '版权所有' : 'All Rights Reserved'}</span>
        <span>Webpage By: LLM & Kai</span>
        <span>{lang === 'cn' ? '网信许可占位' : 'ICP License Placeholder'}</span>
      </div>
    </footer>
  )
}
