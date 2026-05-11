"use client"

import { X } from "lucide-react"
import { MenuBar } from "./engine/menu-bar"
import { Toolbar } from "./engine/toolbar"
import { DockProvider } from "./engine/dock/dock-context"
import { DockLayout } from "./engine/dock/dock-layout"
import {
  RemoteViewportProvider,
  useRemoteViewport,
} from "./engine/remote-viewport-context"

function ScriptErrorToastOverlay() {
  const { scriptErrorToasts, dismissScriptErrorToast } = useRemoteViewport()
  if (scriptErrorToasts.length === 0) return null

  return (
    <div className="pointer-events-none fixed bottom-4 right-4 z-50 flex flex-col gap-2">
      {scriptErrorToasts.map((toast) => (
        <div
          key={toast.id}
          className="pointer-events-auto flex max-w-sm items-start gap-2 rounded border border-red-800 bg-neutral-950 px-3 py-2.5 shadow-lg"
        >
          <div className="min-w-0 flex-1">
            <p className="text-xs font-medium text-red-400">Script Error</p>
            <p className="mt-0.5 text-[11px] text-neutral-400 truncate">{toast.objectId}</p>
            <p className="mt-1 text-xs text-neutral-300 line-clamp-3">{toast.message}</p>
          </div>
          <button
            className="shrink-0 rounded p-0.5 text-neutral-500 hover:bg-neutral-800 hover:text-neutral-300"
            onClick={() => dismissScriptErrorToast(toast.id)}
            type="button"
          >
            <X className="h-3.5 w-3.5" />
          </button>
        </div>
      ))}
    </div>
  )
}

export function WraithEngine() {
  return (
    <RemoteViewportProvider>
      <DockProvider>
        <div className="relative flex flex-col h-screen bg-black text-white overflow-hidden">
          <MenuBar />
          <Toolbar />
          <DockLayout />
          <ScriptErrorToastOverlay />
        </div>
      </DockProvider>
    </RemoteViewportProvider>
  )
}
