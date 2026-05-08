"use client"

import { MenuBar } from "./engine/menu-bar"
import { Toolbar } from "./engine/toolbar"
import { DockProvider } from "./engine/dock/dock-context"
import { DockLayout } from "./engine/dock/dock-layout"
import { RemoteViewportProvider } from "./engine/remote-viewport-context"

export function WraithEngine() {
  return (
    <RemoteViewportProvider>
      <DockProvider>
        <div className="flex flex-col h-screen bg-black text-white overflow-hidden">
          <MenuBar />
          <Toolbar />
          <DockLayout />
        </div>
      </DockProvider>
    </RemoteViewportProvider>
  )
}
