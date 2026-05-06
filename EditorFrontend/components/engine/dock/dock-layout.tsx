"use client"

import { useRef, useCallback } from "react"
import { useDock, type DockNode, type DockCell, type DockRow, type DockColumn } from "./dock-context"
import { DockPanel } from "./dock-panel"
import { FloatingPanel } from "./floating-panel"

// ─── resize splitter ──────────────────────────────────────────────────────────

interface SplitterProps {
    direction: "horizontal" | "vertical"
    prevId: string
    nextId: string
    prevWeight: number
    nextWeight: number
}

function Splitter({ direction, prevId, nextId, prevWeight, nextWeight }: SplitterProps) {
    const { updateWeight } = useDock()
    const dragging = useRef(false)
    const startPos = useRef(0)
    const startPrev = useRef(prevWeight)
    const startNext = useRef(nextWeight)
    const containerRef = useRef<HTMLDivElement>(null)

    const onMouseDown = useCallback(
        (e: React.MouseEvent) => {
            e.preventDefault()
            dragging.current = true
            startPos.current = direction === "horizontal" ? e.clientX : e.clientY
            startPrev.current = prevWeight
            startNext.current = nextWeight

            const container = containerRef.current?.parentElement
            const totalPx = container
                ? direction === "horizontal"
                    ? container.offsetWidth
                    : container.offsetHeight
                : 600
            const totalWeight = prevWeight + nextWeight

            const onMove = (ev: MouseEvent) => {
                if (!dragging.current) return
                const delta = (direction === "horizontal" ? ev.clientX : ev.clientY) - startPos.current
                const deltaNorm = (delta / totalPx) * totalWeight
                const newPrev = Math.max(0.1, startPrev.current + deltaNorm)
                const newNext = Math.max(0.1, startNext.current - deltaNorm)
                updateWeight(prevId, newPrev)
                updateWeight(nextId, newNext)
            }

            const onUp = () => {
                dragging.current = false
                window.removeEventListener("mousemove", onMove)
                window.removeEventListener("mouseup", onUp)
            }

            window.addEventListener("mousemove", onMove)
            window.addEventListener("mouseup", onUp)
        },
        [direction, prevId, nextId, prevWeight, nextWeight, updateWeight]
    )

    return (
        <div
            ref={containerRef}
            className={`shrink-0 bg-neutral-800 hover:bg-white/30 active:bg-white/40 transition-colors z-10 group
        ${direction === "horizontal"
                    ? "w-[3px] cursor-col-resize"
                    : "h-[3px] cursor-row-resize"
                }`}
            onMouseDown={onMouseDown}
        >
            <div
                className={`bg-white/0 group-hover:bg-white/20 transition-colors
          ${direction === "horizontal" ? "w-full h-full" : "w-full h-full"}`}
            />
        </div>
    )
}

// ─── recursive node renderer ──────────────────────────────────────────────────

function RenderNode({ node }: { node: DockNode }) {
    if (node.type === "cell") {
        return (
            <DockPanel
                tabGroup={(node as DockCell).tabGroup}
                style={{ flex: (node as DockCell).weight }}
                className="min-h-0 min-w-0"
            />
        )
    }

    const isRow = node.type === "row"
    const children = (node as DockRow | DockColumn).children

    // Build siblings: [child0, splitter01, child1, splitter12, child2 ...]
    const items: React.ReactNode[] = []
    children.forEach((child, i) => {
        if (i > 0) {
            items.push(
                <Splitter
                    key={`split-${children[i - 1].id}-${child.id}`}
                    direction={isRow ? "horizontal" : "vertical"}
                    prevId={children[i - 1].id}
                    nextId={child.id}
                    prevWeight={(children[i - 1] as any).weight ?? 1}
                    nextWeight={(child as any).weight ?? 1}
                />
            )
        }
        items.push(<RenderNode key={child.id} node={child} />)
    })

    return (
        <div
            className={`flex min-h-0 min-w-0 ${isRow ? "flex-row" : "flex-col"}`}
            style={{ flex: (node as any).weight ?? 1 }}
        >
            {items}
        </div>
    )
}

// ─── global drag cursor overlay ───────────────────────────────────────────────

function DragCursorOverlay() {
    const { dragState } = useDock()
    if (!dragState.panelId) return null
    return (
        <div className="fixed inset-0 z-30 cursor-grabbing" style={{ pointerEvents: "none" }} />
    )
}

// ─── main layout ─────────────────────────────────────────────────────────────

export function DockLayout() {
    const { layout } = useDock()

    return (
        <div className="flex-1 overflow-hidden relative flex flex-col">
            {/* Root layout tree */}
            <div className="flex flex-1 flex-row min-h-0 overflow-hidden">
                <RenderNode node={layout.root} />
            </div>

            {/* Floating panels rendered on top */}
            {layout.floatingPanels.map((fp) => (
                <FloatingPanel key={fp.panelId} floating={fp} />
            ))}

            <DragCursorOverlay />
        </div>
    )
}