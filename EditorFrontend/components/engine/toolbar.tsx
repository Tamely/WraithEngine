"use client"

import {
  Save,
  FolderOpen,
  Undo,
  Redo,
  Play,
  Pause,
  SkipForward,
  Square,
  Settings,
  Grid3X3,
  Move,
  RotateCcw,
  Maximize2,
  Eye,
  Layers,
  Sun,
  Camera,
} from "lucide-react"
import { useRemoteViewport } from "./remote-viewport-context"

export function Toolbar() {
  const { gizmoMode, setGizmoMode } = useRemoteViewport()

  return (
    <div className="flex items-center h-10 bg-neutral-950 border-b border-neutral-800 px-2 gap-1">
      <ToolbarGroup>
        <ToolbarButton icon={Save} tooltip="Save" />
        <ToolbarButton icon={FolderOpen} tooltip="Open" />
      </ToolbarGroup>

      <ToolbarDivider />

      <ToolbarGroup>
        <ToolbarButton icon={Undo} tooltip="Undo" />
        <ToolbarButton icon={Redo} tooltip="Redo" />
      </ToolbarGroup>

      <ToolbarDivider />

      <ToolbarGroup>
        <ToolbarButton
          icon={Move}
          tooltip="Move (Q)"
          active={gizmoMode === "translate"}
          onClick={() => void setGizmoMode("translate")}
        />
        <ToolbarButton
          icon={RotateCcw}
          tooltip="Rotate (R)"
          active={gizmoMode === "rotate"}
          onClick={() => void setGizmoMode("rotate")}
        />
        <ToolbarButton
          icon={Maximize2}
          tooltip="Scale (E)"
          active={gizmoMode === "scale"}
          onClick={() => void setGizmoMode("scale")}
        />
      </ToolbarGroup>

      <ToolbarDivider />

      <ToolbarGroup>
        <ToolbarButton icon={Grid3X3} tooltip="Grid Snap" />
        <ToolbarButton icon={Layers} tooltip="Layers" />
      </ToolbarGroup>

      <div className="flex-1" />

      <ToolbarGroup>
        <ToolbarButton icon={Play} tooltip="Play" className="text-green-500 hover:text-green-400" />
        <ToolbarButton icon={Pause} tooltip="Pause" />
        <ToolbarButton icon={SkipForward} tooltip="Skip" />
        <ToolbarButton icon={Square} tooltip="Stop" />
      </ToolbarGroup>

      <ToolbarDivider />

      <ToolbarGroup>
        <ToolbarButton icon={Eye} tooltip="Viewport Options" />
        <ToolbarButton icon={Sun} tooltip="Lighting" />
        <ToolbarButton icon={Camera} tooltip="Camera" />
        <ToolbarButton icon={Settings} tooltip="Settings" />
      </ToolbarGroup>
    </div>
  )
}

function ToolbarGroup({ children }: { children: React.ReactNode }) {
  return <div className="flex items-center gap-0.5">{children}</div>
}

function ToolbarButton({
  icon: Icon,
  tooltip,
  active,
  className,
  onClick,
}: {
  icon: React.ElementType
  tooltip: string
  active?: boolean
  className?: string
  onClick?: () => void
}) {
  return (
    <button
      title={tooltip}
      onClick={onClick}
      className={`p-2 rounded hover:bg-neutral-800 transition-colors ${
        active ? "bg-neutral-700 text-white" : "text-neutral-400"
      } ${className || ""}`}
    >
      <Icon className="w-4 h-4" />
    </button>
  )
}

function ToolbarDivider() {
  return <div className="w-px h-6 bg-neutral-700 mx-1" />
}
