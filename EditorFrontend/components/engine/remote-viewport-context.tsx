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

export type RemoteSessionState =
  | "idle"
  | "connecting"
  | "snapshot-loading"
  | "session-ready"
  | "reconnecting"
  | "degraded-transport"
  | "command-rejected"
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

export interface SessionParticipant {
  userId: number
  displayName: string
  presenceState: "connected" | "away" | "disconnected"
  isLocal: boolean
  selectionObjectId: string | null
  currentTool: string
  presentationColor: string
  camera: {
    position: [number, number, number]
    yawDegrees: number
    pitchDegrees: number
  } | null
}

export interface SessionTransformDetails {
  location: [number, number, number]
  rotationDegrees: [number, number, number]
  scale: [number, number, number]
}

export interface SessionObjectDetails {
  objectId: string
  displayName: string
  kind: SessionSceneItemKind
  visible: boolean
  capabilities: {
    supportsTransform: boolean
    transformReadOnly: boolean
  }
  transform: SessionTransformDetails | null
  collaboration: {
    selectedByUserIds: number[]
    lockState: "unlocked" | "placeholder"
    lockOwnerUserId: number | null
  }
}

interface RemoteViewportActions {
  reconnect: () => Promise<void>
  toggleLook: () => Promise<void>
  setMode: (mode: RemoteViewportViewMode) => Promise<void>
  refreshSessionSnapshot: () => Promise<void>
  selectObject: (objectId: string) => Promise<boolean>
  updateTransform: (details: SessionObjectTransformUpdate) => Promise<boolean>
}

export interface SessionObjectTransformUpdate {
  objectId: string
  location: [number, number, number]
  rotationDegrees: [number, number, number]
  scale: [number, number, number]
}

interface RemoteViewportContextValue {
  connectionState: RemoteViewportConnectionState
  sessionState: RemoteSessionState
  statusText: string
  detailText: string
  frameText: string
  sessionStatusText: string
  sessionDetailText: string
  viewMode: RemoteViewportViewMode
  isLooking: boolean
  eventLog: string[]
  serverOrigin: string
  currentUserId: number | null
  participants: SessionParticipant[]
  sceneTree: SessionSceneItem[]
  selectedObjectId: string | null
  selectedObject: SessionSceneItem | null
  selectedObjectDetails: SessionObjectDetails | null
  selections: SessionSelection[]
  setConnectionState: (value: RemoteViewportConnectionState) => void
  setSessionState: (value: RemoteSessionState) => void
  setStatusText: (value: string) => void
  setDetailText: (value: string) => void
  setFrameText: (value: string) => void
  setSessionStatusText: (value: string) => void
  setSessionDetailText: (value: string) => void
  setViewMode: (value: RemoteViewportViewMode) => void
  setIsLooking: (value: boolean) => void
  setServerOrigin: (value: string) => void
  appendEventLog: (value: string) => void
  clearEventLog: () => void
  registerActions: (actions: RemoteViewportActions) => void
  setSessionSnapshot: (snapshot: SessionSnapshot) => void
  clearSessionSnapshot: () => void
  applySelectionChanged: (userId: number, objectId: string | null) => void
  applyParticipantCameraUpdate: (
    userId: number,
    camera: SessionParticipant["camera"]
  ) => void
  reconnect: () => Promise<void>
  toggleLook: () => Promise<void>
  setMode: (mode: RemoteViewportViewMode) => Promise<void>
  refreshSessionSnapshot: () => Promise<void>
  selectObject: (objectId: string) => Promise<boolean>
  updateTransform: (details: SessionObjectTransformUpdate) => Promise<boolean>
}

interface SessionSnapshot {
  currentUserId: number
  participants: SessionParticipant[]
  sceneTree: SessionSceneItem[]
  selections: SessionSelection[]
  selectedObjectDetails: SessionObjectDetails | null
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
    updateTransform: async () => false,
  })
  const [connectionState, setConnectionState] =
    useState<RemoteViewportConnectionState>("idle")
  const [sessionState, setSessionState] = useState<RemoteSessionState>("idle")
  const [statusText, setStatusText] = useState("Ready to connect")
  const [detailText, setDetailText] = useState("Waiting for remote viewport stream")
  const [frameText, setFrameText] = useState("No stream metadata yet")
  const [sessionStatusText, setSessionStatusText] = useState("Session idle")
  const [sessionDetailText, setSessionDetailText] = useState(
    "Waiting for authoritative session state"
  )
  const [viewMode, setViewMode] = useState<RemoteViewportViewMode>("lit")
  const [isLooking, setIsLooking] = useState(false)
  const [eventLog, setEventLog] = useState<string[]>([])
  const [serverOrigin, setServerOrigin] = useState("")
  const [currentUserId, setCurrentUserId] = useState<number | null>(null)
  const [participants, setParticipants] = useState<SessionParticipant[]>([])
  const [sceneTree, setSceneTree] = useState<SessionSceneItem[]>([])
  const [selections, setSelections] = useState<SessionSelection[]>([])
  const [selectedObjectDetails, setSelectedObjectDetails] =
    useState<SessionObjectDetails | null>(null)

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
    setParticipants(snapshot.participants)
    setSceneTree(snapshot.sceneTree)
    setSelections(
      snapshot.participants
        .filter((participant) => participant.selectionObjectId !== null)
        .map((participant) => ({
          userId: participant.userId,
          objectId: participant.selectionObjectId as string,
        }))
    )
    setSelectedObjectDetails(snapshot.selectedObjectDetails)
  }, [])

  const clearSessionSnapshot = useCallback(() => {
    setCurrentUserId(null)
    setParticipants([])
    setSceneTree([])
    setSelections([])
    setSelectedObjectDetails(null)
  }, [])

  const applySelectionChanged = useCallback((userId: number, objectId: string | null) => {
    setSelections((current) => {
      const withoutUser = current.filter((selection) => selection.userId !== userId)
      return objectId ? [...withoutUser, { userId, objectId }] : withoutUser
    })
  }, [])

  const applyParticipantCameraUpdate = useCallback(
    (userId: number, camera: SessionParticipant["camera"]) => {
      setParticipants((current) =>
        current.map((participant) =>
          participant.userId === userId ? { ...participant, camera } : participant
        )
      )
    },
    []
  )

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

  const updateTransform = useCallback(async (details: SessionObjectTransformUpdate) => {
    return actionsRef.current.updateTransform(details)
  }, [])

  const selectedObjectId =
    currentUserId !== null
      ? selections.find((selection) => selection.userId === currentUserId)?.objectId ?? null
      : null
  const selectedObject = findSceneItem(sceneTree, selectedObjectId)

  const value = useMemo(
    () => ({
      connectionState,
      sessionState,
      statusText,
      detailText,
      frameText,
      sessionStatusText,
      sessionDetailText,
      viewMode,
      isLooking,
      eventLog,
      serverOrigin,
      currentUserId,
      participants,
      sceneTree,
      selectedObjectId,
      selectedObject,
      selectedObjectDetails,
      selections,
      setConnectionState,
      setSessionState,
      setStatusText,
      setDetailText,
      setFrameText,
      setSessionStatusText,
      setSessionDetailText,
      setViewMode,
      setIsLooking,
      setServerOrigin,
      appendEventLog,
      clearEventLog,
      registerActions,
      setSessionSnapshot,
      clearSessionSnapshot,
      applySelectionChanged,
      applyParticipantCameraUpdate,
      reconnect,
      toggleLook,
      setMode,
      refreshSessionSnapshot,
      selectObject,
      updateTransform,
    }),
    [
      appendEventLog,
      applySelectionChanged,
      applyParticipantCameraUpdate,
      clearEventLog,
      connectionState,
      clearSessionSnapshot,
      currentUserId,
      detailText,
      eventLog,
      frameText,
      isLooking,
      participants,
      sessionDetailText,
      sessionState,
      sessionStatusText,
      reconnect,
      refreshSessionSnapshot,
      registerActions,
      sceneTree,
      selectObject,
      selectedObject,
      selectedObjectDetails,
      selectedObjectId,
      selections,
      serverOrigin,
      setMode,
      setSessionDetailText,
      setSessionState,
      setSessionSnapshot,
      setSessionStatusText,
      statusText,
      toggleLook,
      updateTransform,
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
