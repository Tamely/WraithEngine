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

export type RemoteRuntimeState = "edit" | "playing" | "paused"

export type RemoteViewportViewMode = "lit" | "unlit" | "wireframe"
export type RemoteViewportGizmoMode = "translate" | "scale" | "rotate"
export type SessionSceneItemKind = "folder" | "mesh" | "light" | "camera" | "actor"

export interface RemoteViewportGridSnapSettings {
  enabled: boolean
  translationStep: number
  rotationStepDegrees: number
  scaleStep: number
}

export interface SessionAssetDescriptor {
  id: number
  name: string
  kind: "mesh" | "texture"
  path: string
}

export interface SessionPropertyDescriptor {
  name: string
  type: "string" | "bool" | "number" | "vec3" | "enum"
  readOnly: boolean
  value?: string
}

export interface ScriptErrorToast {
  id: string
  objectId: string
  message: string
}

export interface SessionObjectSchema {
  objectId: string
  className: string
  properties: SessionPropertyDescriptor[]
}

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

export interface SessionLightDetails {
  color: [number, number, number]
  intensity: number
}

export interface SessionMaterialDetails {
  baseColorFactor: [number, number, number, number]
  metallic: number
  roughness: number
  textureAssetPath: string | null
}

export interface SessionPhysicsDetails {
  bodyType: "none" | "static" | "dynamic"
  colliderType: "none" | "box" | "sphere"
  boxHalfExtents: [number, number, number]
  sphereRadius: number
  mass: number
  friction: number
  restitution: number
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
  light: SessionLightDetails | null
  material: SessionMaterialDetails | null
  physics: SessionPhysicsDetails | null
  collaboration: {
    selectedByUserIds: number[]
    lockState: "unlocked" | "locked"
    lockOwnerUserId: number | null
  }
}

interface RemoteViewportActions {
  reconnect: () => Promise<void>
  toggleLook: () => Promise<void>
  setMode: (mode: RemoteViewportViewMode) => Promise<void>
  setGizmoMode: (mode: RemoteViewportGizmoMode) => Promise<void>
  setGridSnapSettings: (settings: RemoteViewportGridSnapSettings) => Promise<void>
  refreshSessionSnapshot: () => Promise<void>
  selectObject: (objectId: string) => Promise<boolean>
  renameObject: (objectId: string, displayName: string) => Promise<boolean>
  setObjectVisibility: (objectId: string, visible: boolean) => Promise<boolean>
  goToParticipantCamera: (userId: number) => Promise<boolean>
  updateTransform: (details: SessionObjectTransformUpdate) => Promise<boolean>
  createObject: (templateId: string) => Promise<boolean>
  duplicateObject: (objectId: string) => Promise<boolean>
  deleteObject: (objectId: string) => Promise<boolean>
  reparentObject: (objectId: string, newParentId: string) => Promise<boolean>
  listAssets: () => Promise<void>
  getSchema: (objectId: string) => Promise<void>
  saveScene: () => Promise<void>
  playSession: () => Promise<boolean>
  pauseSession: () => Promise<boolean>
  resumeSession: () => Promise<boolean>
  stopSession: () => Promise<boolean>
  setProperty: (
    objectId: string,
    property: string,
    value: string | number | boolean | [number, number, number]
  ) => Promise<boolean>
  reloadScripts: () => Promise<void>
  setMeshAsset: (objectId: string, assetPath: string) => Promise<boolean>
  setLightProperties: (
    objectId: string,
    color: [number, number, number],
    intensity: number
  ) => Promise<boolean>
  setMaterialProperties: (
    objectId: string,
    baseColorFactor: [number, number, number, number],
    metallic: number,
    roughness: number
  ) => Promise<boolean>
  setMaterialTexture: (objectId: string, textureAssetPath: string) => Promise<boolean>
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
  runtimeState: RemoteRuntimeState
  canControlRuntime: boolean
  viewMode: RemoteViewportViewMode
  gizmoMode: RemoteViewportGizmoMode
  gridSnapSettings: RemoteViewportGridSnapSettings
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
  applyObjectLockChanged: (
    objectId: string,
    lockState: "unlocked" | "locked",
    lockOwnerUserId: number | null
  ) => void
  lockedObjects: Record<string, number>
  assets: SessionAssetDescriptor[]
  setAssets: (assets: SessionAssetDescriptor[]) => void
  listAssets: () => Promise<void>
  objectSchema: SessionObjectSchema | null
  setObjectSchema: (schema: SessionObjectSchema) => void
  getSchema: (objectId: string) => Promise<void>
  saveScene: () => Promise<void>
  playSession: () => Promise<boolean>
  pauseSession: () => Promise<boolean>
  resumeSession: () => Promise<boolean>
  stopSession: () => Promise<boolean>
  saveStatus: "idle" | "saving" | "saved" | "failed"
  setSaveStatus: (status: "idle" | "saving" | "saved" | "failed") => void
  setProperty: (
    objectId: string,
    property: string,
    value: string | number | boolean | [number, number, number]
  ) => Promise<boolean>
  reloadScripts: () => Promise<void>
  setMeshAsset: (objectId: string, assetPath: string) => Promise<boolean>
  setLightProperties: (
    objectId: string,
    color: [number, number, number],
    intensity: number
  ) => Promise<boolean>
  setMaterialProperties: (
    objectId: string,
    baseColorFactor: [number, number, number, number],
    metallic: number,
    roughness: number
  ) => Promise<boolean>
  setMaterialTexture: (objectId: string, textureAssetPath: string) => Promise<boolean>
  reloadStatus: "idle" | "reloading" | "reloaded" | "failed"
  setReloadStatus: (status: "idle" | "reloading" | "reloaded" | "failed") => void
  scriptErrorToasts: ScriptErrorToast[]
  addScriptErrorToast: (objectId: string, message: string) => void
  dismissScriptErrorToast: (id: string) => void
  reconnect: () => Promise<void>
  toggleLook: () => Promise<void>
  setMode: (mode: RemoteViewportViewMode) => Promise<void>
  setGizmoMode: (mode: RemoteViewportGizmoMode) => Promise<void>
  setGridSnapSettings: (settings: RemoteViewportGridSnapSettings) => Promise<void>
  refreshSessionSnapshot: () => Promise<void>
  selectObject: (objectId: string) => Promise<boolean>
  renameObject: (objectId: string, displayName: string) => Promise<boolean>
  setObjectVisibility: (objectId: string, visible: boolean) => Promise<boolean>
  goToParticipantCamera: (userId: number) => Promise<boolean>
  updateTransform: (details: SessionObjectTransformUpdate) => Promise<boolean>
  createObject: (templateId: string) => Promise<boolean>
  duplicateObject: (objectId: string) => Promise<boolean>
  deleteObject: (objectId: string) => Promise<boolean>
  reparentObject: (objectId: string, newParentId: string) => Promise<boolean>
}

interface SessionSnapshot {
  currentUserId: number
  runtimeControllerUserId: number
  runtimeState: RemoteRuntimeState
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
  const defaultGridSnapSettings: RemoteViewportGridSnapSettings = {
    enabled: true,
    translationStep: 1,
    rotationStepDegrees: 15,
    scaleStep: 0.1,
  }

  const actionsRef = useRef<RemoteViewportActions>({
    reconnect: async () => {},
    toggleLook: async () => {},
    setMode: async () => {},
    setGizmoMode: async () => {},
    setGridSnapSettings: async () => {},
    refreshSessionSnapshot: async () => {},
    selectObject: async () => false,
    renameObject: async () => false,
    setObjectVisibility: async () => false,
    goToParticipantCamera: async () => false,
    updateTransform: async () => false,
    createObject: async () => false,
    duplicateObject: async () => false,
    deleteObject: async () => false,
    reparentObject: async () => false,
    listAssets: async () => {},
    getSchema: async () => {},
    saveScene: async () => {},
    playSession: async () => false,
    pauseSession: async () => false,
    resumeSession: async () => false,
    stopSession: async () => false,
    setProperty: async () => false,
    reloadScripts: async () => {},
    setMeshAsset: async () => false,
    setLightProperties: async () => false,
    setMaterialProperties: async () => false,
    setMaterialTexture: async () => false,
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
  const [runtimeState, setRuntimeState] = useState<RemoteRuntimeState>("edit")
  const [viewMode, setViewMode] = useState<RemoteViewportViewMode>("lit")
  const [gizmoMode, setGizmoModeState] = useState<RemoteViewportGizmoMode>("translate")
  const [gridSnapSettings, setGridSnapSettingsState] =
    useState<RemoteViewportGridSnapSettings>(defaultGridSnapSettings)
  const [isLooking, setIsLooking] = useState(false)
  const [eventLog, setEventLog] = useState<string[]>([])
  const [serverOrigin, setServerOrigin] = useState("")
  const [currentUserId, setCurrentUserId] = useState<number | null>(null)
  const [runtimeControllerUserId, setRuntimeControllerUserId] = useState<number | null>(null)
  const [participants, setParticipants] = useState<SessionParticipant[]>([])
  const [sceneTree, setSceneTree] = useState<SessionSceneItem[]>([])
  const [selections, setSelections] = useState<SessionSelection[]>([])
  const [selectedObjectDetails, setSelectedObjectDetails] =
    useState<SessionObjectDetails | null>(null)
  const [lockedObjects, setLockedObjects] = useState<Record<string, number>>({})
  const [assets, setAssets] = useState<SessionAssetDescriptor[]>([])
  const [objectSchema, setObjectSchema] = useState<SessionObjectSchema | null>(null)
  const [saveStatus, setSaveStatus] = useState<"idle" | "saving" | "saved" | "failed">("idle")
  const [reloadStatus, setReloadStatus] = useState<"idle" | "reloading" | "reloaded" | "failed">("idle")
  const [scriptErrorToasts, setScriptErrorToasts] = useState<ScriptErrorToast[]>([])

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
    setRuntimeControllerUserId(snapshot.runtimeControllerUserId)
    setRuntimeState(snapshot.runtimeState)
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
    setRuntimeControllerUserId(null)
    setRuntimeState("edit")
    setParticipants([])
    setSceneTree([])
    setSelections([])
    setSelectedObjectDetails(null)
    setLockedObjects({})
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

  const applyObjectLockChanged = useCallback(
    (objectId: string, lockState: "unlocked" | "locked", lockOwnerUserId: number | null) => {
      setSelectedObjectDetails((current) => {
        if (!current || current.objectId !== objectId) return current
        return {
          ...current,
          collaboration: {
            ...current.collaboration,
            lockState,
            lockOwnerUserId,
          },
        }
      })
      setLockedObjects((current) => {
        if (lockState === "locked" && lockOwnerUserId !== null) {
          return { ...current, [objectId]: lockOwnerUserId }
        }
        const next = { ...current }
        delete next[objectId]
        return next
      })
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

  const setGizmoModeAction = useCallback(async (mode: RemoteViewportGizmoMode) => {
    setGizmoModeState(mode)
    await actionsRef.current.setGizmoMode(mode)
  }, [])

  const setGridSnapSettings = useCallback(async (settings: RemoteViewportGridSnapSettings) => {
    setGridSnapSettingsState(settings)
    await actionsRef.current.setGridSnapSettings(settings)
  }, [])

  const refreshSessionSnapshot = useCallback(async () => {
    await actionsRef.current.refreshSessionSnapshot()
  }, [])

  const selectObject = useCallback(async (objectId: string) => {
    return actionsRef.current.selectObject(objectId)
  }, [])

  const goToParticipantCamera = useCallback(async (userId: number) => {
    return actionsRef.current.goToParticipantCamera(userId)
  }, [])

  const renameObject = useCallback(async (objectId: string, displayName: string) => {
    return actionsRef.current.renameObject(objectId, displayName)
  }, [])

  const setObjectVisibility = useCallback(async (objectId: string, visible: boolean) => {
    return actionsRef.current.setObjectVisibility(objectId, visible)
  }, [])

  const updateTransform = useCallback(async (details: SessionObjectTransformUpdate) => {
    return actionsRef.current.updateTransform(details)
  }, [])

  const createObject = useCallback(async (templateId: string) => {
    return actionsRef.current.createObject(templateId)
  }, [])

  const duplicateObject = useCallback(async (objectId: string) => {
    return actionsRef.current.duplicateObject(objectId)
  }, [])

  const deleteObject = useCallback(async (objectId: string) => {
    return actionsRef.current.deleteObject(objectId)
  }, [])

  const reparentObject = useCallback(async (objectId: string, newParentId: string) => {
    return actionsRef.current.reparentObject(objectId, newParentId)
  }, [])

  const listAssets = useCallback(async () => {
    await actionsRef.current.listAssets()
  }, [])

  const getSchema = useCallback(async (objectId: string) => {
    await actionsRef.current.getSchema(objectId)
  }, [])

  const saveScene = useCallback(async () => {
    setSaveStatus("saving")
    await actionsRef.current.saveScene()
  }, [])

  const playSession = useCallback(async () => actionsRef.current.playSession(), [])

  const pauseSession = useCallback(async () => actionsRef.current.pauseSession(), [])

  const resumeSession = useCallback(async () => actionsRef.current.resumeSession(), [])

  const stopSession = useCallback(async () => actionsRef.current.stopSession(), [])

  const setProperty = useCallback(
    async (
      objectId: string,
      property: string,
      value: string | number | boolean | [number, number, number]
    ) => actionsRef.current.setProperty(objectId, property, value),
    []
  )

  const reloadScripts = useCallback(async () => {
    setReloadStatus("reloading")
    await actionsRef.current.reloadScripts()
  }, [])

  const setMeshAsset = useCallback(
    async (objectId: string, assetPath: string) =>
      actionsRef.current.setMeshAsset(objectId, assetPath),
    []
  )

  const setLightProperties = useCallback(
    async (objectId: string, color: [number, number, number], intensity: number) =>
      actionsRef.current.setLightProperties(objectId, color, intensity),
    []
  )

  const setMaterialProperties = useCallback(
    async (
      objectId: string,
      baseColorFactor: [number, number, number, number],
      metallic: number,
      roughness: number
    ) => actionsRef.current.setMaterialProperties(objectId, baseColorFactor, metallic, roughness),
    []
  )

  const setMaterialTexture = useCallback(
    async (objectId: string, textureAssetPath: string) =>
      actionsRef.current.setMaterialTexture(objectId, textureAssetPath),
    []
  )

  const addScriptErrorToast = useCallback((objectId: string, message: string) => {
    const id = `${Date.now()}-${Math.random().toString(16).slice(2)}`
    setScriptErrorToasts((current) => [...current, { id, objectId, message }])
  }, [])

  const dismissScriptErrorToast = useCallback((id: string) => {
    setScriptErrorToasts((current) => current.filter((toast) => toast.id !== id))
  }, [])

  const selectedObjectId =
    currentUserId !== null
      ? selections.find((selection) => selection.userId === currentUserId)?.objectId ?? null
      : null
  const selectedObject = findSceneItem(sceneTree, selectedObjectId)
  const canControlRuntime =
    currentUserId !== null &&
    runtimeControllerUserId !== null &&
    currentUserId === runtimeControllerUserId

  const value = useMemo(
    () => ({
      connectionState,
      sessionState,
      statusText,
      detailText,
      frameText,
      sessionStatusText,
      sessionDetailText,
      runtimeState,
      canControlRuntime,
      viewMode,
      gizmoMode,
      gridSnapSettings,
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
      lockedObjects,
      assets,
      setAssets,
      listAssets,
      objectSchema,
      setObjectSchema,
      getSchema,
      saveScene,
      playSession,
      pauseSession,
      resumeSession,
      stopSession,
      saveStatus,
      setSaveStatus,
      setProperty,
      reloadScripts,
      setMeshAsset,
      setLightProperties,
      setMaterialProperties,
      setMaterialTexture,
      reloadStatus,
      setReloadStatus,
      scriptErrorToasts,
      addScriptErrorToast,
      dismissScriptErrorToast,
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
      applyObjectLockChanged,
      reconnect,
      toggleLook,
      setMode,
      setGizmoMode: setGizmoModeAction,
      setGridSnapSettings,
      refreshSessionSnapshot,
      selectObject,
      renameObject,
      setObjectVisibility,
      goToParticipantCamera,
      updateTransform,
      createObject,
      duplicateObject,
      deleteObject,
      reparentObject,
    }),
    [
      appendEventLog,
      applySelectionChanged,
      applyObjectLockChanged,
      applyParticipantCameraUpdate,
      clearEventLog,
      connectionState,
      clearSessionSnapshot,
      currentUserId,
      runtimeControllerUserId,
      detailText,
      eventLog,
      frameText,
      isLooking,
      participants,
      gridSnapSettings,
      sessionDetailText,
      sessionState,
      sessionStatusText,
      runtimeState,
      canControlRuntime,
      reconnect,
      refreshSessionSnapshot,
      registerActions,
      renameObject,
      sceneTree,
      selectObject,
      setObjectVisibility,
      goToParticipantCamera,
      selectedObject,
      selectedObjectDetails,
      selectedObjectId,
      selections,
      lockedObjects,
      assets,
      listAssets,
      objectSchema,
      getSchema,
      saveScene,
      playSession,
      pauseSession,
      resumeSession,
      stopSession,
      saveStatus,
      setProperty,
      reloadScripts,
      setMeshAsset,
      setLightProperties,
      setMaterialProperties,
      setMaterialTexture,
      reloadStatus,
      scriptErrorToasts,
      addScriptErrorToast,
      dismissScriptErrorToast,
      serverOrigin,
      gizmoMode,
      setMode,
      setGizmoModeAction,
      setGridSnapSettings,
      setSessionDetailText,
      setSessionState,
      setSessionSnapshot,
      setSessionStatusText,
      statusText,
      toggleLook,
      updateTransform,
      createObject,
      duplicateObject,
      deleteObject,
      reparentObject,
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
