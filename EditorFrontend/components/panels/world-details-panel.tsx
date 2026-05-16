"use client"

import { useRemoteViewport } from "@/components/engine/remote-viewport-context"
import { HexColorPicker } from "react-colorful"
import { useState, useEffect } from "react"
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

  const [topColorHex, setTopColorHex] = useState(vec3ToHex(worldSettings.skyboxColorTop))
  const [bottomColorHex, setBottomColorHex] = useState(vec3ToHex(worldSettings.skyboxColorBottom))

  // Update local state when remote state changes
  useEffect(() => {
    setTopColorHex(vec3ToHex(worldSettings.skyboxColorTop))
    setBottomColorHex(vec3ToHex(worldSettings.skyboxColorBottom))
  }, [worldSettings])

  const handleTopColorChange = (newColor: string) => {
    setTopColorHex(newColor)
    setWorldSettings(hexToVec3(newColor), hexToVec3(bottomColorHex))
  }

  const handleBottomColorChange = (newColor: string) => {
    setBottomColorHex(newColor)
    setWorldSettings(hexToVec3(topColorHex), hexToVec3(newColor))
  }

  return (
    <div className="h-full w-full overflow-y-auto overflow-x-hidden bg-background text-sm">
      <div className="flex flex-col gap-4 p-4">
        <div className="flex flex-col gap-2">
          <h3 className="font-semibold text-foreground/80 mb-2">Environment Settings</h3>
          
          <div className="grid grid-cols-[100px_1fr] items-center gap-4">
            <span className="text-muted-foreground text-xs uppercase font-medium">Sky Top</span>
            <Popover>
              <PopoverTrigger asChild>
                <div className="flex items-center gap-2 cursor-pointer rounded-md border border-border p-1 hover:bg-muted/50 transition-colors">
                  <div
                    className="h-6 w-full rounded-[4px] border shadow-sm"
                    style={{ backgroundColor: topColorHex }}
                  />
                </div>
              </PopoverTrigger>
              <PopoverContent className="w-auto border-border bg-card p-3" side="left">
                <div className="flex flex-col gap-3">
                  <div className="text-xs font-medium text-muted-foreground">Skybox Top Color</div>
                  <HexColorPicker color={topColorHex} onChange={handleTopColorChange} />
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-mono text-muted-foreground w-12">HEX</span>
                    <input
                      type="text"
                      className="flex h-8 w-full rounded-md border border-input bg-background px-3 py-1 text-xs shadow-sm transition-colors focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-ring"
                      value={topColorHex}
                      onChange={(e) => handleTopColorChange(e.target.value)}
                    />
                  </div>
                </div>
              </PopoverContent>
            </Popover>
          </div>

          <div className="grid grid-cols-[100px_1fr] items-center gap-4">
            <span className="text-muted-foreground text-xs uppercase font-medium">Sky Bottom</span>
            <Popover>
              <PopoverTrigger asChild>
                <div className="flex items-center gap-2 cursor-pointer rounded-md border border-border p-1 hover:bg-muted/50 transition-colors">
                  <div
                    className="h-6 w-full rounded-[4px] border shadow-sm"
                    style={{ backgroundColor: bottomColorHex }}
                  />
                </div>
              </PopoverTrigger>
              <PopoverContent className="w-auto border-border bg-card p-3" side="left">
                <div className="flex flex-col gap-3">
                  <div className="text-xs font-medium text-muted-foreground">Skybox Bottom Color</div>
                  <HexColorPicker color={bottomColorHex} onChange={handleBottomColorChange} />
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-mono text-muted-foreground w-12">HEX</span>
                    <input
                      type="text"
                      className="flex h-8 w-full rounded-md border border-input bg-background px-3 py-1 text-xs shadow-sm transition-colors focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-ring"
                      value={bottomColorHex}
                      onChange={(e) => handleBottomColorChange(e.target.value)}
                    />
                  </div>
                </div>
              </PopoverContent>
            </Popover>
          </div>

        </div>
      </div>
    </div>
  )
}
