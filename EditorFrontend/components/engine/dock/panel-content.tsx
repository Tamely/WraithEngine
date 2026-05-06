"use client"

import { useState } from "react"
import { type PanelId } from "./dock-context"
import { Viewport } from "../viewport"
import { Outliner } from "../outliner"
import { Details } from "../details"
import { ContentBrowser } from "../content-browser"
import { RemoteViewportPanel } from "../remote-viewport-panel"

// Shared selection state lives here so Outliner → Details stays in sync
// even when they're in different dock positions.
let sharedSelected: string | null = "PlayerCharacter"
const listeners = new Set<() => void>()
function useSharedSelection(): [string | null, (v: string | null) => void] {
    const [, forceRender] = useState(0)
    // subscribe
    if (!listeners.has(forceRender as any)) {
        listeners.add(forceRender as any)
    }
    const set = (v: string | null) => {
        sharedSelected = v
        listeners.forEach((fn) => (fn as any)((c: number) => c + 1))
    }
    return [sharedSelected, set]
}

interface PanelContentProps {
    panelId: PanelId
}

export function PanelContent({ panelId }: PanelContentProps) {
    const [selected, setSelected] = useSharedSelection()

    switch (panelId) {
        case "viewport":
            return <Viewport />
        case "outliner":
            return <Outliner selectedItem={selected} onSelectItem={setSelected} />
        case "details":
            return <Details selectedItem={selected} />
        case "content-browser":
            return <ContentBrowser />
        case "remote-viewport":
            return <RemoteViewportPanel />
        default:
            return null
    }
}
