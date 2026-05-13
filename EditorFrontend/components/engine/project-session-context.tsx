"use client"

import { createContext, useCallback, useContext, useState, type ReactNode } from "react"
import type { ProjectDescriptor } from "./project-browser"

interface ProjectSessionContextValue {
  activeProject: ProjectDescriptor
  serverOrigin: string
  requestedScriptPath: string | null
  requestOpenScript: (path: string) => void
  clearRequestedScriptPath: () => void
}

const ProjectSessionContext = createContext<ProjectSessionContextValue | null>(null)

export function ProjectSessionProvider({
  activeProject,
  serverOrigin,
  children,
}: {
  activeProject: ProjectDescriptor
  serverOrigin: string
  children: ReactNode
}) {
  const [requestedScriptPath, setRequestedScriptPath] = useState<string | null>(null)
  const requestOpenScript = useCallback((path: string) => {
    setRequestedScriptPath(path)
  }, [])
  const clearRequestedScriptPath = useCallback(() => {
    setRequestedScriptPath(null)
  }, [])

  return (
    <ProjectSessionContext.Provider
      value={{
        activeProject,
        serverOrigin,
        requestedScriptPath,
        requestOpenScript,
        clearRequestedScriptPath,
      }}
    >
      {children}
    </ProjectSessionContext.Provider>
  )
}

export function useProjectSession() {
  const context = useContext(ProjectSessionContext)
  if (!context) {
    throw new Error("useProjectSession must be used within ProjectSessionProvider")
  }
  return context
}
