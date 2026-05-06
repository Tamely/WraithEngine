"use client"

import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react"

export type RemoteViewportConnectionState =
  | "idle"
  | "connecting"
  | "awaiting-backend"
  | "awaiting-media"
  | "streaming"
  | "control-ready"
  | "unsupported"
  | "error"

export type RemoteViewportViewMode = "lit" | "unlit" | "wireframe"

interface RemoteViewportActions {
  reconnect: () => Promise<void>
  toggleLook: () => Promise<void>
  setMode: (mode: RemoteViewportViewMode) => Promise<void>
}

interface RemoteViewportContextValue {
  connectionState: RemoteViewportConnectionState
  statusText: string
  detailText: string
  frameText: string
  viewMode: RemoteViewportViewMode
  isLooking: boolean
  eventLog: string[]
  serverOrigin: string
  setConnectionState: (value: RemoteViewportConnectionState) => void
  setStatusText: (value: string) => void
  setDetailText: (value: string) => void
  setFrameText: (value: string) => void
  setViewMode: (value: RemoteViewportViewMode) => void
  setIsLooking: (value: boolean) => void
  setServerOrigin: (value: string) => void
  appendEventLog: (value: string) => void
  clearEventLog: () => void
  registerActions: (actions: RemoteViewportActions) => void
  reconnect: () => Promise<void>
  toggleLook: () => Promise<void>
  setMode: (mode: RemoteViewportViewMode) => Promise<void>
}

const MAX_LOG_LINES = 10

const RemoteViewportContext = createContext<RemoteViewportContextValue | null>(null)

export function RemoteViewportProvider({ children }: { children: ReactNode }) {
  const actionsRef = useRef<RemoteViewportActions>({
    reconnect: async () => {},
    toggleLook: async () => {},
    setMode: async () => {},
  })
  const [connectionState, setConnectionState] =
    useState<RemoteViewportConnectionState>("idle")
  const [statusText, setStatusText] = useState("Ready to connect")
  const [detailText, setDetailText] = useState("Waiting for remote viewport stream")
  const [frameText, setFrameText] = useState("No stream metadata yet")
  const [viewMode, setViewMode] = useState<RemoteViewportViewMode>("lit")
  const [isLooking, setIsLooking] = useState(false)
  const [eventLog, setEventLog] = useState<string[]>([])
  const [serverOrigin, setServerOrigin] = useState("")

  const appendEventLog = useCallback((value: string) => {
    setEventLog((current) => [value, ...current].slice(0, MAX_LOG_LINES))
  }, [])

  const clearEventLog = useCallback(() => {
    setEventLog([])
  }, [])

  const registerActions = useCallback((actions: RemoteViewportActions) => {
    actionsRef.current = actions
  }, [])

  const reconnect = useCallback(async () => {
    await actionsRef.current.reconnect()
  }, [])

  const toggleLook = useCallback(async () => {
    await actionsRef.current.toggleLook()
  }, [])

  const setMode = useCallback(async (mode: RemoteViewportViewMode) => {
    await actionsRef.current.setMode(mode)
  }, [])

  const value = useMemo(
    () => ({
      connectionState,
      statusText,
      detailText,
      frameText,
      viewMode,
      isLooking,
      eventLog,
      serverOrigin,
      setConnectionState,
      setStatusText,
      setDetailText,
      setFrameText,
      setViewMode,
      setIsLooking,
      setServerOrigin,
      appendEventLog,
      clearEventLog,
      registerActions,
      reconnect,
      toggleLook,
      setMode,
    }),
    [
      appendEventLog,
      clearEventLog,
      connectionState,
      detailText,
      eventLog,
      frameText,
      isLooking,
      reconnect,
      registerActions,
      serverOrigin,
      setMode,
      statusText,
      toggleLook,
      viewMode,
    ]
  )

  return (
    <RemoteViewportContext.Provider value={value}>
      {children}
    </RemoteViewportContext.Provider>
  )
}

export function useRemoteViewport() {
  const context = useContext(RemoteViewportContext)
  if (!context) {
    throw new Error("useRemoteViewport must be used within RemoteViewportProvider")
  }
  return context
}
