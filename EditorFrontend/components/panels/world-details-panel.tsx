"use client"

import { useRemoteViewport } from "@/components/engine/remote-viewport-context"
import { HexColorPicker } from "react-colorful"
import { useEffect, useState } from "react"
import { Globe } from "lucide-react"
import { Popover, PopoverContent, PopoverTrigger } from "@/components/ui/popover"

function vec3ToHex(vec: [number, number, number]): string {
  const toHex = (n: number) => {
    const hex = Math.round(Math.max(0, Math.min(1, n)) * 255).toString(16)
    return hex.length === 1 ? "0" + hex : hex
  }
  return `#${toHex(vec[0])}${toHex(vec[1])}${toHex(vec[2])}`
}

function hexToVec3(hex: string): [number, number, number] {
  const r = parseInt(hex.slice(1, 3), 16) / 255
  const g = parseInt(hex.slice(3, 5), 16) / 255
  const b = parseInt(hex.slice(5, 7), 16) / 255
  return [r, g, b]
}

export function WorldDetailsPanel() {
  const { worldSettings, setWorldSettings } = useRemoteViewport()

  // Derive primitive remote values once so effect deps compare by value, not
  // by the worldSettings object reference (which is a new object on every
  // session-snapshot refresh).
  const remoteTopHex = vec3ToHex(worldSettings.skyboxColorTop)
  const remoteBottomHex = vec3ToHex(worldSettings.skyboxColorBottom)
  const remoteHdrPath = worldSettings.skyboxHDRPath

  const [topColorHex, setTopColorHex] = useState(remoteTopHex)
  const [bottomColorHex, setBottomColorHex] = useState(remoteBottomHex)
  const [hdrPath, setHdrPath] = useState(remoteHdrPath)

  // Only adopt the remote value when the underlying string actually changes;
  // typing into the input no longer gets clobbered by unrelated snapshot
  // refreshes.
  useEffect(() => {
    setTopColorHex(remoteTopHex)
  }, [remoteTopHex])
  useEffect(() => {
    setBottomColorHex(remoteBottomHex)
  }, [remoteBottomHex])
  useEffect(() => {
    setHdrPath(remoteHdrPath)
  }, [remoteHdrPath])

  const handleTopColorChange = (next: string) => {
    setTopColorHex(next)
    setWorldSettings(hexToVec3(next), hexToVec3(bottomColorHex), hdrPath)
  }

  const handleBottomColorChange = (next: string) => {
    setBottomColorHex(next)
    setWorldSettings(hexToVec3(topColorHex), hexToVec3(next), hdrPath)
  }

  const commitHdrPath = (next: string) => {
    if (next === remoteHdrPath) return
    setWorldSettings(hexToVec3(topColorHex), hexToVec3(bottomColorHex), next)
  }

  const hdrActive = hdrPath.trim().length > 0

  return (
    <div className="flex h-full flex-col overflow-hidden">
      <div className="flex h-8 items-center justify-between border-b border-neutral-800 bg-neutral-950 px-2">
        <div className="flex items-center gap-1">
          <Globe className="h-4 w-4 text-neutral-400" />
          <span className="text-xs font-medium text-neutral-300">World Details</span>
        </div>
      </div>

      <div className="flex-1 overflow-y-auto">
        <div className="flex flex-col gap-4 p-3">
          <section className="flex flex-col gap-2">
            <h3 className="text-[11px] font-semibold uppercase tracking-wide text-neutral-400">
              Environment
            </h3>

            <div
              className={`flex flex-col gap-2 ${hdrActive ? "pointer-events-none opacity-40" : ""}`}
            >
              <ColorRow
                label="Sky Top"
                value={topColorHex}
                onChange={handleTopColorChange}
                popoverTitle="Skybox Top Color"
              />
              <ColorRow
                label="Sky Bottom"
                value={bottomColorHex}
                onChange={handleBottomColorChange}
                popoverTitle="Skybox Bottom Color"
              />
            </div>

            <div className="mt-2 grid grid-cols-[80px_1fr] items-center gap-2">
              <span className="text-[11px] uppercase tracking-wide text-neutral-500">
                HDR Sky
              </span>
              <input
                type="text"
                placeholder="Textures/studio.hdr"
                className="h-7 w-full rounded-sm border border-neutral-800 bg-neutral-900 px-2 text-xs text-neutral-200 placeholder:text-neutral-600 focus:border-neutral-600 focus:outline-none"
                value={hdrPath}
                onChange={(e) => setHdrPath(e.target.value)}
                onBlur={(e) => commitHdrPath(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === "Enter") {
                    commitHdrPath((e.target as HTMLInputElement).value)
                    ;(e.target as HTMLInputElement).blur()
                  } else if (e.key === "Escape") {
                    setHdrPath(remoteHdrPath)
                    ;(e.target as HTMLInputElement).blur()
                  }
                }}
              />
            </div>
            <p className="pl-[88px] pr-1 text-[10px] leading-snug text-neutral-500">
              Content-relative path to an equirectangular .hdr (or cooked .wtex). Leave empty
              to use the gradient colors.
            </p>
          </section>
        </div>
      </div>
    </div>
  )
}

function ColorRow({
  label,
  value,
  onChange,
  popoverTitle,
}: {
  label: string
  value: string
  onChange: (next: string) => void
  popoverTitle: string
}) {
  return (
    <div className="grid grid-cols-[80px_1fr] items-center gap-2">
      <span className="text-[11px] uppercase tracking-wide text-neutral-500">{label}</span>
      <Popover>
        <PopoverTrigger asChild>
          <button
            type="button"
            className="flex h-7 w-full items-center rounded-sm border border-neutral-800 bg-neutral-900 px-1.5 transition-colors hover:border-neutral-700"
          >
            <div
              className="h-full w-full rounded-[2px] border border-neutral-800"
              style={{ backgroundColor: value }}
            />
          </button>
        </PopoverTrigger>
        <PopoverContent className="w-auto border-neutral-800 bg-neutral-950 p-3" side="left">
          <div className="flex flex-col gap-3">
            <div className="text-xs font-medium text-neutral-300">{popoverTitle}</div>
            <HexColorPicker color={value} onChange={onChange} />
            <div className="flex items-center gap-2">
              <span className="w-10 font-mono text-[10px] text-neutral-500">HEX</span>
              <input
                type="text"
                className="h-7 w-full rounded-sm border border-neutral-800 bg-neutral-900 px-2 font-mono text-xs text-neutral-200 focus:border-neutral-600 focus:outline-none"
                value={value}
                onChange={(e) => onChange(e.target.value)}
              />
            </div>
          </div>
        </PopoverContent>
      </Popover>
    </div>
  )
}
