"use client"

import { useState } from "react"
import { MenuBar } from "./engine/menu-bar"
import { Toolbar } from "./engine/toolbar"
import { Viewport } from "./engine/viewport"
import { Outliner } from "./engine/outliner"
import { Details } from "./engine/details"
import { ContentBrowser } from "./engine/content-browser"

export function WraithEngine() {
  const [selectedItem, setSelectedItem] = useState<string | null>("PlayerCharacter")

  return (
    <div className="flex flex-col h-screen bg-black text-white overflow-hidden">
      <MenuBar />
      <Toolbar />
      <div className="flex flex-1 overflow-hidden">
        <div className="flex flex-col flex-1">
          <Viewport />
          <ContentBrowser />
        </div>
        <div className="w-80 flex flex-col border-l border-neutral-800">
          <Outliner selectedItem={selectedItem} onSelectItem={setSelectedItem} />
          <Details selectedItem={selectedItem} />
        </div>
      </div>
    </div>
  )
}
