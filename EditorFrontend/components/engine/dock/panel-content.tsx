"use client"

import { type PanelId } from "./dock-context"
import { Viewport } from "../viewport"
import { Outliner } from "../outliner"
import { Details } from "../details"
import { ContentBrowser } from "../content-browser"
import { RemoteViewportPanel } from "../remote-viewport-panel"
import { ScriptEditor } from "../script-editor"
import { WorldDetailsPanel } from "../../panels/world-details-panel"
import { PlaceActorsPanel } from "../../panels/place-actors-panel"

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
        case "script-editor":
            return <ScriptEditor />
        case "world-details":
            return <WorldDetailsPanel />
        case "place-actors":
            return <PlaceActorsPanel />
        default:
            return null
    }
}
