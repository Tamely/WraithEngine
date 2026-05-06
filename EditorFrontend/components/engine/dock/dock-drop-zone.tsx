"use client"

import { useDock, type DockZone, type PanelId } from "./dock-context"

interface DockDropZoneProps {
    tabGroupId: string
    zone: DockZone
    onDrop: () => void
}

// Each zone is a distinct non-overlapping region.
// "tab" sits in the center as a small badge — directional zones are half-strips on each edge.
const zoneStyles: Record<DockZone, string> = {
    top: "absolute top-0    left-1/4  right-1/4 h-1/4 cursor-pointer",
    bottom: "absolute bottom-0 left-1/4  right-1/4 h-1/4 cursor-pointer",
    left: "absolute left-0   top-1/4   bottom-1/4 w-1/4 cursor-pointer",
    right: "absolute right-0  top-1/4   bottom-1/4 w-1/4 cursor-pointer",
    tab: "absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-20 h-10 cursor-pointer",
    center: "absolute inset-0 cursor-pointer",
}

// Visual feedback overlays (separate from hit-targets to avoid z conflicts)
const zoneHighlightIdle: Record<DockZone, string> = {
    top: "bg-white/5  border border-dashed border-white/20",
    bottom: "bg-white/5  border border-dashed border-white/20",
    left: "bg-white/5  border border-dashed border-white/20",
    right: "bg-white/5  border border-dashed border-white/20",
    tab: "bg-white/5  border border-dashed border-white/20 rounded",
    center: "",
}

const zoneHighlightHover: Record<DockZone, string> = {
    top: "bg-blue-500/20 border border-blue-400/70 border-t-2",
    bottom: "bg-blue-500/20 border border-blue-400/70 border-b-2",
    left: "bg-blue-500/20 border border-blue-400/70 border-l-2",
    right: "bg-blue-500/20 border border-blue-400/70 border-r-2",
    tab: "bg-blue-500/30 border border-blue-400/70 rounded",
    center: "",
}

interface ZoneProps {
    tabGroupId: string
    zone: DockZone
    isFloating: boolean
    onDropZone: (tabGroupId: string, zone: DockZone) => void
}

function Zone({ tabGroupId, zone, isFloating, onDropZone }: ZoneProps) {
    const { dropPanel, dockFloatingPanel, dragState } = useDock()

    if (!dragState.panelId) return null

    const handleMouseUp = () => {
        const pid = dragState.panelId as PanelId
        if (isFloating) {
            dockFloatingPanel(pid, tabGroupId, zone)
        } else {
            dropPanel(pid, tabGroupId, zone)
        }
        onDropZone(tabGroupId, zone)
    }

    if (zone === "center") return null // center is not exposed to user

    return (
        <div
            data-dock-drop-zone
            data-tab-group-id={tabGroupId}
            data-zone={zone}
            className={`absolute z-50 flex items-center justify-center transition-colors duration-75 select-none group ${zoneStyles[zone]} ${zoneHighlightIdle[zone]} hover:${zoneHighlightHover[zone]}`}
            onMouseUp={handleMouseUp}
        >
            <span className="text-[9px] font-semibold uppercase tracking-widest text-white/30 group-hover:text-white/90 transition-colors pointer-events-none">
                {zone}
            </span>
        </div>
    )
}

export function DockDropZones({
    tabGroupId,
    isFloating,
    onDropZone,
}: {
    tabGroupId: string
    isFloating: boolean
    onDropZone: (tabGroupId: string, zone: DockZone) => void
}) {
    return (
        <>
            <Zone tabGroupId={tabGroupId} zone="top" isFloating={isFloating} onDropZone={onDropZone} />
            <Zone tabGroupId={tabGroupId} zone="bottom" isFloating={isFloating} onDropZone={onDropZone} />
            <Zone tabGroupId={tabGroupId} zone="left" isFloating={isFloating} onDropZone={onDropZone} />
            <Zone tabGroupId={tabGroupId} zone="right" isFloating={isFloating} onDropZone={onDropZone} />
            <Zone tabGroupId={tabGroupId} zone="tab" isFloating={isFloating} onDropZone={onDropZone} />
        </>
    )
}
