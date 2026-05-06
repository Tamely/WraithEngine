"use client"

import { Settings, Lock, Unlock } from "lucide-react"
import { useState } from "react"
import { useRemoteViewport } from "./remote-viewport-context"

export function Details() {
  const { selectedObject } = useRemoteViewport()
  const [isLocked, setIsLocked] = useState(false)

  return (
    <div className="flex flex-1 flex-col overflow-hidden">
      <div className="flex h-8 items-center justify-between border-b border-neutral-800 bg-neutral-950 px-2">
        <div className="flex items-center gap-1">
          <Settings className="h-4 w-4 text-neutral-400" />
          <span className="text-xs font-medium text-neutral-300">Details</span>
        </div>
        <button
          className="rounded p-1 hover:bg-neutral-800"
          onClick={() => setIsLocked(!isLocked)}
          type="button"
        >
          {isLocked ? (
            <Lock className="h-3.5 w-3.5 text-neutral-400" />
          ) : (
            <Unlock className="h-3.5 w-3.5 text-neutral-500" />
          )}
        </button>
      </div>
      <div className="flex-1 overflow-y-auto">
        {selectedObject ? (
          <div className="space-y-4 p-3">
            <section className="rounded border border-neutral-800 bg-neutral-950/60 p-3">
              <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">
                Identity
              </p>
              <div className="mt-3 space-y-2">
                <DetailRow label="Name" value={selectedObject.displayName} />
                <DetailRow label="Object ID" value={selectedObject.id} />
                <DetailRow label="Type" value={selectedObject.kind} />
                <DetailRow label="Visible" value={selectedObject.visible ? "true" : "false"} />
              </div>
            </section>
          </div>
        ) : (
          <div className="flex h-full items-center justify-center text-xs text-neutral-600">
            Select an object to view details
          </div>
        )}
      </div>
    </div>
  )
}

function DetailRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-center gap-3">
      <span className="w-20 shrink-0 text-xs text-neutral-500">{label}</span>
      <div className="min-w-0 flex-1 rounded border border-neutral-800 bg-neutral-900 px-2 py-1 text-xs text-neutral-300">
        {value}
      </div>
    </div>
  )
}
