"use client"

import { useState } from "react"
import { MenuBar } from "./engine/menu-bar"
import { Toolbar } from "./engine/toolbar"
import { Viewport } from "./engine/viewport"
import { Outliner } from "./engine/outliner"
import { Details } from "./engine/details"
import { ContentBrowser } from "./engine/content-browser"
import { ResizeHandle } from "./engine/resize-handle"
import { useResize } from "@/hooks/use-resize"

export function WraithEngine() {
  const [selectedItem, setSelectedItem] = useState<string | null>("PlayerCharacter")

  // Right panel width (horizontal drag)
  const rightPanel = useResize({
    direction: "horizontal",
    initialSize: 320,
    min: 180,
    max: 600,
    reverse: true,
  })

  // Content browser height (vertical drag, from bottom)
  const contentBrowser = useResize({
    direction: "vertical",
    initialSize: 192,
    min: 80,
    max: 480,
    reverse: true,
  })

  // Outliner vs Details split inside the right panel (vertical)
  const outlinerHeight = useResize({
    direction: "vertical",
    initialSize: 260,
    min: 80,
    max: 560,
  })

  return (
    <div className="flex flex-col h-screen bg-black text-white overflow-hidden">
      <MenuBar />
      <Toolbar />

      <div className="flex flex-1 overflow-hidden">

        <div className="flex flex-col flex-1 overflow-hidden">
          <div className="flex-1 overflow-hidden" style={{ minHeight: 0 }}>
            <Viewport />
          </div>

          <ResizeHandle direction="vertical" onMouseDown={contentBrowser.onMouseDown} />

          <div style={{ height: contentBrowser.size }} className="shrink-0 overflow-hidden">
            <ContentBrowser />
          </div>
        </div>
        <ResizeHandle direction="horizontal" onMouseDown={rightPanel.onMouseDown} />
        <div
          style={{ width: rightPanel.size }}
          className="flex flex-col shrink-0 border-l border-neutral-800 overflow-hidden"
        >
          <div style={{ height: outlinerHeight.size }} className="shrink-0 overflow-hidden">
            <Outliner selectedItem={selectedItem} onSelectItem={setSelectedItem} />
          </div>
          <ResizeHandle direction="vertical" onMouseDown={outlinerHeight.onMouseDown} />
          <div className="flex-1 overflow-hidden" style={{ minHeight: 0 }}>
            <Details selectedItem={selectedItem} />
          </div>
        </div>

      </div>
    </div>
  )
}
