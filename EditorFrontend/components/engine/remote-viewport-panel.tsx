"use client"

import { Camera, ChevronDown, RotateCcw } from "lucide-react"
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu"
import { useRemoteViewport, type RemoteViewportViewMode } from "./remote-viewport-context"

export function RemoteViewportPanel() {
  const {
    detailText,
    eventLog,
    frameText,
    isLooking,
    serverOrigin,
    statusText,
    viewMode,
    clearEventLog,
    reconnect,
    setMode,
    toggleLook,
  } = useRemoteViewport()

  return (
    <div className="h-full overflow-auto bg-neutral-950 p-3 text-xs text-neutral-300">
      <div className="rounded-lg border border-neutral-800 bg-black/65 p-3 backdrop-blur-sm">
        <div className="flex items-center justify-between">
          <div>
            <p className="text-[11px] uppercase tracking-[0.18em] text-neutral-500">
              Remote Viewport
            </p>
            <p className="mt-1 text-sm text-white">{statusText}</p>
          </div>
          <div className="flex items-center gap-1">
            <IconButton icon={RotateCcw} label="Reconnect" onClick={() => void reconnect()} />
            <IconButton
              icon={Camera}
              label={isLooking ? "Disable look" : "Enable look"}
              onClick={() => void toggleLook()}
            />
          </div>
        </div>
        <p className="mt-2 text-neutral-400">{detailText}</p>
        <div className="mt-3 grid grid-cols-2 gap-2 text-[11px] text-neutral-400">
          <div className="rounded border border-neutral-800 bg-neutral-950/70 px-2 py-1.5">
            <span className="block text-neutral-500">View Mode</span>
            <ViewModeDropdown viewMode={viewMode} onChange={setMode} />
          </div>
          <div className="rounded border border-neutral-800 bg-neutral-950/70 px-2 py-1.5">
            <span className="block text-neutral-500">Controls</span>
            <span className="mt-1 block text-neutral-200">WASD + click to look</span>
          </div>
          <div className="rounded border border-neutral-800 bg-neutral-950/70 px-2 py-1.5">
            <span className="block text-neutral-500">Transport</span>
            <span className="mt-1 block text-neutral-200">{frameText}</span>
          </div>
          <div className="rounded border border-neutral-800 bg-neutral-950/70 px-2 py-1.5">
            <span className="block text-neutral-500">Server</span>
            <span className="mt-1 block truncate text-neutral-200">{serverOrigin || "Not set"}</span>
          </div>
        </div>
        <div className="mt-3 rounded border border-neutral-800 bg-neutral-950/70 p-2">
          <div className="mb-2 flex items-center justify-between">
            <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">
              Session Log
            </p>
            <button
              className="text-[11px] text-neutral-500 hover:text-neutral-200"
              onClick={clearEventLog}
              type="button"
            >
              Clear
            </button>
          </div>
          <div className="max-h-72 space-y-1 overflow-auto font-mono text-[10px] text-neutral-400">
            {eventLog.length === 0 ? (
              <p>No events yet</p>
            ) : (
              eventLog.map((line, index) => <p key={`${index}-${line}`}>{line}</p>)
            )}
          </div>
        </div>
      </div>
    </div>
  )
}

function ViewModeDropdown({
  viewMode,
  onChange,
}: {
  viewMode: RemoteViewportViewMode
  onChange: (mode: RemoteViewportViewMode) => Promise<void>
}) {
  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <button
          className="mt-1 flex items-center gap-1 text-left text-neutral-200 hover:text-white"
          type="button"
        >
          {viewMode}
          <ChevronDown className="h-3 w-3" />
        </button>
      </DropdownMenuTrigger>
      <DropdownMenuContent
        align="start"
        className="border-neutral-800 bg-neutral-950 text-neutral-200"
      >
        <DropdownMenuRadioGroup
          value={viewMode}
          onValueChange={(value) => void onChange(value as RemoteViewportViewMode)}
        >
          <DropdownMenuRadioItem value="lit">Lit</DropdownMenuRadioItem>
          <DropdownMenuRadioItem value="unlit">Unlit</DropdownMenuRadioItem>
          <DropdownMenuRadioItem value="wireframe">Wireframe</DropdownMenuRadioItem>
        </DropdownMenuRadioGroup>
      </DropdownMenuContent>
    </DropdownMenu>
  )
}

function IconButton({
  icon: Icon,
  label,
  onClick,
}: {
  icon: typeof Camera
  label: string
  onClick: () => void
}) {
  return (
    <button
      aria-label={label}
      className="rounded border border-neutral-800 bg-neutral-950/70 p-1.5 text-neutral-300 hover:border-neutral-700 hover:text-white"
      onClick={onClick}
      type="button"
    >
      <Icon className="h-3.5 w-3.5" />
    </button>
  )
}
