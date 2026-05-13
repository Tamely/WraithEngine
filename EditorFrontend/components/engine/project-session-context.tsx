"use client"

import { createContext, useContext, type ReactNode } from "react"
import type { ProjectDescriptor } from "./project-browser"

interface ProjectSessionContextValue {
  activeProject: ProjectDescriptor
  serverOrigin: string
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
  return (
    <ProjectSessionContext.Provider value={{ activeProject, serverOrigin }}>
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
