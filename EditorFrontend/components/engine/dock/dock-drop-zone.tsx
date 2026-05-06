"use client"

import { useState } from "react"
import { useDock, type DockZone, type PanelId } from "./dock-context"

interface DockDropZoneProps {
    tabGroupId: string
    zone: DockZone
    isFloatingDrop?: boolean
}

const zoneStyles: Record<DockZone, string> = {
    left: "absolute left-0 top-0 bottom-0 w-1/4",
    right: "absolute right-0 top-0 bottom-0 w-1/4",
    top: "absolute top-0 left-0 right-0 h-1/4",
    bottom: "absolute bottom-0 left-0 right-0 h-1/4",
    center: "absolute inset-0",
    tab: "absolute inset-0",
}

const zoneHighlight: Record<DockZone, string> = {
    left: "bg-white/10 border-l-2 border-white/60",
    right: "bg-white/10 border-r-2 border-white/60",
    top: "bg-white/10 border-t-2 border-white/60",
    bottom: "bg-white/10 border-b-2 border-white/60",
    center: "bg-white/10 border-2 border-white/60",
    tab: "bg-white/5 border-2 border-white/40 border-dashed",
}

export function DockDropZone({ tabGroupId, zone, isFloatingDrop }: DockDropZoneProps) {
    const { dragState, dropPanel, dockFloatingPanel } = useDock()
    const [isHovered, setIsHovered] = useState(false)

    if (!dragState.panelId) return null

    const handleDrop = () => {
        const pid = dragState.panelId as PanelId
        if (isFloatingDrop || dragState.isFloating) {
            dockFloatingPanel(pid, tabGroupId, zone)
        } else {
            dropPanel(pid, tabGroupId, zone)
        }
        setIsHovered(false)
    }

    // For floating panel drags, make zones slightly visible so user knows where to drop
    const isFloatingDrag = dragState.isFloating

    return (
        <div
            data-dock-drop-zone
            data-tab-group-id={tabGroupId}
            data-zone={zone}
            className={`${zoneStyles[zone]} z-40 transition-all duration-100 ${isHovered
                    ? zoneHighlight[zone]
                    : isFloatingDrag
                        ? "bg-white/5 border border-dashed border-white/20"
                        : "opacity-0 hover:opacity-100"
                }`}
            onMouseEnter={() => setIsHovered(true)}
            onMouseLeave={() => setIsHovered(false)}
            onMouseUp={handleDrop}
        >
            {(isHovered || isFloatingDrag) && (
                <div className="absolute inset-0 flex items-center justify-center pointer-events-none">
                    <span className="text-[10px] text-white/70 font-medium uppercase tracking-widest bg-black/60 px-2 py-0.5 rounded">
                        {zone}
                    </span>
                </div>
            )}
        </div>
    )
}
