/**
 * STML Browser API — 浏览器端 STML 解析器
 * 桥接 stml_ts 源码，仅使用 loads() 相关模块
 */
import { nullNode, stringNode, listNode, mapNode, AstNode as StmlAstNode } from '../../stml_ts/src/ast'
import { STMLLexer, Warning } from '../../stml_ts/src/lexer'
import { LineTreeBuilder, AstBuilder } from '../../stml_ts/src/parser'

/* ── STML AST → 页面 ASTNode 转换 ── */

export interface ASTNode {
  type: string
  label: string
  children?: ASTNode[]
}

function stmlToVis(node: StmlAstNode): ASTNode {
  if (node.isNull()) return { type: 'null', label: 'null' }
  if (node.isString()) return { type: 'src', label: node.asString() ?? '' }
  if (node.isList()) {
    const items = node.asList() ?? []
    return { type: 'list', label: 'list', children: items.map(stmlToVis) }
  }
  if (node.isMap()) {
    const entries = node.asMap() ?? []
    return {
      type: 'dict', label: 'dict',
      children: entries.map(([key, val]) => ({
        type: 'key', label: key,
        children: [stmlToVis(val)]
      }))
    }
  }
  return { type: 'null', label: 'null' }
}

export function parseSTML(text: string): { ast: ASTNode; warnings: Warning[] } {
  const lexer = new STMLLexer(text)
  const { tokens, warnings: lexWarnings } = lexer.tokenize()

  const treeBuilder = new LineTreeBuilder()
  treeBuilder.process(tokens)

  const astBuilder = new AstBuilder()
  const docsList = treeBuilder.docs.map(docLines => astBuilder.build(docLines))

  const allWarnings = [...lexWarnings, ...treeBuilder.warnings, ...astBuilder.warnings_]

  // 包装为 docs 列表
  const wrapper: Array<[string, StmlAstNode]> = [['docs', listNode(docsList)]]
  const rootAst = mapNode(wrapper)

  return { ast: stmlToVis(rootAst), warnings: allWarnings }
}
