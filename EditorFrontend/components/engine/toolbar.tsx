"use client"

import { useEffect } from "react"
import {
  Save,
  Check,
  X,
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
  RefreshCw,
} from "lucide-react"
import { useRemoteViewport } from "./remote-viewport-context"
import { PresenceRoster } from "./presence-roster"

export function Toolbar() {
  const {
    gizmoMode,
    setGizmoMode,
    saveScene,
    saveStatus,
    setSaveStatus,
    reloadScripts,
    reloadStatus,
    setReloadStatus,
  } = useRemoteViewport()

  useEffect(() => {
    if (saveStatus !== "saved" && saveStatus !== "failed") return
    const timer = window.setTimeout(() => setSaveStatus("idle"), 2500)
    return () => window.clearTimeout(timer)
  }, [saveStatus, setSaveStatus])

  useEffect(() => {
    if (reloadStatus !== "reloaded" && reloadStatus !== "failed") return
    const timer = window.setTimeout(() => setReloadStatus("idle"), 2500)
    return () => window.clearTimeout(timer)
  }, [reloadStatus, setReloadStatus])

  return (
    <div className="flex items-center h-10 bg-neutral-950 border-b border-neutral-800 px-2 gap-1">
      <ToolbarGroup>
        <ToolbarButton
          icon={saveStatus === "saved" ? Check : saveStatus === "failed" ? X : Save}
          tooltip="Save"
          onClick={() => void saveScene()}
          className={
            saveStatus === "saved"
              ? "text-green-400"
              : saveStatus === "failed"
              ? "text-red-400"
              : saveStatus === "saving"
              ? "opacity-50"
              : undefined
          }
        />
        <ToolbarButton icon={FolderOpen} tooltip="Open" />
        <ToolbarButton
          icon={reloadStatus === "reloaded" ? Check : reloadStatus === "failed" ? X : RefreshCw}
          tooltip="Reload Scripts"
          onClick={() => void reloadScripts()}
          className={
            reloadStatus === "reloaded"
              ? "text-green-400"
              : reloadStatus === "failed"
              ? "text-red-400"
              : reloadStatus === "reloading"
              ? "opacity-50"
              : undefined
          }
        />
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

      <PresenceRoster />

      <ToolbarDivider />

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
