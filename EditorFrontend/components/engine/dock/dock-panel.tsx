"use client"

import { useCallback, useState } from "react"
import { GripHorizontal } from "lucide-react"
import { useDock, type PanelId, type TabGroup, type DockZone } from "./dock-context"
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
}

export function DockPanel({ tabGroup, className = "", style }: DockPanelProps) {
    const { dragState, setDragState, floatPanel, setActiveTab } = useDock()
    const [isPanelHovered, setIsPanelHovered] = useState(false)

    // A drag is active and the dragged panel is not the active panel in this group
    const isDragging = dragState.panelId !== null && dragState.panelId !== tabGroup.activePanel
    // Only show zones when dragging AND the mouse is over THIS panel
    const showZones = isDragging && isPanelHovered

    const handleTabMouseDown = useCallback(
        (e: React.MouseEvent, panelId: PanelId) => {
            if (e.button !== 0) return
            e.preventDefault()
            setDragState({ panelId, sourceTabGroupId: tabGroup.id, isFloating: false })

            const onMouseUp = (ev: MouseEvent) => {
                window.removeEventListener("mouseup", onMouseUp)
                // If nothing handled the drop, float the panel
                setDragState((prev) => {
                    if (prev.panelId === panelId) {
                        floatPanel(panelId, ev.clientX - 100, ev.clientY - 16)
                        return { panelId: null, sourceTabGroupId: null, isFloating: false }
                    }
                    return prev
                })
            }
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
