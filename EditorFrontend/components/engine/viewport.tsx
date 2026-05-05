"use client"

import { useRef, useEffect, useState } from "react"
import {
  Maximize2,
  Grid3X3,
  Eye,
  Camera,
  ChevronDown,
} from "lucide-react"

export function Viewport() {
  const videoRef = useRef<HTMLVideoElement>(null)
  const [isConnected, setIsConnected] = useState(false)

  useEffect(() => {
    // Placeholder for WebRTC connection
    // Your WebRTC implementation would go here
    // When connected, set isConnected to true and attach stream to videoRef.current.srcObject
  }, [])

  return (
    <div className="flex-1 flex flex-col bg-neutral-900 border-b border-neutral-800">
      <div className="flex items-center justify-between h-8 bg-neutral-950 border-b border-neutral-800 px-2">
        <div className="flex items-center gap-2">
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Perspective
            <ChevronDown className="w-3 h-3" />
          </button>
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Lit
            <ChevronDown className="w-3 h-3" />
          </button>
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Show
            <ChevronDown className="w-3 h-3" />
          </button>
        </div>
        <div className="flex items-center gap-1">
          <ViewportButton icon={Grid3X3} />
          <ViewportButton icon={Eye} />
          <ViewportButton icon={Camera} />
          <ViewportButton icon={Maximize2} />
        </div>
      </div>
      <div className="flex-1 relative">
        <video
          ref={videoRef}
          className="absolute inset-0 w-full h-full object-contain bg-black"
          autoPlay
          playsInline
          muted
        />
        {!isConnected && (
          <div className="absolute inset-0 flex items-center justify-center bg-neutral-900">
            <div className="text-center">
              <div className="w-16 h-16 mx-auto mb-4 border-2 border-neutral-700 rounded-lg flex items-center justify-center">
                <Camera className="w-8 h-8 text-neutral-600" />
              </div>
              <p className="text-neutral-500 text-sm">Waiting for WebRTC stream...</p>
              <p className="text-neutral-600 text-xs mt-1">Connect your game instance</p>
            </div>
          </div>
        )}
        <div className="absolute bottom-2 left-2 flex items-center gap-2 text-xs text-neutral-500">
          <span>FPS: --</span>
          <span>|</span>
          <span>Draw Calls: --</span>
          <span>|</span>
          <span>Triangles: --</span>
        </div>
        <div className="absolute top-2 right-2 flex items-center gap-2">
          <div className="px-2 py-1 bg-black/50 rounded text-xs text-neutral-400">
            X: 0.00 Y: 0.00 Z: 0.00
          </div>
        </div>
      </div>
    </div>
  )
}

function ViewportButton({ icon: Icon }: { icon: React.ElementType }) {
  return (
    <button className="p-1.5 text-neutral-400 hover:text-white hover:bg-neutral-800 rounded transition-colors">
      <Icon className="w-3.5 h-3.5" />
    </button>
  )
}
