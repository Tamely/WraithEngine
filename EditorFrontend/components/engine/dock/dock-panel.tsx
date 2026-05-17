"use client"

import { useCallback, useState } from "react"
import { GripHorizontal } from "lucide-react"
import { useDock, type PanelId, type TabGroup } from "./dock-context"
import { DockDropZones } from "./dock-drop-zone"
import { PanelContent } from "./panel-content"

interface DockPanelProps {
    tabGroup: TabGroup
    className?: string
    style?: React.CSSProperties
}

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

export function DockPanel({ tabGroup, className = "", style }: DockPanelProps) {
    const { dragState, setDragState, floatPanel, setActiveTab } = useDock()
    const [isPanelHovered, setIsPanelHovered] = useState(false)

    const isDragging = dragState.panelId !== null
    const isSourceGroup = dragState.sourceTabGroupId === tabGroup.id
    const showZones = isDragging && (!isSourceGroup || dragState.isFloating || isPanelHovered)

    const handleTabMouseDown = useCallback(
        (e: React.MouseEvent, panelId: PanelId) => {
            if (e.button !== 0) return
            e.preventDefault()
            const startX = e.clientX
            const startY = e.clientY
            let didMove = false

            setDragState({
                panelId,
                sourceTabGroupId: tabGroup.id,
                isFloating: false,
                previewX: e.clientX + 16,
                previewY: e.clientY + 16,
            })

            const onMouseMove = (ev: MouseEvent) => {
                const deltaX = ev.clientX - startX
                const deltaY = ev.clientY - startY
                if (!didMove && Math.hypot(deltaX, deltaY) > 4) {
                    didMove = true
                }
                setDragState((prev) =>
                    prev.panelId === panelId
                        ? {
                            ...prev,
                            previewX: ev.clientX + 16,
                            previewY: ev.clientY + 16,
                        }
                        : prev
                )
            }

            const onMouseUp = (ev: MouseEvent) => {
                window.removeEventListener("mousemove", onMouseMove)
                window.removeEventListener("mouseup", onMouseUp)
                setDragState((prev) => {
                    if (prev.panelId === panelId) {
                        if (didMove) {
                            floatPanel(panelId, ev.clientX - 140, ev.clientY - 18)
                        }
                        return {
                            panelId: null,
                            sourceTabGroupId: null,
                            isFloating: false,
                            previewX: 0,
                            previewY: 0,
                        }
                    }
                    return prev
                })
            }
            window.addEventListener("mousemove", onMouseMove)
            window.addEventListener("mouseup", onMouseUp)
        },
        [tabGroup.id, setDragState, floatPanel]
    )

    const handleDropZone = useCallback(() => {
        // dragState cleared by the zone handler; just ensure hover resets
        setIsPanelHovered(false)
    }, [])

    return (
        <div
            className={`flex flex-col overflow-hidden bg-neutral-950 ${className}`}
            style={style}
            onMouseEnter={() => setIsPanelHovered(true)}
            onMouseLeave={() => setIsPanelHovered(false)}
        >
            {/* Tab bar */}
            <div className="flex items-center h-8 bg-black border-b border-neutral-800 shrink-0 select-none">
                {tabGroup.panels.map((panelId) => {
                    const isActive = tabGroup.activePanel === panelId
                    return (
                        <div
                            key={panelId}
                            className={`flex items-center gap-1.5 h-full px-3 border-r border-neutral-800 cursor-pointer text-xs transition-colors
                ${isActive
                                    ? "bg-neutral-900 text-white border-t border-t-white"
                                    : "text-neutral-500 hover:text-neutral-300 hover:bg-neutral-900/50"
                                }`}
                            onClick={() => setActiveTab(tabGroup.id, panelId)}
                        >
                            <GripHorizontal
                                className="w-3 h-3 opacity-40 cursor-grab active:cursor-grabbing shrink-0"
                                onMouseDown={(e) => {
                                    e.stopPropagation()
                                    handleTabMouseDown(e, panelId)
                                }}
                            />
                            <span>{PANEL_LABELS[panelId]}</span>
                        </div>
                    )
                })}
                <div className="flex-1" />
            </div>

            {/* Panel content + drop zones */}
            <div className="flex-1 overflow-hidden relative">
                <PanelContent panelId={tabGroup.activePanel} />

                {showZones && (
                    <DockDropZones
                        tabGroupId={tabGroup.id}
                        isFloating={dragState.isFloating}
                        onDropZone={handleDropZone}
                    />
                )}
            </div>
        </div>
    )
}
