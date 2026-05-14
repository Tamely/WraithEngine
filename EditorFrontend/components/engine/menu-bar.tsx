"use client"

import {
  Archive,
  ChevronDown,
  FolderOpen,
  Package,
  Plus,
} from "lucide-react"
import { useEffect, useRef, useState } from "react"
import type { ProjectDescriptor } from "./project-browser"

const menuItems = ["File", "Edit", "Window", "Tools", "Build", "Select", "Actor", "Help"]

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
  const [fileMenuOpen, setFileMenuOpen] = useState(false)
  const [buildMenuOpen, setBuildMenuOpen] = useState(false)
  const fileMenuRef = useRef<HTMLDivElement | null>(null)
  const buildMenuRef = useRef<HTMLDivElement | null>(null)

  useEffect(() => {
    if (!fileMenuOpen && !buildMenuOpen) return

    function handlePointerDown(event: MouseEvent) {
      if (!fileMenuRef.current?.contains(event.target as Node)) {
        setFileMenuOpen(false)
      }
      if (!buildMenuRef.current?.contains(event.target as Node)) {
        setBuildMenuOpen(false)
      }
    }

    function handleEscape(event: KeyboardEvent) {
      if (event.key === "Escape") {
        setFileMenuOpen(false)
        setBuildMenuOpen(false)
      }
    }

    window.addEventListener("mousedown", handlePointerDown)
    window.addEventListener("keydown", handleEscape)

    return () => {
      window.removeEventListener("mousedown", handlePointerDown)
      window.removeEventListener("keydown", handleEscape)
    }
  }, [buildMenuOpen, fileMenuOpen])

  return (
    <div className="flex items-center h-8 bg-neutral-950 border-b border-neutral-800 px-2">
      <div className="flex items-center gap-1 mr-4">
        <div className="w-8 h-6 bg-white text-black font-bold text-xs flex items-center justify-center rounded">
          WE
        </div>
      </div>
      <div className="flex items-center gap-1">
        {menuItems.map((item) => (
          item === "File" ? (
            <div key={item} className="relative" ref={fileMenuRef}>
              <button
                aria-expanded={fileMenuOpen}
                className="px-3 py-1 text-xs text-neutral-300 hover:bg-neutral-800 hover:text-white rounded transition-colors"
                onClick={() => setFileMenuOpen((current) => !current)}
                type="button"
              >
                {item}
              </button>
              {fileMenuOpen ? (
                <div className="absolute left-0 top-full z-50 mt-1 w-52 rounded-lg border border-neutral-800 bg-neutral-950/95 p-1 shadow-2xl backdrop-blur">
                  <button
                    className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
                    onClick={() => {
                      setFileMenuOpen(false)
                      onNewProject()
                    }}
                    type="button"
                  >
                    <Plus className="h-3.5 w-3.5" />
                    <span>New Project</span>
                  </button>
                  <button
                    className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white"
                    onClick={() => {
                      setFileMenuOpen(false)
                      onOpenProject()
                    }}
                    type="button"
                  >
                    <FolderOpen className="h-3.5 w-3.5" />
                    <span>Open Project</span>
                  </button>
                </div>
              ) : null}
            </div>
          ) : item === "Build" ? (
            <div key={item} className="relative" ref={buildMenuRef}>
              <button
                aria-expanded={buildMenuOpen}
                className="px-3 py-1 text-xs text-neutral-300 hover:bg-neutral-800 hover:text-white rounded transition-colors"
                onClick={() => setBuildMenuOpen((current) => !current)}
                type="button"
              >
                {item}
              </button>
              {buildMenuOpen ? (
                <div className="absolute left-0 top-full z-50 mt-1 w-56 rounded-lg border border-neutral-800 bg-neutral-950/95 p-1 shadow-2xl backdrop-blur">
                  <button
                    className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white disabled:cursor-not-allowed disabled:opacity-40"
                    disabled={!activeProject || buildBusy}
                    onClick={() => {
                      setBuildMenuOpen(false)
                      onCookProject()
                    }}
                    type="button"
                  >
                    <Archive className="h-3.5 w-3.5" />
                    <span>Cook Project</span>
                  </button>
                  <button
                    className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-left text-xs text-neutral-300 transition-colors hover:bg-neutral-800 hover:text-white disabled:cursor-not-allowed disabled:opacity-40"
                    disabled={!activeProject || buildBusy}
                    onClick={() => {
                      setBuildMenuOpen(false)
                      onPackageProject()
                    }}
                    type="button"
                  >
                    <Package className="h-3.5 w-3.5" />
                    <span>Package Project</span>
                  </button>
                </div>
              ) : null}
            </div>
          ) : (
            <button
              key={item}
              className="px-3 py-1 text-xs text-neutral-300 hover:bg-neutral-800 hover:text-white rounded transition-colors"
              type="button"
            >
              {item}
            </button>
          )
        ))}
      </div>
      <div className="flex-1" />
      <div className="flex items-center gap-2 text-xs text-neutral-400">
        <div className="flex items-center gap-2 rounded px-2 py-1">
          <FolderOpen className="w-3.5 h-3.5" />
          <span>Project: {activeProject?.name ?? "None"}</span>
          <ChevronDown className="w-3 h-3 opacity-40" />
        </div>
      </div>
    </div>
  )
}
