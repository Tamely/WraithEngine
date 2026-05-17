"use client"

import {
  Archive,
  ChevronDown,
  Code,
  Eye,
  FolderOpen,
  Globe,
  Layers,
  List,
  Monitor,
  Package,
  Plus,
  Settings,
  Tv,
  Check,
} from "lucide-react"
import { useEffect, useRef, useState } from "react"
import type { ProjectDescriptor } from "./project-browser"
import { useDock, type PanelId } from "./dock/dock-context"

const PANEL_ENTRIES: { id: PanelId; label: string; Icon: React.ComponentType<{ className?: string }> }[] = [
  { id: "viewport",        label: "Viewport",        Icon: Monitor },
  { id: "outliner",        label: "Outliner",        Icon: List },
  { id: "place-actors",    label: "Place Actors",    Icon: Layers },
  { id: "details",         label: "Details",         Icon: Settings },
  { id: "world-details",   label: "World Details",   Icon: Globe },
  { id: "content-browser", label: "Content Browser", Icon: FolderOpen },
  { id: "script-editor",   label: "Script Editor",   Icon: Code },
  { id: "remote-viewport", label: "Remote Viewport", Icon: Tv },
]

export function MenuBar({
  activeProject,
  onNewProject,
  onOpenProject,
  onCookProject,
  onPackageProject,
  buildBusy,
}: {
  activeProject: ProjectDescriptor | null
  onNewProject: () => void
  onOpenProject: () => void
  onCookProject: () => void
  onPackageProject: () => void
  buildBusy: boolean
}) {
  const { openPanelIds, openPanel, closePanel } = useDock()

  const [fileMenuOpen, setFileMenuOpen] = useState(false)
  const [buildMenuOpen, setBuildMenuOpen] = useState(false)
  const [windowMenuOpen, setWindowMenuOpen] = useState(false)
  const fileMenuRef = useRef<HTMLDivElement | null>(null)
  const buildMenuRef = useRef<HTMLDivElement | null>(null)
  const windowMenuRef = useRef<HTMLDivElement | null>(null)

  useEffect(() => {
    if (!fileMenuOpen && !buildMenuOpen && !windowMenuOpen) return

    function handlePointerDown(event: MouseEvent) {
      if (!fileMenuRef.current?.contains(event.target as Node)) setFileMenuOpen(false)
      if (!buildMenuRef.current?.contains(event.target as Node)) setBuildMenuOpen(false)
      if (!windowMenuRef.current?.contains(event.target as Node)) setWindowMenuOpen(false)
    }

    function handleEscape(event: KeyboardEvent) {
      if (event.key === "Escape") {
        setFileMenuOpen(false)
        setBuildMenuOpen(false)
        setWindowMenuOpen(false)
      }
    }

    window.addEventListener("mousedown", handlePointerDown)
    window.addEventListener("keydown", handleEscape)
    return () => {
      window.removeEventListener("mousedown", handlePointerDown)
      window.removeEventListener("keydown", handleEscape)
    }
  }, [buildMenuOpen, fileMenuOpen, windowMenuOpen])

  function handleTogglePanel(panelId: PanelId) {
    if (openPanelIds.has(panelId)) {
      closePanel(panelId)
    } else {
      openPanel(panelId)
    }
  }

  return (
    <div className="flex h-8 items-center border-b border-neutral-800 bg-neutral-950 px-2">
      <div className="mr-4 flex items-center gap-1">
        <div className="flex h-6 w-8 items-center justify-center rounded bg-white text-xs font-bold text-black">
          WE
        </div>
      </div>

      <div className="flex items-center gap-1">
        {/* File */}
        <div className="relative" ref={fileMenuRef}>
          <button
            aria-expanded={fileMenuOpen}
            className="rounded px-3 py-1 text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
            onClick={() => { setFileMenuOpen((v) => !v); setBuildMenuOpen(false); setWindowMenuOpen(false) }}
            type="button"
          >
            File
          </button>
          {fileMenuOpen && (
            <div className="absolute left-0 top-full z-50 mt-1 w-52 rounded-lg border border-neutral-800 bg-neutral-950/95 p-1 shadow-2xl backdrop-blur">
              <button
                className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
                onClick={() => { setFileMenuOpen(false); onNewProject() }}
                type="button"
              >
                <Plus className="h-3.5 w-3.5" />
                New Project
              </button>
              <button
                className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
                onClick={() => { setFileMenuOpen(false); onOpenProject() }}
                type="button"
              >
                <FolderOpen className="h-3.5 w-3.5" />
                Open Project
              </button>
            </div>
          )}
        </div>

        {/* Window */}
        <div className="relative" ref={windowMenuRef}>
          <button
            aria-expanded={windowMenuOpen}
            className="rounded px-3 py-1 text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
            onClick={() => { setWindowMenuOpen((v) => !v); setFileMenuOpen(false); setBuildMenuOpen(false) }}
            type="button"
          >
            Window
          </button>
          {windowMenuOpen && (
            <div className="absolute left-0 top-full z-50 mt-1 w-52 rounded-lg border border-neutral-800 bg-neutral-950/95 p-1 shadow-2xl backdrop-blur">
              {PANEL_ENTRIES.map(({ id, label, Icon }) => {
                const isOpen = openPanelIds.has(id)
                return (
                  <button
                    key={id}
                    className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
                    onClick={() => handleTogglePanel(id)}
                    type="button"
                  >
                    <Icon className="h-3.5 w-3.5 shrink-0 text-neutral-400" />
                    <span className="flex-1">{label}</span>
                    {isOpen && <Check className="h-3 w-3 text-neutral-400" />}
                  </button>
                )
              })}
            </div>
          )}
        </div>

        {/* Build */}
        <div className="relative" ref={buildMenuRef}>
          <button
            aria-expanded={buildMenuOpen}
            className="rounded px-3 py-1 text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
            onClick={() => { setBuildMenuOpen((v) => !v); setFileMenuOpen(false); setWindowMenuOpen(false) }}
            type="button"
          >
            Build
          </button>
          {buildMenuOpen && (
            <div className="absolute left-0 top-full z-50 mt-1 w-56 rounded-lg border border-neutral-800 bg-neutral-950/95 p-1 shadow-2xl backdrop-blur">
              <button
                className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white disabled:cursor-not-allowed disabled:opacity-40"
                disabled={!activeProject || buildBusy}
                onClick={() => { setBuildMenuOpen(false); onCookProject() }}
                type="button"
              >
                <Archive className="h-3.5 w-3.5" />
                Cook Project
              </button>
              <button
                className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white disabled:cursor-not-allowed disabled:opacity-40"
                disabled={!activeProject || buildBusy}
                onClick={() => { setBuildMenuOpen(false); onPackageProject() }}
                type="button"
              >
                <Package className="h-3.5 w-3.5" />
                Package Project
              </button>
            </div>
          )}
        </div>

        {/* Inert stubs */}
        {["Edit", "Tools", "Select", "Actor", "Help"].map((item) => (
          <button
            key={item}
            className="rounded px-3 py-1 text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
            type="button"
          >
            {item}
          </button>
        ))}
      </div>

      <div className="flex-1" />
      <div className="flex items-center gap-2 text-xs text-neutral-400">
        <div className="flex items-center gap-2 rounded px-2 py-1">
          <FolderOpen className="h-3.5 w-3.5" />
          <span>Project: {activeProject?.name ?? "None"}</span>
          <ChevronDown className="h-3 w-3 opacity-40" />
        </div>
      </div>
    </div>
  )
}
