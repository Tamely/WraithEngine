"use client"

import { FolderOpen, Loader2, Plus, RefreshCw, Search, SlidersHorizontal, X } from "lucide-react"
import { useMemo, useState } from "react"

export interface ProjectDescriptor {
  projectId: string
  name: string
  slug: string
  rootPath: string
  contentDir: string
  engineContentDir: string
  sceneFilePath: string
}

interface ProjectBrowserProps {
  projects: ProjectDescriptor[]
  activeProject: ProjectDescriptor | null
  loading: boolean
  busy: boolean
  error: string | null
  serverOrigin: string
  canClose: boolean
  onClose: () => void
  onRefresh: () => Promise<void>
  onOpenProject: (slug: string) => Promise<void>
  onCreateProject: (name: string) => Promise<void>
}

export function ProjectBrowser({
  projects,
  activeProject,
  loading,
  busy,
  error,
  canClose,
  onClose,
  onRefresh,
  onOpenProject,
  onCreateProject,
}: ProjectBrowserProps) {
  const [filter, setFilter] = useState("")
  const [selected, setSelected] = useState<ProjectDescriptor | null>(activeProject ?? null)
  const [showCreate, setShowCreate] = useState(false)
  const [newProjectName, setNewProjectName] = useState("")

  const sortedProjects = useMemo(
    () =>
      [...projects]
        .sort((a, b) => a.name.localeCompare(b.name))
        .filter((p) => p.name.toLowerCase().includes(filter.toLowerCase())),
    [projects, filter]
  )

  async function handleCreateProject() {
    const trimmed = newProjectName.trim()
    if (!trimmed) return
    await onCreateProject(trimmed)
    setNewProjectName("")
    setShowCreate(false)
  }

  async function handleOpen() {
    if (!selected) return
    await onOpenProject(selected.slug)
  }

  const selectedIsActive = selected?.slug === activeProject?.slug

  return (
    // Backdrop
    <div className="absolute inset-0 z-40 flex items-center justify-center bg-black/80 text-white">
      {/* Dialog */}
      <div className="flex w-[860px] max-w-[95vw] flex-col rounded-lg border border-neutral-800 bg-neutral-950 shadow-[0_32px_80px_rgba(0,0,0,0.8)]"
        style={{ maxHeight: "80vh" }}>

        {/* Title bar */}
        <div className="flex shrink-0 items-center justify-between border-b border-neutral-800 px-4 py-2.5">
          <span className="text-sm font-medium text-white">Open Project</span>
          {canClose && (
            <button
              className="flex h-6 w-6 items-center justify-center rounded text-neutral-500 transition-colors hover:bg-neutral-800 hover:text-white"
              onClick={onClose}
              type="button"
              aria-label="Close"
            >
              <X className="h-3.5 w-3.5" />
            </button>
          )}
        </div>

        {/* Search + actions bar */}
        <div className="flex shrink-0 items-center gap-2 border-b border-neutral-800 px-3 py-2">
          <div className="relative flex-1">
            <Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-neutral-500 pointer-events-none" />
            <input
              className="h-8 w-full rounded border border-neutral-800 bg-neutral-900 pl-8 pr-3 text-sm text-white outline-none transition-colors placeholder:text-neutral-600 focus:border-neutral-700"
              placeholder="Filter Projects..."
              value={filter}
              onChange={(e) => setFilter(e.target.value)}
              type="text"
            />
          </div>
          <button
            className="flex h-8 items-center gap-1.5 rounded border border-neutral-800 bg-neutral-900 px-2.5 text-xs text-neutral-400 transition-colors hover:border-neutral-700 hover:text-white"
            type="button"
            title="Sort"
          >
            <SlidersHorizontal className="h-3.5 w-3.5" />
          </button>
          <button
            className="flex h-8 items-center gap-1.5 rounded border border-neutral-800 bg-neutral-900 px-2.5 text-xs text-neutral-400 transition-colors hover:border-neutral-700 hover:text-white disabled:opacity-50"
            disabled={busy || loading}
            onClick={() => void onRefresh()}
            type="button"
            title="Refresh"
          >
            {loading ? <Loader2 className="h-3.5 w-3.5 animate-spin" /> : <RefreshCw className="h-3.5 w-3.5" />}
          </button>
        </div>

        {/* Project thumbnail grid */}
        <div className="min-h-0 flex-1 overflow-y-auto p-4">
          {sortedProjects.length === 0 && !showCreate ? (
            <div className="flex flex-col items-center justify-center py-16 text-center">
              <FolderOpen className="h-10 w-10 text-neutral-700" />
              <p className="mt-3 text-sm font-medium text-white">No projects found</p>
              <p className="mt-1 text-xs text-neutral-500">
                {filter ? "Try a different filter." : "Create a new project to get started."}
              </p>
            </div>
          ) : (
            <div className="flex flex-wrap gap-3">
              {/* New project card */}
              {showCreate ? (
                <div className="flex w-[120px] flex-col gap-2 rounded border border-dashed border-neutral-700 bg-neutral-900/60 p-2">
                  <div className="flex h-[80px] items-center justify-center rounded bg-neutral-800">
                    <Plus className="h-6 w-6 text-neutral-500" />
                  </div>
                  <input
                    autoFocus
                    className="h-7 rounded border border-neutral-700 bg-neutral-900 px-2 text-xs text-white outline-none placeholder:text-neutral-600 focus:border-neutral-600"
                    placeholder="Project name"
                    value={newProjectName}
                    disabled={busy}
                    onChange={(e) => setNewProjectName(e.target.value)}
                    onKeyDown={(e) => {
                      if (e.key === "Enter") void handleCreateProject()
                      if (e.key === "Escape") { setShowCreate(false); setNewProjectName("") }
                    }}
                    type="text"
                  />
                  <div className="flex gap-1">
                    <button
                      className="flex h-6 flex-1 items-center justify-center rounded bg-white text-[10px] font-medium text-black transition-colors hover:bg-neutral-200 disabled:opacity-50"
                      disabled={busy || newProjectName.trim().length === 0}
                      onClick={() => void handleCreateProject()}
                      type="button"
                    >
                      {busy ? <Loader2 className="h-3 w-3 animate-spin" /> : "Create"}
                    </button>
                    <button
                      className="flex h-6 w-6 items-center justify-center rounded border border-neutral-700 text-neutral-400 transition-colors hover:text-white"
                      onClick={() => { setShowCreate(false); setNewProjectName("") }}
                      type="button"
                    >
                      <X className="h-3 w-3" />
                    </button>
                  </div>
                </div>
              ) : null}

              {sortedProjects.map((project) => {
                const isActive = activeProject?.slug === project.slug
                const isSelected = selected?.slug === project.slug
                return (
                  <button
                    key={project.projectId}
                    type="button"
                    onClick={() => setSelected(project)}
                    onDoubleClick={() => { setSelected(project); void onOpenProject(project.slug) }}
                    className={`group flex w-[120px] flex-col rounded border p-1.5 text-left transition-all focus:outline-none ${isSelected
                        ? "border-white bg-white/10"
                        : "border-transparent hover:border-neutral-700 hover:bg-neutral-900/60"
                      }`}
                  >
                    {/* Thumbnail */}
                    <div className={`relative h-[80px] w-full overflow-hidden rounded ${isSelected ? "ring-1 ring-white" : ""
                      }`}>
                      <div className="flex h-full w-full items-center justify-center bg-neutral-800">
                        <FolderOpen className="h-8 w-8 text-neutral-600" />
                      </div>
                    </div>

                    {/* Name + status */}
                    <div className="mt-1.5 px-0.5">
                      <p className={`truncate text-[11px] font-medium leading-tight ${isSelected ? "text-white" : "text-neutral-300"
                        }`}>
                        {project.name}
                      </p>
                      <p className={`mt-0.5 text-[10px] leading-tight ${isActive
                          ? isSelected ? "text-white font-semibold" : "text-neutral-300"
                          : "text-neutral-600"
                        }`}>
                        {isActive ? "Current" : project.slug}
                      </p>
                    </div>
                  </button>
                )
              })}
            </div>
          )}

          {error ? (
            <div className="mt-3 rounded border border-red-900/40 bg-red-950/30 px-3 py-2 text-xs text-red-300">
              {error}
            </div>
          ) : null}
        </div>

        {/* Footer bar */}
        <div className="shrink-0 border-t border-neutral-800 bg-neutral-950 px-4 py-3">
          {/* Project name row */}
          <div className="flex items-center gap-3">
            <span className="shrink-0 text-xs text-neutral-400">Project Name</span>
            <input
              className="h-7 flex-1 rounded border border-neutral-800 bg-neutral-900 px-2.5 text-xs text-white outline-none transition-colors placeholder:text-neutral-600 focus:border-neutral-700"
              value={selected?.name ?? ""}
              readOnly
              placeholder="No project selected"
              type="text"
            />
          </div>

          {/* Action buttons row */}
          <div className="mt-3 flex items-center justify-between gap-2">
            <button
              className="flex h-7 items-center gap-1.5 rounded border border-neutral-700 px-3 text-xs text-neutral-300 transition-colors hover:border-neutral-600 hover:bg-neutral-900 hover:text-white"
              onClick={() => { setShowCreate(true); setNewProjectName("") }}
              type="button"
            >
              <Plus className="h-3.5 w-3.5" />
              New Project
            </button>

            <div className="flex items-center gap-2">
              {canClose && (
                <button
                  className="h-7 rounded border border-neutral-700 px-3 text-xs text-neutral-300 transition-colors hover:border-neutral-600 hover:bg-neutral-900 hover:text-white"
                  onClick={onClose}
                  type="button"
                >
                  Cancel
                </button>
              )}
              <button
                className="h-7 rounded bg-white px-4 text-xs font-medium text-black transition-colors hover:bg-neutral-200 disabled:cursor-not-allowed disabled:opacity-50"
                disabled={busy || !selected}
                onClick={() => void handleOpen()}
                type="button"
              >
                {busy ? (
                  <Loader2 className="h-3.5 w-3.5 animate-spin" />
                ) : selectedIsActive ? (
                  "Reload"
                ) : (
                  "Open"
                )}
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
