"use client"

import { useCallback } from "react"
import { GripHorizontal } from "lucide-react"
import { useDock, type PanelId, type TabGroup } from "./dock-context"
import { DockDropZone } from "./dock-drop-zone"
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
}

export function DockPanel({ tabGroup, className = "", style }: DockPanelProps) {
    const { dragState, setDragState, floatPanel, setActiveTab } = useDock()
    // Show drop zones when ANY panel is being dragged (docked OR floating)
    const isDragging = dragState.panelId !== null

    const handleTabMouseDown = useCallback(
        (e: React.MouseEvent, panelId: PanelId) => {
            if (e.button !== 0) return
            e.preventDefault()
            setDragState({ panelId, sourceTabGroupId: tabGroup.id, isFloating: false })

            const onMouseUp = (ev: MouseEvent) => {
                window.removeEventListener("mouseup", onMouseUp)
                // If we haven't dropped on a zone, float it
                setDragState((prev: any) => {
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

    return (
        <div
            className={`flex flex-col overflow-hidden bg-neutral-950 ${className}`}
            style={style}
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
                            onMouseDown={(e) => {
                                if (tabGroup.panels.length === 1) {
                                    handleTabMouseDown(e, panelId)
                                }
                            }}
                        >
                            <GripHorizontal
                                className="w-3 h-3 opacity-40 cursor-grab"
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

            {/* Panel content */}
            <div className="flex-1 overflow-hidden relative">
                <PanelContent panelId={tabGroup.activePanel} />

                {/* Drop zones — shown whenever any drag is in progress */}
                {isDragging && dragState.panelId !== tabGroup.activePanel && (
                    <>
                        <DockDropZone tabGroupId={tabGroup.id} zone="left" />
                        <DockDropZone tabGroupId={tabGroup.id} zone="right" />
                        <DockDropZone tabGroupId={tabGroup.id} zone="top" />
                        <DockDropZone tabGroupId={tabGroup.id} zone="bottom" />
                        <DockDropZone tabGroupId={tabGroup.id} zone="tab" />
                    </>
                )}
            </div>
        </div>
    )
}
