"use client"

import { FolderOpen, Loader2, Plus, RefreshCw, Sparkles } from "lucide-react"
import { useMemo, useState } from "react"

export interface ProjectDescriptor {
  projectId: string
  name: string
  slug: string
  rootPath: string
  contentDir: string
  scriptsDir: string
  scriptProjectPath: string
  scriptSolutionPath: string
  scriptAssemblyName: string
  scriptRootNamespace: string
  starterScriptPath: string
  starterScriptClassName: string
  starterScriptQualifiedClassName: string
  cookedDir: string
  cookManifestPath: string
  buildDir: string
  packageDir: string
  packagedContentDir: string
  packagedCookedDir: string
  packagedSceneFilePath: string
  packageManifestPath: string
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
  serverOrigin,
  canClose,
  onClose,
  onRefresh,
  onOpenProject,
  onCreateProject,
}: ProjectBrowserProps) {
  const [projectName, setProjectName] = useState("")
  const sortedProjects = useMemo(
    () => [...projects].sort((left, right) => left.name.localeCompare(right.name)),
    [projects]
  )

  async function handleCreateProject() {
    const trimmed = projectName.trim()
    if (!trimmed) return
    await onCreateProject(trimmed)
    setProjectName("")
  }

  return (
    <div className="absolute inset-0 z-40 flex min-h-0 min-w-0 items-stretch bg-black text-white">
      <div className="flex w-full min-h-0 flex-col lg:flex-row">
        <section className="flex w-full flex-col justify-between border-b border-neutral-800 bg-neutral-950 px-6 py-6 backdrop-blur-xl lg:w-[32rem] lg:border-b-0 lg:border-r">
          <div>
            <div className="flex items-center justify-between">
              <div className="inline-flex items-center gap-2 rounded-full border border-white/20 bg-white/5 px-3 py-1 text-[10px] font-semibold uppercase tracking-[0.28em] text-white">
                <Sparkles className="h-3.5 w-3.5" />
                Project Workspace
              </div>
              {canClose ? (
                <button
                  className="rounded-full border border-neutral-700 px-3 py-1.5 text-xs text-neutral-300 transition-colors hover:border-neutral-600 hover:bg-neutral-900 hover:text-white"
                  onClick={onClose}
                  type="button"
                >
                  Return to Editor
                </button>
              ) : null}
            </div>

            <h1 className="mt-6 max-w-md text-3xl font-semibold tracking-tight text-white">
              Choose the project that owns this editor session.
            </h1>
            <p className="mt-3 max-w-md text-sm leading-6 text-neutral-400">
              New projects get their own content root and scene file. Shared engine assets stay
              available from the global engine content directory.
            </p>

            <div className="mt-8 rounded-3xl border border-neutral-800 bg-neutral-900/50 p-5 shadow-[0_24px_80px_rgba(0,0,0,0.35)]">
              <div className="flex items-center justify-between gap-4">
                <div>
                  <p className="text-xs font-semibold uppercase tracking-[0.2em] text-neutral-500">
                    Create Project
                  </p>
                  <p className="mt-1 text-sm text-neutral-400">
                    Start a clean workspace under the managed host projects root.
                  </p>
                </div>
                <Plus className="h-5 w-5 text-white" />
              </div>

              <div className="mt-4 flex flex-col gap-3">
                <input
                  className="h-11 rounded-2xl border border-neutral-800 bg-neutral-900 px-4 text-sm text-white outline-none transition-colors placeholder:text-neutral-600 focus:border-neutral-700 focus:bg-neutral-800/80"
                  disabled={busy}
                  onChange={(event) => setProjectName(event.target.value)}
                  onKeyDown={(event) => {
                    if (event.key === "Enter") {
                      event.preventDefault()
                      void handleCreateProject()
                    }
                  }}
                  placeholder="Project name"
                  type="text"
                  value={projectName}
                />
                <button
                  className="inline-flex h-11 items-center justify-center gap-2 rounded-2xl bg-white px-4 text-sm font-medium text-black transition-transform hover:bg-neutral-100 hover:scale-[1.01] disabled:cursor-not-allowed disabled:opacity-50"
                  disabled={busy || projectName.trim().length === 0}
                  onClick={() => void handleCreateProject()}
                  type="button"
                >
                  {busy ? <Loader2 className="h-4 w-4 animate-spin" /> : <Plus className="h-4 w-4" />}
                  Create Project
                </button>
              </div>
            </div>

            {error ? (
              <div className="mt-4 rounded-2xl border border-red-900/40 bg-red-950/30 px-4 py-3 text-sm text-red-300">
                {error}
              </div>
            ) : null}
          </div>

          <div className="mt-6 rounded-3xl border border-neutral-800 bg-neutral-900/50 p-4 text-xs text-neutral-400">
            <div className="flex items-center justify-between gap-4">
              <span className="uppercase tracking-[0.2em] text-neutral-500">Server</span>
              <span className="truncate text-neutral-300">{serverOrigin}</span>
            </div>
            <div className="mt-3 flex items-center justify-between gap-4">
              <span className="uppercase tracking-[0.2em] text-neutral-500">Shared Engine</span>
              <span className="truncate text-neutral-300">
                {activeProject?.engineContentDir ?? "Content/Engine"}
              </span>
            </div>
          </div>
        </section>

        <section className="flex min-h-0 flex-1 flex-col px-6 py-6">
          <div className="flex items-end justify-between gap-4">
            <div>
              <p className="text-xs font-semibold uppercase tracking-[0.2em] text-neutral-500">
                Open Project
              </p>
              <h2 className="mt-2 text-2xl font-semibold text-white">
                Managed Projects
              </h2>
            </div>
            <button
              className="inline-flex h-10 items-center gap-2 rounded-2xl border border-neutral-800 px-4 text-sm text-neutral-400 transition-colors hover:border-neutral-700 hover:bg-neutral-900 hover:text-white disabled:opacity-50"
              disabled={busy || loading}
              onClick={() => void onRefresh()}
              type="button"
            >
              {loading ? <Loader2 className="h-4 w-4 animate-spin" /> : <RefreshCw className="h-4 w-4" />}
              Refresh
            </button>
          </div>

          <div className="mt-5 grid min-h-0 flex-1 grid-cols-1 gap-4 overflow-y-auto pr-1 xl:grid-cols-2">
            {sortedProjects.length === 0 ? (
              <div className="col-span-full flex min-h-[18rem] flex-col items-center justify-center rounded-[2rem] border border-dashed border-neutral-800 bg-neutral-900/30 px-8 text-center">
                <FolderOpen className="h-10 w-10 text-neutral-700" />
                <p className="mt-4 text-lg font-medium text-white">No projects yet</p>
                <p className="mt-2 max-w-md text-sm leading-6 text-neutral-400">
                  Create your first project to start a dedicated content workspace and editor
                  session.
                </p>
              </div>
            ) : null}

            {sortedProjects.map((project) => {
              const isActive = activeProject?.slug === project.slug
              return (
                <article
                  key={project.projectId}
                  className={`group flex flex-col justify-between rounded-[2rem] border p-5 transition-all ${isActive
                      ? "border-white/20 bg-white/10 shadow-[0_20px_60px_rgba(255,255,255,0.08)]"
                      : "border-neutral-800 bg-neutral-900/40 hover:border-neutral-700 hover:bg-neutral-900/60"
                    }`}
                >
                  <div>
                    <div className="flex items-center justify-between gap-3">
                      <div>
                        <h3 className="text-lg font-semibold text-white">{project.name}</h3>
                        <p className="mt-1 text-xs uppercase tracking-[0.18em] text-neutral-500">
                          {project.slug}
                        </p>
                      </div>
                      {isActive ? (
                        <span className="rounded-full border border-white/20 bg-white/10 px-3 py-1 text-[10px] font-semibold uppercase tracking-[0.18em] text-white">
                          Active
                        </span>
                      ) : null}
                    </div>

                    <dl className="mt-5 space-y-3 text-sm text-neutral-400">
                      <div>
                        <dt className="text-[10px] uppercase tracking-[0.18em] text-neutral-500">
                          Project Content
                        </dt>
                        <dd className="mt-1 break-all text-neutral-300">{project.contentDir}</dd>
                      </div>
                      <div>
                        <dt className="text-[10px] uppercase tracking-[0.18em] text-neutral-500">
                          Shared Engine Content
                        </dt>
                        <dd className="mt-1 break-all text-neutral-300">{project.engineContentDir}</dd>
                      </div>
                    </dl>
                  </div>

                  <div className="mt-6 flex items-center justify-between gap-3">
                    <p className="truncate text-xs text-neutral-500">{project.rootPath}</p>
                    <button
                      className={`inline-flex h-10 shrink-0 items-center gap-2 rounded-2xl px-4 text-sm font-medium transition-colors ${isActive
                          ? "bg-neutral-800 text-white hover:bg-neutral-700"
                          : "bg-white text-black hover:bg-neutral-100"
                        }`}
                      disabled={busy}
                      onClick={() => void onOpenProject(project.slug)}
                      type="button"
                    >
                      {busy ? <Loader2 className="h-4 w-4 animate-spin" /> : <FolderOpen className="h-4 w-4" />}
                      {isActive ? "Reload Project" : "Open Project"}
                    </button>
                  </div>
                </article>
              )
            })}
          </div>
        </section>
      </div>
    </div>
  )
}
