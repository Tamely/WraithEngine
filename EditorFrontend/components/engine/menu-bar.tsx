"use client"

import {
  FolderOpen,
  ChevronDown,
} from "lucide-react"
import type { ProjectDescriptor } from "./project-browser"

const menuItems = ["File", "Edit", "Window", "Tools", "Build", "Select", "Actor", "Help"]

export function MenuBar({
  activeProject,
  onOpenProjectBrowser,
}: {
  activeProject: ProjectDescriptor | null
  onOpenProjectBrowser: () => void
}) {
  return (
    <div className="flex items-center h-8 bg-neutral-950 border-b border-neutral-800 px-2">
      <div className="flex items-center gap-1 mr-4">
        <div className="w-8 h-6 bg-white text-black font-bold text-xs flex items-center justify-center rounded">
          WE
        </div>
      </div>
      <div className="flex items-center gap-1">
        {menuItems.map((item) => (
          <button
            key={item}
            className="px-3 py-1 text-xs text-neutral-300 hover:bg-neutral-800 hover:text-white rounded transition-colors"
          >
            {item}
          </button>
        ))}
      </div>
      <div className="flex-1" />
      <div className="flex items-center gap-2 text-xs text-neutral-400">
        <button
          className="flex items-center gap-2 rounded px-2 py-1 transition-colors hover:bg-neutral-800 hover:text-white"
          onClick={onOpenProjectBrowser}
          type="button"
        >
          <FolderOpen className="w-3.5 h-3.5" />
          <span>Project: {activeProject?.name ?? "None"}</span>
          <ChevronDown className="w-3 h-3" />
        </button>
      </div>
    </div>
  )
}
