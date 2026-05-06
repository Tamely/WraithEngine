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
export type SessionSceneItemKind = "folder" | "mesh" | "light" | "camera" | "actor"

export interface SessionSceneItem {
  id: string
  displayName: string
  kind: SessionSceneItemKind
  visible: boolean
  children: SessionSceneItem[]
}

export interface SessionSelection {
  userId: number
  objectId: string
}

interface RemoteViewportActions {
  reconnect: () => Promise<void>
  toggleLook: () => Promise<void>
  setMode: (mode: RemoteViewportViewMode) => Promise<void>
  refreshSessionSnapshot: () => Promise<void>
  selectObject: (objectId: string) => Promise<boolean>
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
  currentUserId: number | null
  sceneTree: SessionSceneItem[]
  selectedObjectId: string | null
  selectedObject: SessionSceneItem | null
  selections: SessionSelection[]
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
  setSessionSnapshot: (snapshot: SessionSnapshot) => void
  applySelectionChanged: (userId: number, objectId: string | null) => void
  reconnect: () => Promise<void>
  toggleLook: () => Promise<void>
  setMode: (mode: RemoteViewportViewMode) => Promise<void>
  refreshSessionSnapshot: () => Promise<void>
  selectObject: (objectId: string) => Promise<boolean>
}

interface SessionSnapshot {
  currentUserId: number
  sceneTree: SessionSceneItem[]
  selections: SessionSelection[]
}

const MAX_LOG_LINES = 10

const RemoteViewportContext = createContext<RemoteViewportContextValue | null>(null)

function findSceneItem(items: SessionSceneItem[], objectId: string | null): SessionSceneItem | null {
  if (!objectId) {
    return null
  }

  for (const item of items) {
    if (item.id === objectId) {
      return item
    }

    const child = findSceneItem(item.children, objectId)
    if (child) {
      return child
    }
  }

  return null
}

export function RemoteViewportProvider({ children }: { children: ReactNode }) {
  const actionsRef = useRef<RemoteViewportActions>({
    reconnect: async () => {},
    toggleLook: async () => {},
    setMode: async () => {},
    refreshSessionSnapshot: async () => {},
    selectObject: async () => false,
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
  const [currentUserId, setCurrentUserId] = useState<number | null>(null)
  const [sceneTree, setSceneTree] = useState<SessionSceneItem[]>([])
  const [selections, setSelections] = useState<SessionSelection[]>([])

  const appendEventLog = useCallback((value: string) => {
    setEventLog((current) => [value, ...current].slice(0, MAX_LOG_LINES))
  }, [])

  const clearEventLog = useCallback(() => {
    setEventLog([])
  }, [])

  const registerActions = useCallback((actions: RemoteViewportActions) => {
    actionsRef.current = actions
  }, [])

  const setSessionSnapshot = useCallback((snapshot: SessionSnapshot) => {
    setCurrentUserId(snapshot.currentUserId)
    setSceneTree(snapshot.sceneTree)
    setSelections(snapshot.selections)
  }, [])

  const applySelectionChanged = useCallback((userId: number, objectId: string | null) => {
    setSelections((current) => {
      const withoutUser = current.filter((selection) => selection.userId !== userId)
      return objectId ? [...withoutUser, { userId, objectId }] : withoutUser
    })
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

  const refreshSessionSnapshot = useCallback(async () => {
    await actionsRef.current.refreshSessionSnapshot()
  }, [])

  const selectObject = useCallback(async (objectId: string) => {
    return actionsRef.current.selectObject(objectId)
  }, [])

  const selectedObjectId =
    currentUserId !== null
      ? selections.find((selection) => selection.userId === currentUserId)?.objectId ?? null
      : null
  const selectedObject = findSceneItem(sceneTree, selectedObjectId)

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
      currentUserId,
      sceneTree,
      selectedObjectId,
      selectedObject,
      selections,
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
      setSessionSnapshot,
      applySelectionChanged,
      reconnect,
      toggleLook,
      setMode,
      refreshSessionSnapshot,
      selectObject,
    }),
    [
      appendEventLog,
      applySelectionChanged,
      clearEventLog,
      connectionState,
      currentUserId,
      detailText,
      eventLog,
      frameText,
      isLooking,
      reconnect,
      refreshSessionSnapshot,
      registerActions,
      sceneTree,
      selectObject,
      selectedObject,
      selectedObjectId,
      selections,
      serverOrigin,
      setMode,
      setSessionSnapshot,
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
