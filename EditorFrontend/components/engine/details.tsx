"use client"

import { Lock, Settings, Unlock } from "lucide-react"
import { useState } from "react"
import { useRemoteViewport, type SessionObjectDetails } from "./remote-viewport-context"

export function Details() {
  const { selectedObjectDetails, selectedObjectId } = useRemoteViewport()
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
        {!selectedObjectId ? (
          <EmptyState text="Select an object to view details" />
        ) : !selectedObjectDetails ? (
          <EmptyState text="No authoritative details are available for this object yet" />
        ) : (
          <DetailsContent details={selectedObjectDetails} />
        )}
      </div>
    </div>
  )
}

function DetailsContent({ details }: { details: SessionObjectDetails }) {
  return (
    <div className="space-y-4 p-3">
      <section className="rounded border border-neutral-800 bg-neutral-950/60 p-3">
        <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">Identity</p>
        <div className="mt-3 space-y-2">
          <DetailRow label="Name" value={details.displayName} />
          <DetailRow label="Object ID" value={details.objectId} />
          <DetailRow label="Type" value={details.kind} />
          <DetailRow label="Visible" value={details.visible ? "true" : "false"} />
        </div>
      </section>

      <section className="rounded border border-neutral-800 bg-neutral-950/60 p-3">
        <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">Transform</p>
        {details.capabilities.supportsTransform && details.transform ? (
          <div className="mt-3 space-y-2">
            <DetailRow
              label="Location"
              value={formatVec3(details.transform.location)}
            />
            <DetailRow
              label="Rotation"
              value={formatVec3(details.transform.rotationDegrees)}
            />
            <DetailRow label="Scale" value={formatVec3(details.transform.scale)} />
            <DetailRow
              label="Editability"
              value={details.capabilities.transformReadOnly ? "Read-only" : "Editable"}
            />
          </div>
        ) : (
          <p className="mt-3 text-xs text-neutral-500">
            This object does not expose authoritative transform details yet.
          </p>
        )}
      </section>
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

function EmptyState({ text }: { text: string }) {
  return <div className="flex h-full items-center justify-center px-4 text-center text-xs text-neutral-600">{text}</div>
}

function formatVec3(value: [number, number, number]) {
  return `X: ${value[0].toFixed(2)}, Y: ${value[1].toFixed(2)}, Z: ${value[2].toFixed(2)}`
}
