"use client"

import { Lock, Settings, Unlock } from "lucide-react"
import { useEffect, useState } from "react"
import {
  useRemoteViewport,
  type SessionObjectDetails,
  type SessionObjectTransformUpdate,
} from "./remote-viewport-context"

type DraftTransform = {
  location: [string, string, string]
  rotationDegrees: [string, string, string]
  scale: [string, string, string]
}

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
  const { participants, updateTransform } = useRemoteViewport()
  const [draft, setDraft] = useState<DraftTransform>(() => toDraft(details))
  const [isSaving, setIsSaving] = useState(false)

  useEffect(() => {
    setDraft(toDraft(details))
  }, [details])

  const canEdit = details.capabilities.supportsTransform && !details.capabilities.transformReadOnly
  const selectedByNames = details.collaboration.selectedByUserIds.map((userId) => {
    const collaborator = participants.find((entry) => entry.userId === userId)
    return collaborator?.displayName ?? `User ${userId}`
  })
  const lockOwnerName =
    details.collaboration.lockOwnerUserId !== null
      ? participants.find((entry) => entry.userId === details.collaboration.lockOwnerUserId)
          ?.displayName ?? `User ${details.collaboration.lockOwnerUserId}`
      : "None"

  async function applyTransform() {
    const parsed = parseDraft(details.objectId, draft)
    if (!parsed) {
      return
    }

    setIsSaving(true)
    try {
      await updateTransform(parsed)
    } finally {
      setIsSaving(false)
    }
  }

  return (
    <div className="space-y-4 p-3">
      <section className="rounded border border-neutral-800 bg-neutral-950/60 p-3">
        <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">Identity</p>
        <div className="mt-3 space-y-2">
          <DetailRow label="Name" value={details.displayName} />
          <DetailRow label="Object ID" value={details.objectId} />
          <DetailRow label="Type" value={details.kind} />
          <DetailRow label="Visible" value={details.visible ? "true" : "false"} />
          <DetailRow
            label="Selected By"
            value={selectedByNames.length > 0 ? selectedByNames.join(", ") : "Nobody"}
          />
          <DetailRow label="Lock State" value={details.collaboration.lockState} />
          <DetailRow label="Lock Owner" value={lockOwnerName} />
        </div>
      </section>

      <section className="rounded border border-neutral-800 bg-neutral-950/60 p-3">
        <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">Transform</p>
        {details.capabilities.supportsTransform && details.transform ? (
          <div className="mt-3 space-y-3">
            <VectorEditor
              disabled={!canEdit || isSaving}
              label="Location"
              value={draft.location}
              onChange={(value) => setDraft((current) => ({ ...current, location: value }))}
            />
            <VectorEditor
              disabled={!canEdit || isSaving}
              label="Rotation"
              value={draft.rotationDegrees}
              onChange={(value) =>
                setDraft((current) => ({ ...current, rotationDegrees: value }))
              }
            />
            <VectorEditor
              disabled={!canEdit || isSaving}
              label="Scale"
              value={draft.scale}
              onChange={(value) => setDraft((current) => ({ ...current, scale: value }))}
            />
            <DetailRow label="Editability" value={canEdit ? "Editable" : "Read-only"} />
            {canEdit && (
              <div className="flex justify-end">
                <button
                  className="rounded border border-neutral-700 bg-neutral-900 px-3 py-1.5 text-xs text-neutral-200 hover:border-neutral-600 hover:bg-neutral-800 disabled:cursor-not-allowed disabled:opacity-50"
                  disabled={isSaving}
                  onClick={() => void applyTransform()}
                  type="button"
                >
                  {isSaving ? "Applying..." : "Apply Transform"}
                </button>
              </div>
            )}
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

function VectorEditor({
  disabled,
  label,
  onChange,
  value,
}: {
  disabled: boolean
  label: string
  onChange: (value: [string, string, string]) => void
  value: [string, string, string]
}) {
  return (
    <div className="flex items-start gap-3">
      <span className="w-20 shrink-0 pt-2 text-xs text-neutral-500">{label}</span>
      <div className="grid min-w-0 flex-1 grid-cols-3 gap-2">
        {(["X", "Y", "Z"] as const).map((axis, index) => (
          <label key={axis} className="space-y-1">
            <span className="block text-[10px] uppercase tracking-[0.14em] text-neutral-600">
              {axis}
            </span>
            <input
              className="w-full rounded border border-neutral-800 bg-neutral-900 px-2 py-1 text-xs text-neutral-300 outline-none focus:border-neutral-600 disabled:cursor-not-allowed disabled:opacity-50"
              disabled={disabled}
              onChange={(event) => {
                const next: [string, string, string] = [...value] as [string, string, string]
                next[index] = event.target.value
                onChange(next)
              }}
              type="number"
              value={value[index]}
            />
          </label>
        ))}
      </div>
    </div>
  )
}

function EmptyState({ text }: { text: string }) {
  return (
    <div className="flex h-full items-center justify-center px-4 text-center text-xs text-neutral-600">
      {text}
    </div>
  )
}

function toDraft(details: SessionObjectDetails): DraftTransform {
  return {
    location: toStringVec3(details.transform?.location ?? [0, 0, 0]),
    rotationDegrees: toStringVec3(details.transform?.rotationDegrees ?? [0, 0, 0]),
    scale: toStringVec3(details.transform?.scale ?? [1, 1, 1]),
  }
}

function toStringVec3(value: [number, number, number]): [string, string, string] {
  return [String(value[0]), String(value[1]), String(value[2])]
}

function parseDraft(
  objectId: string,
  draft: DraftTransform
): SessionObjectTransformUpdate | null {
  const location = parseVec3(draft.location)
  const rotationDegrees = parseVec3(draft.rotationDegrees)
  const scale = parseVec3(draft.scale)
  if (!location || !rotationDegrees || !scale) {
    return null
  }

  return {
    objectId,
    location,
    rotationDegrees,
    scale,
  }
}

function parseVec3(value: [string, string, string]): [number, number, number] | null {
  const parsed = value.map((entry) => Number(entry.trim()))
  if (parsed.some((entry) => Number.isNaN(entry) || !Number.isFinite(entry))) {
    return null
  }

  return [parsed[0], parsed[1], parsed[2]]
}
