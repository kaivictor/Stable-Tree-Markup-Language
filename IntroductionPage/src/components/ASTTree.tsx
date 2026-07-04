import { useRef, useEffect, useMemo } from 'react'

export interface ASTNode {
  type: string
  label: string
  children?: ASTNode[]
}

interface ASTTreeProps {
  data: ASTNode
}

/* ── 尺寸 ── */
const NODE_H = 22
const PAD_X = 8
const RADIUS = 3
const V_GAP = 4
const BRANCH_W = 28       /* ├─ / └─ 水平分支宽度 */
const H_GAP = 16          /* 类型框与值框间隙 */
const PAD = 12
const MAX_NODE_W = 440    /* 单个节点（类型+值）最大宽度 */
const FONT = '11px -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans SC", sans-serif'

/* ── 颜色 ── */
const TYPE_BG = '#1a3566'; const TYPE_BD = '#264a8a'; const TYPE_FG = '#7aadf0'
const VAL_BG  = '#24488a'; const VAL_BD  = '#3468b8'; const VAL_FG  = '#ffffff'
const CNT_BG  = '#162d50'; const CNT_BD  = '#245080'; const CNT_FG  = '#5a98e0'
const NULL_BG = '#301a50'; const NULL_BD = '#48307a'; const NULL_FG = '#b888e0'
const LINE_CLR = '#4a7acc'
const LINE_W = 1.2

const CNT_TYPES = new Set(['docs', 'dict', 'list'])

/* ── 工具 ── */
function measure(ctx: CanvasRenderingContext2D, t: string) {
  ctx.font = FONT; return ctx.measureText(t).width
}
function boxW(ctx: CanvasRenderingContext2D, t: string) {
  return measure(ctx, t) + PAD_X * 2
}
/* 截断文本使其宽度不超过 maxW（像素），超出加 … */
function truncate(ctx: CanvasRenderingContext2D, t: string, maxW: number): string {
  if (boxW(ctx, t) <= maxW) return t
  const ellipsis = '…'
  const ellW = measure(ctx, ellipsis)
  let lo = 0, hi = t.length
  while (lo < hi) {
    const mid = (lo + hi + 1) >> 1
    if (measure(ctx, t.slice(0, mid)) + ellW + PAD_X * 2 <= maxW) lo = mid
    else hi = mid - 1
  }
  return lo > 0 ? t.slice(0, lo) + ellipsis : ellipsis
}

/* ── 布局 ── */
interface LN {
  x: number; y: number           /* 类型框左上角 */
  tw: number                      /* 类型框宽 */
  vx: number; vw: number         /* 值框相对x偏移和宽 */
  hasVal: boolean
  depth: number
  isLast: boolean                 /* 是否是父节点的最后一个子节点 */
  parentHasMore: boolean[]        /* 祖先中哪些还有后续兄弟（用于画 │ ） */
  node: ASTNode
  kids: LN[]
  _override?: { type?: string; label?: string }
}

function rows(n: ASTNode): number {
  if (!n.children?.length) return 1
  return 1 + n.children.reduce((s, c) => s + rows(c), 0)
}

function lay(
  ctx: CanvasRenderingContext2D,
  n: ASTNode, depth: number, y: number,
  isLast: boolean, parentHasMore: boolean[],
  isRoot: boolean
): LN {
  const x = PAD + depth * BRANCH_W
  let tw = 0, hasVal = false, vx = 0, vw = 0
  let ln_override: { type?: string; label?: string } | undefined

  if (isRoot) {
    tw = boxW(ctx, n.label)
  } else if (CNT_TYPES.has(n.type)) {
    tw = boxW(ctx, n.type)
  } else {
    tw = boxW(ctx, n.type)
    hasVal = !!n.label
    vx = tw + H_GAP
    vw = hasVal ? boxW(ctx, n.label) : 0
  }

  /* 限制单个节点总宽度不超过 MAX_NODE_W */
  const availForNode = MAX_NODE_W - (x - PAD)  // 减去缩进占用的空间
  if (!isRoot && !CNT_TYPES.has(n.type) && hasVal) {
    const totalW = tw + H_GAP + vw
    if (totalW > availForNode) {
      // 优先截断值框，保留类型框
      const maxVW = availForNode - tw - H_GAP
      if (maxVW < boxW(ctx, '…')) {
        // 值框放不下了，整体用省略号
        const label = truncate(ctx, n.type + '─' + n.label, availForNode)
        tw = boxW(ctx, label); hasVal = false; vx = 0; vw = 0
        // 存储截断后的文本
        ln_override = { type: label }
      } else {
        // 截断值
        const truncLabel = truncate(ctx, n.label, maxVW)
        vw = boxW(ctx, truncLabel)
        ln_override = { label: truncLabel }
      }
    }
  } else if (tw > availForNode) {
    const truncText = truncate(ctx, n.label || n.type, availForNode)
    tw = boxW(ctx, truncText)
    if (isRoot) ln_override = { label: truncText }
    else ln_override = { type: truncText }
  }

  const ln: LN = { x, y, tw, vx, vw, hasVal, depth, isLast, parentHasMore, node: n, kids: [] }
  if (ln_override) ln._override = ln_override

  if (!n.children?.length) return ln

  let cy = y + NODE_H + V_GAP
  for (let i = 0; i < n.children.length; i++) {
    const c = n.children[i]
    const cIsLast = i === n.children.length - 1
    /* 传递祖先的 hasMore：当前层之后的所有层 */
    const cHasMore = [...parentHasMore, !cIsLast]
    const r = rows(c)
    ln.kids.push(lay(ctx, c, depth + 1, cy, cIsLast, cHasMore, false))
    cy += r * (NODE_H + V_GAP)
  }
  return ln
}

function bounds(ln: LN) {
  const r = ln.hasVal ? ln.x + ln.vx + ln.vw : ln.x + ln.tw
  let x0 = ln.x, y0 = ln.y, x1 = r, y1 = ln.y + NODE_H
  for (const k of ln.kids) {
    const b = bounds(k)
    x0 = Math.min(x0, b.x0); y0 = Math.min(y0, b.y0)
    x1 = Math.max(x1, b.x1); y1 = Math.max(y1, b.y1)
  }
  return { x0, y0, x1, y1 }
}

/* ── 绘制 ── */
function rr(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.beginPath()
  ctx.moveTo(x + r, y); ctx.lineTo(x + w - r, y)
  ctx.quadraticCurveTo(x + w, y, x + w, y + r)
  ctx.lineTo(x + w, y + h - r)
  ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
  ctx.lineTo(x + r, y + h)
  ctx.quadraticCurveTo(x, y + h, x, y + h - r)
  ctx.lineTo(x, y + r)
  ctx.quadraticCurveTo(x, y, x + r, y)
  ctx.closePath()
}

function drawBox(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, bg: string, bd: string, fg: string, text: string) {
  rr(ctx, x, y, w, h, RADIUS)
  ctx.fillStyle = bg; ctx.fill()
  ctx.strokeStyle = bd; ctx.lineWidth = 1; ctx.stroke()
  ctx.fillStyle = fg; ctx.font = FONT
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle'
  ctx.fillText(text, x + w / 2, y + h / 2)
}

function drawNode(ctx: CanvasRenderingContext2D, ln: LN, isRoot: boolean) {
  const { x, y, node } = ln
  const oType = ln._override?.type
  const oLabel = ln._override?.label

  if (isRoot) {
    drawBox(ctx, x, y, ln.tw, NODE_H, CNT_BG, CNT_BD, CNT_FG, oLabel ?? node.label)
    return
  }
  if (CNT_TYPES.has(node.type)) {
    drawBox(ctx, x, y, ln.tw, NODE_H, CNT_BG, CNT_BD, CNT_FG, node.type)
    return
  }

  /* 类型框 */
  drawBox(ctx, x, y, ln.tw, NODE_H, TYPE_BG, TYPE_BD, TYPE_FG, oType ?? node.type)

  /* 值框 */
  if (ln.hasVal && ln.vw > 0) {
    const vx = x + ln.vx
    const bg = node.type === 'null' ? NULL_BG : VAL_BG
    const bd = node.type === 'null' ? NULL_BD : VAL_BD
    const fg = node.type === 'null' ? NULL_FG : VAL_FG
    drawBox(ctx, vx, y, ln.vw, NODE_H, bg, bd, fg, oLabel ?? node.label)

    /* 类型→值 短横线 */
    ctx.strokeStyle = LINE_CLR; ctx.lineWidth = LINE_W
    ctx.beginPath()
    ctx.moveTo(x + ln.tw, y + NODE_H / 2)
    ctx.lineTo(vx, y + NODE_H / 2)
    ctx.stroke()
  }
}

/* 画 ├─ └─ │ 风格连线 */
function drawLines(ctx: CanvasRenderingContext2D, ln: LN, isRoot: boolean) {
  if (isRoot) {
    /* 根节点直接画子节点连线 */
    for (const k of ln.kids) drawLines(ctx, k, false)
    return
  }

  ctx.strokeStyle = LINE_CLR; ctx.lineWidth = LINE_W
  ctx.lineCap = 'round'

  const midY = ln.y + NODE_H / 2

  /* 画祖先的 │ 延续线 */
  for (let d = 0; d < ln.parentHasMore.length; d++) {
    if (ln.parentHasMore[d]) {
      const lx = PAD + d * BRANCH_W + BRANCH_W / 2
      /* 垂直线从本节点上方延伸到下方（覆盖整个节点行高） */
      ctx.beginPath()
      ctx.moveTo(lx, ln.y - V_GAP)
      ctx.lineTo(lx, ln.y + NODE_H + V_GAP)
      ctx.stroke()
    }
  }

  /* 画本层的 ├─ 或 └─ */
  const branchX = ln.x - BRANCH_W + BRANCH_W / 2  /* 分支垂直线x */
  const nodeLeftX = ln.x                           /* 节点左侧x */

  /* 垂直线：从上方延伸到本节点中心（├）或只到本节点中心（└） */
  if (!ln.isLast) {
    /* ├─ ：垂直线从上方穿过中心继续向下 */
    ctx.beginPath()
    ctx.moveTo(branchX, ln.y - V_GAP)
    ctx.lineTo(branchX, midY)
    ctx.stroke()
  } else {
    /* └─ ：垂直线从上方到中心 */
    ctx.beginPath()
    ctx.moveTo(branchX, ln.y - V_GAP)
    ctx.lineTo(branchX, midY)
    ctx.stroke()
  }

  /* 水平分支线：从垂直线到节点左侧 */
  ctx.beginPath()
  ctx.moveTo(branchX, midY)
  ctx.lineTo(nodeLeftX, midY)
  ctx.stroke()

  /* 递归画子节点 */
  for (const k of ln.kids) drawLines(ctx, k, false)
}

function drawAll(ctx: CanvasRenderingContext2D, ln: LN, isRoot: boolean) {
  drawNode(ctx, ln, isRoot)
  for (const k of ln.kids) drawAll(ctx, k, false)
}

/* ── 组件 ── */
export default function ASTTree({ data }: ASTTreeProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null)

  const size = useMemo(() => {
    const c = document.createElement('canvas')
    const ctx = c.getContext('2d')!
    const ln = lay(ctx, data, 0, PAD, true, [], true)
    const b = bounds(ln)
    return {
      w: Math.max(200, b.x1 - b.x0 + PAD * 2),
      h: Math.max(150, b.y1 - b.y0 + PAD * 2)
    }
  }, [data])

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')!
    const dpr = window.devicePixelRatio || 1
    canvas.width = size.w * dpr; canvas.height = size.h * dpr
    ctx.scale(dpr, dpr)
    ctx.clearRect(0, 0, size.w, size.h)

    const ln = lay(ctx, data, 0, PAD, true, [], true)
    drawLines(ctx, ln, true)
    drawAll(ctx, ln, true)
  }, [data, size])

  return (
    <div className="ast-container">
      <canvas ref={canvasRef} style={{ width: size.w, height: size.h, display: 'block' }} />
    </div>
  )
}