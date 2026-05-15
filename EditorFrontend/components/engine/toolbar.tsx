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
  ChevronDown,
} from "lucide-react"
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuLabel,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu"
import {
  useRemoteViewport,
  type RemoteViewportGridSnapSettings,
} from "./remote-viewport-context"
import { PresenceRoster } from "./presence-roster"

const TRANSLATION_SNAP_OPTIONS = [0.25, 0.5, 1, 5] as const
const ROTATION_SNAP_OPTIONS = [5, 10, 15, 45] as const
const SCALE_SNAP_OPTIONS = [0.01, 0.05, 0.1, 0.25] as const

export function Toolbar() {
  const {
    gizmoMode,
    gridSnapSettings,
    setGizmoMode,
    setGridSnapSettings,
    saveScene,
    playSession,
    pauseSession,
    resumeSession,
    stopSession,
    saveStatus,
    setSaveStatus,
    reloadScripts,
    reloadStatus,
    setReloadStatus,
    runtimeState,
    canControlRuntime,
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
        <GridSnapDropdown
          settings={gridSnapSettings}
          onChange={(settings) => void setGridSnapSettings(settings)}
        />
        <ToolbarButton icon={Layers} tooltip="Layers" />
      </ToolbarGroup>

      <div className="flex-1" />

      <PresenceRoster />

      <ToolbarDivider />

      <ToolbarGroup>
        <ToolbarButton
          icon={Play}
          tooltip={runtimeState === "paused" ? "Resume" : "Play"}
          className="text-green-500 hover:text-green-400"
          active={runtimeState === "playing"}
          disabled={!canControlRuntime || runtimeState === "playing"}
          onClick={() => void (runtimeState === "paused" ? resumeSession() : playSession())}
        />
        <ToolbarButton
          icon={Pause}
          tooltip="Pause"
          active={runtimeState === "paused"}
          disabled={!canControlRuntime || runtimeState !== "playing"}
          onClick={() => void pauseSession()}
        />
        <ToolbarButton icon={SkipForward} tooltip="Skip" disabled />
        <ToolbarButton
          icon={Square}
          tooltip="Stop"
          disabled={!canControlRuntime || runtimeState === "edit"}
          onClick={() => void stopSession()}
        />
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

function GridSnapDropdown({
  settings,
  onChange,
}: {
  settings: RemoteViewportGridSnapSettings
  onChange: (settings: RemoteViewportGridSnapSettings) => void
}) {
  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <button
          title="Grid Snap"
          className={`flex items-center gap-1 rounded p-2 hover:bg-neutral-800 transition-colors ${
            settings.enabled ? "bg-neutral-700 text-white" : "text-neutral-400"
          }`}
          type="button"
        >
          <Grid3X3 className="h-4 w-4" />
          <ChevronDown className="h-3 w-3" />
        </button>
      </DropdownMenuTrigger>
      <DropdownMenuContent
        align="start"
        className="w-56 border-neutral-800 bg-neutral-950 text-neutral-200"
      >
        <DropdownMenuLabel className="text-xs uppercase tracking-[0.16em] text-neutral-500">
          Move Snap
        </DropdownMenuLabel>
        <DropdownMenuRadioGroup
          value={String(settings.enabled ? settings.translationStep : 0)}
          onValueChange={(value) =>
            onChange({
              ...settings,
              enabled: value !== "0",
              translationStep: value === "0" ? settings.translationStep : Number(value),
            })
          }
        >
          <DropdownMenuRadioItem value="0">Off</DropdownMenuRadioItem>
          {TRANSLATION_SNAP_OPTIONS.map((value) => (
            <DropdownMenuRadioItem key={value} value={String(value)}>
              {formatSnapValue(value)} units
            </DropdownMenuRadioItem>
          ))}
        </DropdownMenuRadioGroup>

        <DropdownMenuSeparator className="bg-neutral-800" />
        <DropdownMenuLabel className="text-xs uppercase tracking-[0.16em] text-neutral-500">
          Rotate Snap
        </DropdownMenuLabel>
        <DropdownMenuRadioGroup
          value={String(settings.rotationStepDegrees)}
          onValueChange={(value) =>
            onChange({
              ...settings,
              rotationStepDegrees: Number(value),
            })
          }
        >
          {ROTATION_SNAP_OPTIONS.map((value) => (
            <DropdownMenuRadioItem key={value} value={String(value)}>
              {value} degrees
            </DropdownMenuRadioItem>
          ))}
        </DropdownMenuRadioGroup>

        <DropdownMenuSeparator className="bg-neutral-800" />
        <DropdownMenuLabel className="text-xs uppercase tracking-[0.16em] text-neutral-500">
          Scale Snap
        </DropdownMenuLabel>
        <DropdownMenuRadioGroup
          value={String(settings.scaleStep)}
          onValueChange={(value) =>
            onChange({
              ...settings,
              scaleStep: Number(value),
            })
          }
        >
          {SCALE_SNAP_OPTIONS.map((value) => (
            <DropdownMenuRadioItem key={value} value={String(value)}>
              {formatSnapValue(value)} scale
            </DropdownMenuRadioItem>
          ))}
        </DropdownMenuRadioGroup>
      </DropdownMenuContent>
    </DropdownMenu>
  )
}

function formatSnapValue(value: number) {
  return Number.isInteger(value) ? String(value) : value.toFixed(2).replace(/0$/, "")
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
  disabled,
}: {
  icon: React.ElementType
  tooltip: string
  active?: boolean
  className?: string
  onClick?: () => void
  disabled?: boolean
}) {
  return (
    <button
      title={tooltip}
      onClick={onClick}
      disabled={disabled}
      className={`p-2 rounded transition-colors ${
        disabled ? "cursor-not-allowed opacity-40" : "hover:bg-neutral-800"
      } ${active ? "bg-neutral-700 text-white" : "text-neutral-400"
      } ${className || ""}`}
    >
      <Icon className="w-4 h-4" />
    </button>
  )
}

function ToolbarDivider() {
  return <div className="w-px h-6 bg-neutral-700 mx-1" />
}
