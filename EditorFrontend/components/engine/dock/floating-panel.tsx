"use client"

import { useRef, useCallback, useState } from "react"
import { X, GripHorizontal } from "lucide-react"
import { useDock, type FloatingPanel as FP, type PanelId, type DockZone } from "./dock-context"
import { PanelContent } from "./panel-content"

const PANEL_LABELS: Record<PanelId, string> = {
    viewport: "Viewport",
    outliner: "Outliner",
    details: "Details",
    "content-browser": "Content Browser",
    "remote-viewport": "Remote Viewport",
    "script-editor": "Script Editor",
    "world-details": "World Details",
    "place-actors": "Place Actors",
}

interface FloatingPanelProps {
    floating: FP
}

export function FloatingPanel({ floating }: FloatingPanelProps) {
    const { setDragState, setFloatingPosition, dockFloatingPanel, dragState } = useDock()
    const dragging = useRef(false)
    const offset = useRef({ x: 0, y: 0 })
    const [size, setSize] = useState({ w: floating.width, h: floating.height })
    const resizing = useRef<{
        edge: string
        startX: number
        startY: number
        startW: number
        startH: number
        startLeft: number
        startTop: number
    } | null>(null)
    const [pos, setPos] = useState({ x: floating.x, y: floating.y })

    // Check for drop zone under cursor position
    const checkForDropZone = useCallback(
        (clientX: number, clientY: number) => {
            // Find all dock drop zones
            const dropZones = document.querySelectorAll("[data-dock-drop-zone]")
            for (const zone of dropZones) {
                const rect = zone.getBoundingClientRect()
                if (
                    clientX >= rect.left &&
                    clientX <= rect.right &&
                    clientY >= rect.top &&
                    clientY <= rect.bottom
                ) {
                    const tabGroupId = zone.getAttribute("data-tab-group-id")
                    const dockZone = zone.getAttribute("data-zone") as DockZone
                    if (tabGroupId && dockZone) {
                        return { tabGroupId, zone: dockZone }
                    }
                }
            }
            return null
        },
        []
    )

    const onTitleMouseDown = useCallback(
        (e: React.MouseEvent) => {
            if (e.button !== 0) return
            e.preventDefault()
            dragging.current = true
            offset.current = { x: e.clientX - pos.x, y: e.clientY - pos.y }

            setDragState({
                panelId: floating.panelId,
                sourceTabGroupId: null,
                isFloating: true,
                previewX: e.clientX + 16,
                previewY: e.clientY + 16,
            })

            const onMove = (ev: MouseEvent) => {
                if (!dragging.current) return
                setPos({ x: ev.clientX - offset.current.x, y: ev.clientY - offset.current.y })
                setDragState((prev) =>
                    prev.panelId === floating.panelId
                        ? {
                            ...prev,
                            previewX: ev.clientX + 16,
                            previewY: ev.clientY + 16,
                        }
                        : prev
                )
            }

            const onUp = (ev: MouseEvent) => {
                dragging.current = false
                window.removeEventListener("mousemove", onMove)
                window.removeEventListener("mouseup", onUp)

                // Check if we dropped on a dock zone
                const dropTarget = checkForDropZone(ev.clientX, ev.clientY)
                if (dropTarget) {
                    dockFloatingPanel(floating.panelId, dropTarget.tabGroupId, dropTarget.zone)
                } else {
                    // Just update final position
                    setFloatingPosition(
                        floating.panelId,
                        ev.clientX - offset.current.x,
                        ev.clientY - offset.current.y
                    )
                    setDragState({
                        panelId: null,
                        sourceTabGroupId: null,
                        isFloating: false,
                        previewX: 0,
                        previewY: 0,
                    })
                }
            }

            window.addEventListener("mousemove", onMove)
            window.addEventListener("mouseup", onUp)
        },
        [floating.panelId, pos, setDragState, setFloatingPosition, checkForDropZone, dockFloatingPanel]
    )

    const onResizeMouseDown = useCallback(
        (e: React.MouseEvent, edge: string) => {
            e.preventDefault()
            e.stopPropagation()
            resizing.current = {
                edge,
                startX: e.clientX,
                startY: e.clientY,
                startW: size.w,
                startH: size.h,
                startLeft: pos.x,
                startTop: pos.y,
            }

            const onMove = (ev: MouseEvent) => {
                if (!resizing.current) return
                const { edge, startX, startY, startW, startH, startLeft, startTop } = resizing.current
                const dx = ev.clientX - startX
                const dy = ev.clientY - startY

                let newW = startW
                let newH = startH
                let newX = startLeft
                let newY = startTop

                if (edge.includes("e")) newW = Math.max(200, startW + dx)
                if (edge.includes("s")) newH = Math.max(120, startH + dy)
                if (edge.includes("w")) {
                    newW = Math.max(200, startW - dx)
                    newX = startLeft + dx
                }
                if (edge.includes("n")) {
                    newH = Math.max(120, startH - dy)
                    newY = startTop + dy
                }

                setSize({ w: newW, h: newH })
                setPos({ x: newX, y: newY })
            }

            const onUp = () => {
                resizing.current = null
                window.removeEventListener("mousemove", onMove)
                window.removeEventListener("mouseup", onUp)
            }

            window.addEventListener("mousemove", onMove)
            window.addEventListener("mouseup", onUp)
        },
        [size, pos]
    )

    return (
        <div
            className={`fixed z-50 flex flex-col rounded border border-neutral-700 bg-neutral-950 shadow-2xl overflow-hidden ${dragState.panelId === floating.panelId ? "opacity-80 ring-1 ring-sky-400/40" : ""}`}
            style={{ left: pos.x, top: pos.y, width: size.w, height: size.h }}
        >
            {/* Title bar */}
            <div
                className="flex items-center h-8 bg-black border-b border-neutral-800 shrink-0 cursor-grab active:cursor-grabbing select-none px-2 gap-2"
                onMouseDown={onTitleMouseDown}
            >
                <GripHorizontal className="w-3.5 h-3.5 text-neutral-600" />
                <span className="text-xs text-neutral-300 font-medium flex-1">
                    {PANEL_LABELS[floating.panelId]}
                </span>
                <span className="text-[10px] text-neutral-600 mr-1">drag to dock</span>
                <button
                    className="p-0.5 hover:bg-neutral-800 rounded text-neutral-500 hover:text-white"
                    onMouseDown={(e) => e.stopPropagation()}
                    onClick={() => {
                        // Close floating panel (remove from layout)
                    }}
                >
                    <X className="w-3.5 h-3.5" />
                </button>
            </div>

            {/* Content */}
            <div className="flex-1 overflow-hidden relative">
                <PanelContent panelId={floating.panelId} />
            </div>

            {/* Resize handles */}
            <div className="absolute inset-0 pointer-events-none">
                {/* edges */}
                <div
                    className="absolute top-0 left-2 right-2 h-1 cursor-n-resize pointer-events-auto"
                    onMouseDown={(e) => onResizeMouseDown(e, "n")}
                />
                <div
                    className="absolute bottom-0 left-2 right-2 h-1 cursor-s-resize pointer-events-auto"
                    onMouseDown={(e) => onResizeMouseDown(e, "s")}
                />
                <div
                    className="absolute left-0 top-2 bottom-2 w-1 cursor-w-resize pointer-events-auto"
                    onMouseDown={(e) => onResizeMouseDown(e, "w")}
                />
                <div
                    className="absolute right-0 top-2 bottom-2 w-1 cursor-e-resize pointer-events-auto"
                    onMouseDown={(e) => onResizeMouseDown(e, "e")}
                />
                {/* corners */}
                <div
                    className="absolute top-0 left-0 w-3 h-3 cursor-nw-resize pointer-events-auto"
                    onMouseDown={(e) => onResizeMouseDown(e, "nw")}
                />
                <div
                    className="absolute top-0 right-0 w-3 h-3 cursor-ne-resize pointer-events-auto"
                    onMouseDown={(e) => onResizeMouseDown(e, "ne")}
                />
                <div
                    className="absolute bottom-0 left-0 w-3 h-3 cursor-sw-resize pointer-events-auto"
                    onMouseDown={(e) => onResizeMouseDown(e, "sw")}
                />
                <div
                    className="absolute bottom-0 right-0 w-3 h-3 cursor-se-resize pointer-events-auto"
                    onMouseDown={(e) => onResizeMouseDown(e, "se")}
                />
            </div>
        </div>
    )
}
