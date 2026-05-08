"use client"

import { useRemoteViewport, type SessionParticipant } from "./remote-viewport-context"

function presenceDotColor(state: SessionParticipant["presenceState"]): string {
  switch (state) {
    case "connected":
      return "#22C55E"
    case "away":
      return "#F59E0B"
    case "disconnected":
      return "#6B7280"
  }
}

function presenceLabel(state: SessionParticipant["presenceState"]): string {
  switch (state) {
    case "connected":
      return "Connected"
    case "away":
      return "Away"
    case "disconnected":
      return "Disconnected"
  }
}

export function PresenceRoster() {
  const { participants } = useRemoteViewport()

  const visible = participants.filter((p) => p.presenceState !== "disconnected")

  if (visible.length === 0) {
    return null
  }

  return (
    <div className="flex items-center gap-1">
      {visible.map((participant) => (
        <span
          key={participant.userId}
          className="relative flex h-6 w-6 items-center justify-center rounded-full text-[10px] font-bold text-white"
          style={{ backgroundColor: participant.presentationColor }}
          title={`${participant.displayName} — ${presenceLabel(participant.presenceState)}`}
        >
          {participant.displayName.charAt(0).toUpperCase()}
          <span
            className="absolute -bottom-0.5 -right-0.5 h-2 w-2 rounded-full ring-1 ring-neutral-950"
            style={{ backgroundColor: presenceDotColor(participant.presenceState) }}
          />
        </span>
      ))}
    </div>
  )
}
