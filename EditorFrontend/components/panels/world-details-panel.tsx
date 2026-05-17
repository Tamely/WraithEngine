"use client"

import { useRemoteViewport } from "@/components/engine/remote-viewport-context"
import { HexColorPicker } from "react-colorful"
import { useEffect, useMemo, useState } from "react"
import { Globe, Image as ImageIcon, Palette } from "lucide-react"
import { Popover, PopoverContent, PopoverTrigger } from "@/components/ui/popover"
import { AssetPickerButton, type AssetPickerItem } from "@/components/panels/asset-picker"

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
  const { worldSettings, setWorldSettings, assets, listAssets } = useRemoteViewport()

  // Derive primitive remote values so effect deps compare by value, not by the
  // worldSettings object reference (a fresh object on every snapshot refresh).
  const remoteTopHex = vec3ToHex(worldSettings.skyboxColorTop)
  const remoteBottomHex = vec3ToHex(worldSettings.skyboxColorBottom)
  const remoteHdrPath = worldSettings.skyboxHDRPath

  const [topColorHex, setTopColorHex] = useState(remoteTopHex)
  const [bottomColorHex, setBottomColorHex] = useState(remoteBottomHex)
  const [hdrPath, setHdrPath] = useState(remoteHdrPath)

  useEffect(() => {
    setTopColorHex(remoteTopHex)
  }, [remoteTopHex])
  useEffect(() => {
    setBottomColorHex(remoteBottomHex)
  }, [remoteBottomHex])
  useEffect(() => {
    setHdrPath(remoteHdrPath)
  }, [remoteHdrPath])

  const hdrItems = useMemo<AssetPickerItem[]>(
    () =>
      assets
        .filter((asset) => asset.kind === "texture" && /\.hdr$/i.test(asset.path))
        .map((asset) => {
          const lastSlash = asset.path.lastIndexOf("/")
          const fileName = lastSlash === -1 ? asset.path : asset.path.slice(lastSlash + 1)
          return {
            key: asset.path,
            label: fileName,
            sublabel: asset.path,
            selectValue: asset.path,
          }
        }),
    [assets],
  )

  const handleTopColorChange = (next: string) => {
    setTopColorHex(next)
    setWorldSettings(hexToVec3(next), hexToVec3(bottomColorHex), hdrPath)
  }

  const handleBottomColorChange = (next: string) => {
    setBottomColorHex(next)
    setWorldSettings(hexToVec3(topColorHex), hexToVec3(next), hdrPath)
  }

  const commitHdrPath = (next: string) => {
    setHdrPath(next)
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
        <div className="space-y-4 p-3">
          <section className="rounded border border-neutral-800 bg-neutral-950/60 p-3">
            <div className="mb-3 flex items-center gap-1.5">
              <Palette className="h-3.5 w-3.5 text-neutral-400" />
              <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">
                Sky Gradient
              </p>
            </div>
            <div className={`space-y-2 ${hdrActive ? "pointer-events-none opacity-40" : ""}`}>
              <ColorRow
                label="Top"
                value={topColorHex}
                onChange={handleTopColorChange}
                popoverTitle="Skybox Top Color"
              />
              <ColorRow
                label="Bottom"
                value={bottomColorHex}
                onChange={handleBottomColorChange}
                popoverTitle="Skybox Bottom Color"
              />
            </div>
          </section>

          <section className="rounded border border-neutral-800 bg-neutral-950/60 p-3">
            <div className="mb-3 flex items-center gap-1.5">
              <ImageIcon className="h-3.5 w-3.5 text-neutral-400" />
              <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">
                HDR Sky
              </p>
            </div>
            <div className="space-y-2">
              <div className="flex items-center gap-3">
                <span className="w-20 shrink-0 text-xs text-neutral-500">File</span>
                <HdrPathInput
                  value={hdrPath}
                  onLocalChange={setHdrPath}
                  onCommit={commitHdrPath}
                  onRevert={() => setHdrPath(remoteHdrPath)}
                  pickerItems={hdrItems}
                  onPickerOpen={() => void listAssets()}
                />
              </div>
              <p className="text-[10px] leading-snug text-neutral-500">
                Drop a .hdr from the content browser or pick one with the folder icon.
                Leave empty to use the gradient above.
              </p>
            </div>
          </section>
        </div>
      </div>
    </div>
  )
}

function isHdrAssetDrag(e: React.DragEvent): boolean {
  const types = e.dataTransfer.types
  if (!types.includes("axiom/asset-path")) {
    const fallback = (window as unknown as { __axiomDragAsset?: { kind: string; path: string } })
      .__axiomDragAsset
    return !!fallback && fallback.kind === "texture" && /\.hdr$/i.test(fallback.path)
  }
  const kind = types.includes("axiom/asset-kind")
    ? (window as unknown as { __axiomDragAsset?: { kind: string; path: string } })
        .__axiomDragAsset?.kind
    : undefined
  return kind === undefined || kind === "texture"
}

function HdrPathInput({
  value,
  onLocalChange,
  onCommit,
  onRevert,
  pickerItems,
  onPickerOpen,
}: {
  value: string
  onLocalChange: (next: string) => void
  onCommit: (next: string) => void
  onRevert: () => void
  pickerItems: AssetPickerItem[]
  onPickerOpen?: () => void
}) {
  const [dragHover, setDragHover] = useState(false)

  return (
    <div
      className={`flex w-full min-w-0 items-center rounded border bg-neutral-900 pr-1 transition-colors focus-within:border-neutral-600 ${
        dragHover ? "border-blue-500 ring-1 ring-blue-500/40" : "border-neutral-800"
      }`}
      onDragEnter={(e) => {
        if (isHdrAssetDrag(e)) {
          e.preventDefault()
          setDragHover(true)
        }
      }}
      onDragOver={(e) => {
        if (isHdrAssetDrag(e)) {
          e.preventDefault()
          e.dataTransfer.dropEffect = "copy"
        }
      }}
      onDragLeave={() => setDragHover(false)}
      onDrop={(e) => {
        setDragHover(false)
        const path =
          e.dataTransfer.getData("axiom/asset-path") ||
          (window as unknown as { __axiomDragAsset?: { kind: string; path: string } })
            .__axiomDragAsset?.path ||
          ""
        const kind =
          e.dataTransfer.getData("axiom/asset-kind") ||
          (window as unknown as { __axiomDragAsset?: { kind: string; path: string } })
            .__axiomDragAsset?.kind ||
          ""
        if (!path) return
        if (kind && kind !== "texture") return
        if (!/\.hdr$/i.test(path)) return
        e.preventDefault()
        onCommit(path)
      }}
    >
      <input
        type="text"
        placeholder="Textures/studio.hdr"
        className="min-w-0 flex-1 bg-transparent px-2 py-1 text-xs text-neutral-300 placeholder:text-neutral-600 outline-none"
        value={value}
        onChange={(e) => onLocalChange(e.target.value)}
        onBlur={(e) => onCommit(e.target.value)}
        onKeyDown={(e) => {
          if (e.key === "Enter") {
            onCommit((e.target as HTMLInputElement).value)
            ;(e.target as HTMLInputElement).blur()
          } else if (e.key === "Escape") {
            onRevert()
            ;(e.target as HTMLInputElement).blur()
          }
        }}
      />
      <AssetPickerButton
        items={pickerItems}
        onSelect={onCommit}
        onOpen={onPickerOpen}
        triggerLabel="Browse project HDRs"
        searchPlaceholder="Search HDR files..."
        emptyMessage="No .hdr files found in project"
      />
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
    <div className="flex items-center gap-3">
      <span className="w-20 shrink-0 text-xs text-neutral-500">{label}</span>
      <Popover>
        <PopoverTrigger asChild>
          <button
            type="button"
            className="w-full rounded border border-neutral-800 bg-neutral-900 px-2 py-1 outline-none transition-colors hover:border-neutral-700 focus:border-neutral-600"
          >
            <div
              className="h-3.5 w-full rounded-sm border border-neutral-800"
              style={{ backgroundColor: value }}
            />
          </button>
        </PopoverTrigger>
        <PopoverContent
          align="end"
          side="left"
          className="dark w-auto border-neutral-800 bg-neutral-950 p-3 text-neutral-200"
        >
          <div className="flex flex-col gap-3">
            <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">
              {popoverTitle}
            </p>
            <HexColorPicker color={value} onChange={onChange} />
            <div className="flex items-center gap-3">
              <span className="w-10 shrink-0 text-xs text-neutral-500">HEX</span>
              <input
                type="text"
                className="w-full rounded border border-neutral-800 bg-neutral-900 px-2 py-1 font-mono text-xs text-neutral-300 outline-none focus:border-neutral-600"
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
