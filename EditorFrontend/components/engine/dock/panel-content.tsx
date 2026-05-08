"use client"

import { type PanelId } from "./dock-context"
import { Viewport } from "../viewport"
import { Outliner } from "../outliner"
import { Details } from "../details"
import { ContentBrowser } from "../content-browser"
import { RemoteViewportPanel } from "../remote-viewport-panel"

interface PanelContentProps {
    panelId: PanelId
}

export function PanelContent({ panelId }: PanelContentProps) {
    switch (panelId) {
        case "viewport":
            return <Viewport />
        case "outliner":
            return <Outliner />
        case "details":
            return <Details />
        case "content-browser":
            return <ContentBrowser />
        case "remote-viewport":
            return <RemoteViewportPanel />
        default:
            return null
    }
}
