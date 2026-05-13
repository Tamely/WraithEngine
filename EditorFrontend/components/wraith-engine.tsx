"use client"

import { X } from "lucide-react"
import { useCallback, useEffect, useMemo, useState } from "react"
import { MenuBar } from "./engine/menu-bar"
import { ProjectBrowser, type ProjectDescriptor } from "./engine/project-browser"
import { ProjectSessionProvider } from "./engine/project-session-context"
import { getServerOrigin } from "./engine/server-origin"
import { Toolbar } from "./engine/toolbar"
import { DockProvider } from "./engine/dock/dock-context"
import { DockLayout } from "./engine/dock/dock-layout"
import {
  RemoteViewportProvider,
  useRemoteViewport,
} from "./engine/remote-viewport-context"

function ScriptErrorToastOverlay() {
  const { scriptErrorToasts, dismissScriptErrorToast } = useRemoteViewport()
  if (scriptErrorToasts.length === 0) return null

  return (
    <div className="pointer-events-none fixed bottom-4 right-4 z-50 flex flex-col gap-2">
      {scriptErrorToasts.map((toast) => (
        <div
          key={toast.id}
          className="pointer-events-auto flex max-w-sm items-start gap-2 rounded border border-red-800 bg-neutral-950 px-3 py-2.5 shadow-lg"
        >
          <div className="min-w-0 flex-1">
            <p className="text-xs font-medium text-red-400">Script Error</p>
            <p className="mt-0.5 text-[11px] text-neutral-400 truncate">{toast.objectId}</p>
            <p className="mt-1 text-xs text-neutral-300 line-clamp-3">{toast.message}</p>
          </div>
          <button
            className="shrink-0 rounded p-0.5 text-neutral-500 hover:bg-neutral-800 hover:text-neutral-300"
            onClick={() => dismissScriptErrorToast(toast.id)}
            type="button"
          >
            <X className="h-3.5 w-3.5" />
          </button>
        </div>
      ))}
    </div>
  )
}

interface ProjectsResponse {
  type: "projects"
  activeProjectSlug: string | null
  projects: ProjectDescriptor[]
}

interface CurrentProjectResponse {
  type: "current_project"
  project: ProjectDescriptor | null
}

function formatProjectRequestError(serverOrigin: string, action: string, error: unknown) {
  const message = error instanceof Error ? error.message : String(error)
  const normalizedMessage = message.toLowerCase()
  const isNetworkError =
    error instanceof TypeError ||
    normalizedMessage.includes("networkerror") ||
    normalizedMessage.includes("failed to fetch") ||
    normalizedMessage.includes("load failed")

  if (isNetworkError) {
    return `Couldn't ${action} because the Axiom server at ${serverOrigin} isn't reachable. Start AxiomRemoteViewportServer or update NEXT_PUBLIC_AXIOM_SERVER_ORIGIN in EditorFrontend/.env.`
  }

  return message
}

export function WraithEngine() {
  const serverOrigin = useMemo(() => getServerOrigin(), [])
  const [projects, setProjects] = useState<ProjectDescriptor[]>([])
  const [activeProject, setActiveProject] = useState<ProjectDescriptor | null>(null)
  const [projectsLoading, setProjectsLoading] = useState(true)
  const [projectsBusy, setProjectsBusy] = useState(false)
  const [projectsError, setProjectsError] = useState<string | null>(null)
  const [projectBrowserOpen, setProjectBrowserOpen] = useState(false)
  const [editorGeneration, setEditorGeneration] = useState(0)

  const fetchJson = useCallback(
    async <T,>(path: string, init?: RequestInit) => {
      const response = await fetch(`${serverOrigin}${path}`, {
        cache: "no-store",
        ...init,
      })
      const text = await response.text()
      const payload = text.length > 0 ? (JSON.parse(text) as T & { message?: string }) : null
      if (!response.ok) {
        throw new Error(payload?.message ?? `${response.status} ${response.statusText}`)
      }
      return payload as T
    },
    [serverOrigin]
  )

  const refreshProjects = useCallback(async () => {
    setProjectsLoading(true)
    setProjectsError(null)
    try {
      const payload = await fetchJson<ProjectsResponse>("/projects")
      setProjects(payload.projects)
      const nextActive =
        payload.activeProjectSlug !== null
          ? payload.projects.find((project) => project.slug === payload.activeProjectSlug) ?? null
          : null
      setActiveProject(nextActive)
      setProjectBrowserOpen((current) => (nextActive === null ? true : current))
    } catch (error) {
      setProjectsError(formatProjectRequestError(serverOrigin, "load projects", error))
      setProjectBrowserOpen(true)
    } finally {
      setProjectsLoading(false)
    }
  }, [fetchJson])

  useEffect(() => {
    void refreshProjects()
  }, [refreshProjects])

  const handleCreateProject = useCallback(
    async (name: string) => {
      setProjectsBusy(true)
      setProjectsError(null)
      try {
        const payload = await fetchJson<CurrentProjectResponse>("/projects/create", {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
          },
          body: JSON.stringify({ name }),
        })
        setActiveProject(payload.project)
        await refreshProjects()
        setProjectBrowserOpen(false)
        setEditorGeneration((current) => current + 1)
      } catch (error) {
        setProjectsError(formatProjectRequestError(serverOrigin, "create a project", error))
      } finally {
        setProjectsBusy(false)
      }
    },
    [fetchJson, refreshProjects]
  )

  const handleOpenProject = useCallback(
    async (slug: string) => {
      setProjectsBusy(true)
      setProjectsError(null)
      try {
        const payload = await fetchJson<CurrentProjectResponse>("/projects/open", {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
          },
          body: JSON.stringify({ slug }),
        })
        setActiveProject(payload.project)
        await refreshProjects()
        setProjectBrowserOpen(false)
        setEditorGeneration((current) => current + 1)
      } catch (error) {
        setProjectsError(formatProjectRequestError(serverOrigin, "open a project", error))
      } finally {
        setProjectsBusy(false)
      }
    },
    [fetchJson, refreshProjects]
  )

  const handleShowNewProject = useCallback(() => {
    setProjectsError(null)
    setProjectBrowserOpen(true)
  }, [])

  const handleShowOpenProject = useCallback(() => {
    setProjectsError(null)
    setActiveProject(null)
    setProjectBrowserOpen(true)
    setEditorGeneration((current) => current + 1)
  }, [])

  const showProjectBrowser = projectBrowserOpen || activeProject === null

  return (
    <RemoteViewportProvider>
      <div className="relative h-screen overflow-hidden bg-black text-white">
        {activeProject ? (
          <DockProvider key={`${activeProject.slug}-${editorGeneration}`}>
            <ProjectSessionProvider activeProject={activeProject} serverOrigin={serverOrigin}>
              <div className="relative flex h-screen flex-col overflow-hidden bg-black text-white">
                <MenuBar
                  activeProject={activeProject}
                  onNewProject={handleShowNewProject}
                  onOpenProject={handleShowOpenProject}
                />
                <Toolbar />
                <DockLayout />
                <ScriptErrorToastOverlay />
              </div>
            </ProjectSessionProvider>
          </DockProvider>
        ) : null}

        {showProjectBrowser ? (
          <ProjectBrowser
            activeProject={activeProject}
            busy={projectsBusy}
            canClose={activeProject !== null}
            error={projectsError}
            loading={projectsLoading}
            onClose={() => setProjectBrowserOpen(false)}
            onCreateProject={handleCreateProject}
            onOpenProject={handleOpenProject}
            onRefresh={refreshProjects}
            projects={projects}
            serverOrigin={serverOrigin}
          />
        ) : null}
      </div>
    </RemoteViewportProvider>
  )
}
