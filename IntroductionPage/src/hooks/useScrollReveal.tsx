import { useRef } from 'react'

/* 全局状态 */
let _visibleMap: Record<string, boolean> = {}
let _counter = 0

function _setEl(el: Element | null) {
  if (!el) return
  const key = 'r' + _counter++
  ;(el as HTMLElement).dataset.rvKey = key
  const ob = new IntersectionObserver(
    ([entry]) => { if (entry.isIntersecting) { _visibleMap = { ..._visibleMap, [key]: true }; el.classList.add('v') } },
    { threshold: 0.12, rootMargin: '0px 0px -36px 0px' }
  )
  ob.observe(el)
}

/* Reveal: 淡入上移 */
export function R(props: { children: React.ReactNode; style?: React.CSSProperties; className?: string }) {
  const ref = useRef<HTMLDivElement>(null)
  /* 用 useEffect 绑定 observer */
  return (
    <div ref={(el) => { if (el) { _setEl(el) } }}
      className={'rv' + (props.className ? ' ' + props.className : '')}
      style={props.style}
    >
      {props.children}
    </div>
  )
}

/* RevealScale: 缩放淡入 */
export function RZ(props: { children: React.ReactNode; style?: React.CSSProperties; className?: string }) {
  return (
    <div ref={(el) => { if (el) { _setEl(el) } }}
      className={'rv-z' + (props.className ? ' ' + props.className : '')}
      style={props.style}
    >
      {props.children}
    </div>
  )
}
