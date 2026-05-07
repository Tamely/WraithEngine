"use client"

import { useEffect, useRef, useState, type ElementType } from "react"
import {
  Maximize2,
  Grid3X3,
  Eye,
  Camera,
  ChevronDown,
} from "lucide-react"
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu"
import {
  useRemoteViewport,
  type SessionObjectTransformUpdate,
  type SessionObjectDetails,
  type RemoteViewportConnectionState,
  type RemoteViewportViewMode,
  type SessionParticipant,
  type SessionSceneItem,
  type SessionSelection,
} from "./remote-viewport-context"

const DEFAULT_AXIOM_SERVER_ORIGIN = "http://127.0.0.1:8080"
const CLIENT_ID_STORAGE_KEY = "axiom-remote-client-id"
const CLIENT_ID_CLAIM_CHANNEL = "axiom-remote-client-claims"
const STATUS_POLL_INTERVAL_MS = 1500
const ICE_POLL_INTERVAL_MS = 1000
const SESSION_POLL_INTERVAL_MS = 1500
const CLIENT_ID_CLAIM_TIMEOUT_MS = 100
type ConnectionState = RemoteViewportConnectionState
type ViewMode = RemoteViewportViewMode
type ChannelPreference = "reliable" | "unreliable"

interface WebRtcVideoStatus {
  codec?: string
  lastFrameIndex?: number | null
  pendingPacketCount?: number
  droppedPacketCount?: number
}

interface WebRtcStatusResponse {
  enabled: boolean
  available: boolean
  detail?: string
  video?: WebRtcVideoStatus
}

interface SessionDescriptionPayload {
  type: RTCSdpType
  sdp: string
}

interface IceCandidatePayload {
  candidate: string
  sdpMid: string | null
  sdpMLineIndex: number | null
}

interface IceCandidateListResponse {
  candidates?: RTCIceCandidateInit[]
}

interface SessionSnapshotResponse {
  sessionId: number
  currentUserId: number
  transport?: {
    connected: boolean
    state?: string
    webrtcConnectionState?: string
  }
  participants: SessionParticipant[]
  sceneTree: SessionSceneItem[]
  selections: SessionSelection[]
  selectedObjectDetails: SessionObjectDetails | null
}

interface SessionConnectResponse {
  type: "session_connect"
  clientId: string
  snapshot: SessionSnapshotResponse
}

type RemoteViewportCommand =
  | {
      type: "set_view_mode"
      viewMode: ViewMode
    }
  | {
      type: "set_look_active"
      isLooking: boolean
      cursorPosition: [number, number]
    }
  | {
      type: "update_viewport_camera"
      worldMovement: [number, number, number]
      cursorPosition: [number, number]
    }
  | {
      type: "set_viewport_camera_pose"
      position: [number, number, number]
      yawDegrees: number
      pitchDegrees: number
    }
  | {
      type: "select_object"
      objectId: string
    }
  | {
      type: "rename_object"
      objectId: string
      displayName: string
    }
  | {
      type: "set_object_visibility"
      objectId: string
      visible: boolean
    }
  | ({
      type: "set_transform"
    } & SessionObjectTransformUpdate)
  | {
      type: "create_object"
      templateId: string
    }
  | {
      type: "duplicate_object"
      objectId: string
    }
  | {
      type: "delete_object"
      objectId: string
    }

function getServerOrigin() {
  const configuredOrigin = process.env.NEXT_PUBLIC_AXIOM_SERVER_ORIGIN?.trim()
  return configuredOrigin && configuredOrigin.length > 0
    ? configuredOrigin
    : DEFAULT_AXIOM_SERVER_ORIGIN
}

function formatLogEntry(value: unknown) {
  return typeof value === "string" ? value : JSON.stringify(value)
}

export function Viewport() {
  const videoRef = useRef<HTMLVideoElement>(null)
  const viewportShellRef = useRef<HTMLDivElement>(null)
  const peerConnectionRef = useRef<RTCPeerConnection | null>(null)
  const reliableChannelRef = useRef<RTCDataChannel | null>(null)
  const unreliableChannelRef = useRef<RTCDataChannel | null>(null)
  const statusPollHandleRef = useRef<number | null>(null)
  const icePollHandleRef = useRef<number | null>(null)
  const sessionPollHandleRef = useRef<number | null>(null)
  const inputFrameHandleRef = useRef<number | null>(null)
  const claimChannelRef = useRef<BroadcastChannel | null>(null)
  const localIceQueueRef = useRef<IceCandidatePayload[]>([])
  const remoteDescriptionAppliedRef = useRef(false)
  const activeGenerationRef = useRef(0)
  const clientIdRef = useRef<string | null>(null)
  const tabInstanceIdRef = useRef(
    typeof crypto !== "undefined" && "randomUUID" in crypto
      ? crypto.randomUUID()
      : `tab-${Date.now()}-${Math.random().toString(16).slice(2)}`
  )
  const participantsRef = useRef<SessionParticipant[]>([])
  const sessionReadyRef = useRef(false)
  const reconnectingRef = useRef(false)
  const pointerLockedRef = useRef(false)
  const keysRef = useRef(new Set<string>())
  const cursorRef = useRef({ x: 0, y: 0 })
  const pendingLookDeltaRef = useRef({ x: 0, y: 0 })
  const isLookingRef = useRef(false)
  const viewModeRef = useRef<ViewMode>("lit")
  const notifyServerOnDestroyRef = useRef(true)
  const connectRef = useRef<() => Promise<void>>(async () => {})
  const sendCommandRef = useRef<
    (command: RemoteViewportCommand, preferredChannel?: ChannelPreference) => Promise<boolean>
  >(async () => false)
  const {
    connectionState,
    statusText,
    detailText,
    frameText,
    viewMode,
    isLooking,
    participants,
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
    clearSessionSnapshot,
    applySelectionChanged,
    applyParticipantCameraUpdate,
    setSessionState,
    sessionStatusText,
    setSessionStatusText,
    setSessionDetailText,
  } = useRemoteViewport()
  const [serverOrigin] = useState(getServerOrigin)

  const isConnected =
    connectionState === "streaming" || connectionState === "control-ready"

  useEffect(() => {
    viewModeRef.current = viewMode
  }, [viewMode])

  useEffect(() => {
    isLookingRef.current = isLooking
  }, [isLooking])

  useEffect(() => {
    participantsRef.current = participants
  }, [participants])

  useEffect(() => {
    let disposed = false

    function appendLog(value: unknown) {
      if (disposed) {
        return
      }
      appendEventLog(formatLogEntry(value))
    }

    function setDisconnectedUi(nextStatus: ConnectionState, title: string, detail: string) {
      if (disposed) {
        return
      }
      setConnectionState(nextStatus)
      setStatusText(title)
      setDetailText(detail)
    }

    function setSessionUi(
      nextState:
        | "idle"
        | "connecting"
        | "snapshot-loading"
        | "session-ready"
        | "reconnecting"
        | "degraded-transport"
        | "command-rejected"
        | "error",
      title: string,
      detail: string
    ) {
      if (disposed) {
        return
      }
      setSessionState(nextState)
      setSessionStatusText(title)
      setSessionDetailText(detail)
    }

    function markTransportDegraded(detail: string) {
      if (!sessionReadyRef.current) {
        return
      }
      setSessionUi("degraded-transport", "Transport degraded", detail)
    }

    function clearPolling() {
      if (statusPollHandleRef.current !== null) {
        window.clearInterval(statusPollHandleRef.current)
        statusPollHandleRef.current = null
      }
      if (icePollHandleRef.current !== null) {
        window.clearInterval(icePollHandleRef.current)
        icePollHandleRef.current = null
      }
      if (sessionPollHandleRef.current !== null) {
        window.clearInterval(sessionPollHandleRef.current)
        sessionPollHandleRef.current = null
      }
    }

    function setupClientIdClaimChannel() {
      if (typeof window === "undefined" || typeof BroadcastChannel === "undefined") {
        return
      }

      const channel = new BroadcastChannel(CLIENT_ID_CLAIM_CHANNEL)
      claimChannelRef.current = channel
      channel.onmessage = (event: MessageEvent<unknown>) => {
        const message =
          event.data && typeof event.data === "object"
            ? (event.data as Record<string, unknown>)
            : null
        if (message === null || message.type !== "query-client-id") {
          return
        }

        const queriedClientId =
          typeof message.clientId === "string" ? message.clientId : null
        const sourceInstanceId =
          typeof message.instanceId === "string" ? message.instanceId : null
        if (
          queriedClientId === null ||
          sourceInstanceId === null ||
          sourceInstanceId === tabInstanceIdRef.current ||
          clientIdRef.current !== queriedClientId
        ) {
          return
        }

        channel.postMessage({
          type: "client-id-claimed",
          clientId: queriedClientId,
          instanceId: tabInstanceIdRef.current,
        })
      }
    }

    async function canReuseStoredClientId(clientId: string) {
      const channel = claimChannelRef.current
      if (channel === null) {
        return true
      }

      return await new Promise<boolean>((resolve) => {
        let settled = false
        const handleMessage = (event: MessageEvent<unknown>) => {
          const message =
            event.data && typeof event.data === "object"
              ? (event.data as Record<string, unknown>)
              : null
          if (message === null || message.type !== "client-id-claimed") {
            return
          }

          if (
            message.clientId === clientId &&
            message.instanceId !== tabInstanceIdRef.current
          ) {
            settled = true
            channel.removeEventListener("message", handleMessage)
            resolve(false)
          }
        }

        channel.addEventListener("message", handleMessage)
        channel.postMessage({
          type: "query-client-id",
          clientId,
          instanceId: tabInstanceIdRef.current,
        })

        window.setTimeout(() => {
          if (settled) {
            return
          }
          channel.removeEventListener("message", handleMessage)
          resolve(true)
        }, CLIENT_ID_CLAIM_TIMEOUT_MS)
      })
    }

    function stopViewportInputPump() {
      if (inputFrameHandleRef.current !== null) {
        window.cancelAnimationFrame(inputFrameHandleRef.current)
        inputFrameHandleRef.current = null
      }
    }

    function startViewportInputPump() {
      if (inputFrameHandleRef.current !== null) {
        return
      }

      const pump = () => {
        sendViewportInputFrame()
        inputFrameHandleRef.current = window.requestAnimationFrame(pump)
      }

      inputFrameHandleRef.current = window.requestAnimationFrame(pump)
    }

    async function parseResponse(response: Response) {
      const text = await response.text()
      if (text.length === 0) {
        return null
      }

      try {
        return JSON.parse(text) as unknown
      } catch {
        return { message: text } satisfies Record<string, string>
      }
    }

    async function fetchJson<T>(path: string, init?: RequestInit) {
      const headers = new Headers(init?.headers)
      if (clientIdRef.current) {
        headers.set("X-Axiom-Client-Id", clientIdRef.current)
      }
      const response = await fetch(`${serverOrigin}${path}`, {
        ...init,
        headers,
      })
      const payload = await parseResponse(response)

      if (!response.ok) {
        const errorPayload = payload as { detail?: string; message?: string } | null
        throw new Error(
          errorPayload?.detail ??
            errorPayload?.message ??
            `${response.status} ${response.statusText}`
        )
      }

      return payload as T
    }

    async function postText(path: string, body: string) {
      return fetchJson(path, {
        method: "POST",
        headers: {
          "Content-Type": "text/plain; charset=utf-8",
        },
        body,
      })
    }

    function applySessionSnapshot(snapshot: SessionSnapshotResponse, reason: string) {
      if (disposed) {
        return
      }

      setSessionSnapshot({
        currentUserId: snapshot.currentUserId,
        participants: snapshot.participants ?? [],
        sceneTree: snapshot.sceneTree ?? [],
        selections: snapshot.selections ?? [],
        selectedObjectDetails: snapshot.selectedObjectDetails ?? null,
      })
      sessionReadyRef.current = true
      setSessionUi(
        "session-ready",
        "Session ready",
        `Authoritative state synced from session ${snapshot.sessionId}.`
      )
      appendLog({
        type: "session_snapshot_received",
        reason,
        sessionId: snapshot.sessionId,
        currentUserId: snapshot.currentUserId,
        sceneItemCount: snapshot.sceneTree?.length ?? 0,
        participantCount: snapshot.participants?.length ?? 0,
        selectionCount: snapshot.selections?.length ?? 0,
        transportState: snapshot.transport?.state ?? "unknown",
        webrtcConnectionState: snapshot.transport?.webrtcConnectionState ?? "unknown",
      })
    }

    async function bootstrapClientSession() {
      const storedClientId =
        typeof window !== "undefined"
          ? window.sessionStorage.getItem(CLIENT_ID_STORAGE_KEY)
          : null
      const reusableClientId =
        storedClientId !== null && (await canReuseStoredClientId(storedClientId))
          ? storedClientId
          : null
      clientIdRef.current = reusableClientId

      const bootstrap = await fetchJson<SessionConnectResponse>("/session/connect", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: "{}",
      })
      clientIdRef.current = bootstrap.clientId
      if (typeof window !== "undefined") {
        window.sessionStorage.setItem(CLIENT_ID_STORAGE_KEY, bootstrap.clientId)
      }

      applySessionSnapshot(bootstrap.snapshot, "bootstrap")
      appendLog({
        type: "session_client_connected",
        clientId: bootstrap.clientId,
        currentUserId: bootstrap.snapshot.currentUserId,
      })
      if (storedClientId !== null && reusableClientId === null) {
        appendLog({
          type: "session_client_forked",
          previousClientId: storedClientId,
          clientId: bootstrap.clientId,
        })
      }
    }

    async function refreshSessionSnapshot(reason: "connect" | "reconnect" | "resync" | "command" | "event" | "manual" = "manual") {
      const snapshot = await fetchJson<SessionSnapshotResponse>("/session", {
        cache: "no-store",
      })
      applySessionSnapshot(snapshot, reason)
      if (reconnectingRef.current || reason === "reconnect") {
        reconnectingRef.current = false
        appendLog({
          type: "reconnect_resync_completed",
          sessionId: snapshot.sessionId,
        })
      }
    }

    async function refreshSessionSnapshotSafely(
      reason: "connect" | "reconnect" | "resync" | "command" | "event" | "manual" = "manual"
    ) {
      try {
        await refreshSessionSnapshot(reason)
      } catch (error) {
        const detail = error instanceof Error ? error.message : String(error)
        appendLog({
          type: "session_snapshot_failed",
          reason,
          detail,
        })
        setSessionUi("error", "Session sync failed", detail)
      }
    }

    function handleEditorEventMessage(message: Record<string, unknown>) {
      if (message.payloadType === "command_acked") {
        const commandType =
          typeof message.commandType === "string" ? message.commandType : "unknown"
        setSessionUi("session-ready", "Session ready", `Command acknowledged: ${commandType}`)
        return
      }

      if (message.payloadType === "selection_changed") {
        const userId =
          typeof message.user === "number" ? message.user : Number(message.user)
        const objectId = typeof message.objectId === "string" ? message.objectId : null
        if (Number.isFinite(userId)) {
          applySelectionChanged(userId, objectId)
          setSessionUi(
            "session-ready",
            "Session ready",
            objectId ? `Selection changed to ${objectId}.` : "Selection cleared."
          )
          void refreshSessionSnapshotSafely("event")
        }
        return
      }

      if (message.payloadType === "viewport_camera_updated") {
        const userId =
          typeof message.user === "number" ? message.user : Number(message.user)
        const position = Array.isArray(message.position) ? message.position : null
        const yawDegrees =
          typeof message.yawDegrees === "number"
            ? message.yawDegrees
            : Number(message.yawDegrees)
        const pitchDegrees =
          typeof message.pitchDegrees === "number"
            ? message.pitchDegrees
            : Number(message.pitchDegrees)
        if (
          Number.isFinite(userId) &&
          position !== null &&
          position.length === 3 &&
          position.every((component) => typeof component === "number") &&
          Number.isFinite(yawDegrees) &&
          Number.isFinite(pitchDegrees)
        ) {
          applyParticipantCameraUpdate(userId, {
            position: [position[0], position[1], position[2]],
            yawDegrees,
            pitchDegrees,
          })
          setSessionUi("session-ready", "Session ready", "Camera presence updated.")
        }
        return
      }

      if (message.payloadType === "presence_changed") {
        setSessionUi("session-ready", "Session ready", "Participant state changed.")
        void refreshSessionSnapshotSafely("event")
        return
      }

      if (message.payloadType === "object_transform_updated") {
        setSessionUi("session-ready", "Session ready", "Transform update applied.")
        void refreshSessionSnapshotSafely("event")
        return
      }

      if (message.payloadType === "object_renamed") {
        setSessionUi("session-ready", "Session ready", "Object rename applied.")
        void refreshSessionSnapshotSafely("event")
        return
      }

      if (message.payloadType === "object_visibility_changed") {
        setSessionUi("session-ready", "Session ready", "Visibility update applied.")
        void refreshSessionSnapshotSafely("event")
        return
      }

      if (message.payloadType === "object_created") {
        setSessionUi("session-ready", "Session ready", "Object created.")
        void refreshSessionSnapshotSafely("event")
        return
      }

      if (message.payloadType === "object_deleted") {
        setSessionUi("session-ready", "Session ready", "Object deleted.")
        void refreshSessionSnapshotSafely("event")
        return
      }

      if (message.payloadType === "command_rejected") {
        const reason =
          typeof message.reason === "string"
            ? message.reason
            : "The remote session rejected the command."
        setDetailText(reason)
        setSessionUi("command-rejected", "Command rejected", reason)
        void refreshSessionSnapshotSafely("event")
      }
    }

    function updateFrameText(status: WebRtcStatusResponse) {
      if (!status.video) {
        setFrameText("No WebRTC status yet")
        return
      }

      const parts: string[] = []
      if (status.video.codec) {
        parts.push(`Codec ${status.video.codec}`)
      }
      if (status.video.lastFrameIndex !== null && status.video.lastFrameIndex !== undefined) {
        parts.push(`Frame ${status.video.lastFrameIndex}`)
      }
      if (status.video.pendingPacketCount !== undefined) {
        parts.push(`Queued ${status.video.pendingPacketCount}`)
      }
      if (status.video.droppedPacketCount !== undefined && status.video.droppedPacketCount > 0) {
        parts.push(`Dropped ${status.video.droppedPacketCount}`)
      }
      setFrameText(parts.length > 0 ? parts.join(" | ") : "Waiting for video metadata")
    }

    function updateViewportMetrics() {
      const video = videoRef.current
      const shell = viewportShellRef.current
      if (!video || !shell || !video.videoWidth || !video.videoHeight) {
        return
      }

      shell.style.aspectRatio = `${video.videoWidth} / ${video.videoHeight}`
    }

    function channelOpen(channel: RTCDataChannel | null) {
      return channel?.readyState === "open"
    }

    function canSendReliably() {
      return channelOpen(reliableChannelRef.current)
    }

    function canSendUnreliably() {
      return channelOpen(unreliableChannelRef.current)
    }

    async function flushLocalIceCandidates(expectedGeneration: number) {
      const peer = peerConnectionRef.current
      if (!peer || expectedGeneration !== activeGenerationRef.current) {
        return
      }
      if (!remoteDescriptionAppliedRef.current || localIceQueueRef.current.length === 0) {
        return
      }

      const pending = localIceQueueRef.current.splice(0, localIceQueueRef.current.length)
      for (const candidate of pending) {
        try {
          await fetchJson("/webrtc/ice-candidate", {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
            },
            body: JSON.stringify(candidate),
          })
          appendLog({ type: "local_ice_uploaded", candidate: candidate.candidate })
        } catch (error) {
          localIceQueueRef.current.unshift(candidate)
          throw error
        }
      }
    }

    async function sendCommand(
      command: RemoteViewportCommand,
      preferredChannel: ChannelPreference = "reliable"
    ) {
      const serialized = JSON.stringify(command)
      if (preferredChannel === "unreliable" && canSendUnreliably()) {
        unreliableChannelRef.current?.send(serialized)
        return true
      }
      if (canSendReliably()) {
        reliableChannelRef.current?.send(serialized)
        return true
      }

      try {
        await fetchJson("/command", {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
          },
          body: serialized,
        })
        return true
      } catch (error) {
        appendLog({
          type: "command_failed",
          command: command.type,
          detail: error instanceof Error ? error.message : String(error),
        })
        return false
      }
    }

    sendCommandRef.current = sendCommand

    function movementVector(): [number, number, number] {
      const keys = keysRef.current
      const forward = (keys.has("KeyW") ? 1 : 0) - (keys.has("KeyS") ? 1 : 0)
      const strafe = (keys.has("KeyD") ? 1 : 0) - (keys.has("KeyA") ? 1 : 0)
      const lift =
        (keys.has("Space") ? 1 : 0) -
        ((keys.has("ShiftLeft") || keys.has("ShiftRight")) ? 1 : 0)
      return [strafe * 0.08, lift * 0.08, forward * 0.08]
    }

    function applyPendingLook() {
      if (!isLookingRef.current) {
        pendingLookDeltaRef.current.x = 0
        pendingLookDeltaRef.current.y = 0
        return false
      }
      if (
        pendingLookDeltaRef.current.x === 0 &&
        pendingLookDeltaRef.current.y === 0
      ) {
        return false
      }

      cursorRef.current.x += pendingLookDeltaRef.current.x
      cursorRef.current.y += pendingLookDeltaRef.current.y
      pendingLookDeltaRef.current.x = 0
      pendingLookDeltaRef.current.y = 0
      return true
    }

    function sendViewportInputFrame() {
      const lookChanged = applyPendingLook()
      const [x, y, z] = movementVector()
      if (!lookChanged && x === 0 && y === 0 && z === 0) {
        return false
      }

      void sendCommand(
        {
          type: "update_viewport_camera",
          worldMovement: [x, y, z],
          cursorPosition: [cursorRef.current.x, cursorRef.current.y],
        },
        "unreliable"
      )
      return true
    }

    async function setLookEnabled(nextValue: boolean) {
      if (isLookingRef.current === nextValue) {
        return
      }

      isLookingRef.current = nextValue
      setIsLooking(nextValue)
      await sendCommand(
        {
          type: "set_look_active",
          isLooking: nextValue,
          cursorPosition: [cursorRef.current.x, cursorRef.current.y],
        },
        "reliable"
      )
    }

    async function pollStatus(expectedGeneration: number) {
      if (expectedGeneration !== activeGenerationRef.current) {
        return
      }

      try {
        const status = await fetchJson<WebRtcStatusResponse>("/webrtc", {
          cache: "no-store",
        })
        if (disposed || expectedGeneration !== activeGenerationRef.current) {
          return
        }

        updateFrameText(status)
        if (!status.enabled) {
          setDisconnectedUi(
            "error",
            "WebRTC disabled",
            status.detail ?? "WebRTC support is disabled in this build."
          )
          return
        }
        if (!status.available) {
          setDisconnectedUi(
            "awaiting-backend",
            "Awaiting backend",
            status.detail ?? "Waiting for native WebRTC backend."
          )
          markTransportDegraded(status.detail ?? "Waiting for native WebRTC backend.")
          return
        }

        const peer = peerConnectionRef.current
        if (peer?.connectionState === "connected") {
          if (canSendReliably()) {
            setConnectionState("control-ready")
            setStatusText("Control ready")
          } else {
            setConnectionState("streaming")
            setStatusText("Streaming")
          }
          setDetailText(status.detail ?? "Receiving live WebRTC viewport stream")
          if (sessionReadyRef.current) {
            setSessionUi(
              "session-ready",
              "Session ready",
              "Viewport transport and authoritative session are both healthy."
            )
          }
          return
        }

        setConnectionState("awaiting-media")
        setStatusText("Negotiating")
        setDetailText(status.detail ?? "Offer accepted. Waiting for remote media...")
        markTransportDegraded(status.detail ?? "Offer accepted. Waiting for remote media...")
      } catch (error) {
        const message =
          error instanceof Error ? error.message : "Failed to reach remote viewport server."
        setDisconnectedUi("error", "Status error", message)
        markTransportDegraded(message)
      }
    }

    async function pollIceCandidates(expectedGeneration: number) {
      const peer = peerConnectionRef.current
      if (!peer || expectedGeneration !== activeGenerationRef.current) {
        return
      }

      try {
        const payload = await fetchJson<IceCandidateListResponse>("/webrtc/ice-candidates", {
          cache: "no-store",
        })
        if (!payload.candidates || expectedGeneration !== activeGenerationRef.current) {
          return
        }

        for (const candidate of payload.candidates) {
          await peer.addIceCandidate(candidate)
          appendLog({ type: "remote_ice_candidate", candidate })
        }
      } catch (error) {
        const message =
          error instanceof Error ? error.message : "Failed to poll remote ICE candidates."
        setDisconnectedUi("error", "ICE error", message)
        markTransportDegraded(message)
      }
    }

    function wireDataChannel(channel: RTCDataChannel, label: string) {
      channel.addEventListener("open", () => {
        appendLog({ type: "data_channel_open", label })
        void refreshSessionSnapshotSafely("resync")
        if (label === "editor-events") {
          setConnectionState("control-ready")
          setStatusText("Control ready")
          if (sessionReadyRef.current) {
            setSessionUi(
              "session-ready",
              "Session ready",
              "Reliable editor channel is open."
            )
          }
        }
      })
      channel.addEventListener("close", () => {
        appendLog({ type: "data_channel_closed", label })
        markTransportDegraded(`${label} channel closed.`)
      })
      channel.addEventListener("error", (event) => {
        appendLog({
          type: "data_channel_error",
          label,
          detail: String(event.type || "error"),
        })
        markTransportDegraded(`${label} channel reported an error.`)
      })
      channel.addEventListener("message", (event) => {
        try {
          const parsed = JSON.parse(event.data) as Record<string, unknown>
          appendLog(parsed)
          handleEditorEventMessage(parsed)
        } catch {
          appendLog({ type: "data_channel_message", label, body: event.data })
        }
      })
    }

    async function notifyServerSessionClosed(reason: string) {
      try {
        const response = await postText("/webrtc/close", reason)
        appendLog({ type: "webrtc_close_ack", reason, response })
      } catch (error) {
        appendLog({
          type: "server_close_notification_failed",
          detail: error instanceof Error ? error.message : String(error),
        })
      }
    }

    async function destroyPeerConnection(reason = "client_reset") {
      clearPolling()
      stopViewportInputPump()

      const reliableChannel = reliableChannelRef.current
      reliableChannelRef.current = null
      if (reliableChannel) {
        reliableChannel.close()
      }

      const unreliableChannel = unreliableChannelRef.current
      unreliableChannelRef.current = null
      if (unreliableChannel) {
        unreliableChannel.close()
      }

      const peer = peerConnectionRef.current
      peerConnectionRef.current = null
      remoteDescriptionAppliedRef.current = false
      localIceQueueRef.current = []

      if (peer) {
        peer.close()
      }

      if (videoRef.current) {
        videoRef.current.srcObject = null
      }

      if (notifyServerOnDestroyRef.current) {
        await notifyServerSessionClosed(reason)
      }
    }

    async function connect() {
      const nextGeneration = activeGenerationRef.current + 1
      activeGenerationRef.current = nextGeneration

      reconnectingRef.current = sessionReadyRef.current
      await destroyPeerConnection("reconnect")
      if (disposed) {
        return
      }

      if (typeof window === "undefined" || typeof window.RTCPeerConnection === "undefined") {
        setDisconnectedUi(
          "unsupported",
          "Unsupported browser",
          "This browser does not support RTCPeerConnection."
        )
        return
      }

      sessionReadyRef.current = false
      clearSessionSnapshot()
      if (reconnectingRef.current) {
        setSessionUi("reconnecting", "Reconnecting", "Rehydrating authoritative session state...")
        appendLog({ type: "reconnect_started" })
      } else {
        setSessionUi("connecting", "Connecting", "Preparing authoritative session bootstrap...")
      }
      setConnectionState("connecting")
      setStatusText("Connecting")
      setDetailText("Creating browser WebRTC offer...")
      setFrameText("No stream metadata yet")
      clearEventLog()
      startViewportInputPump()
      setSessionUi("snapshot-loading", "Snapshot loading", "Connecting to authoritative session...")
      try {
        await bootstrapClientSession()
      } catch (error) {
        const message =
          error instanceof Error ? error.message : "Failed to connect to authoritative session."
        setDisconnectedUi("error", "Session bootstrap failed", message)
        setSessionUi("error", "Session bootstrap failed", message)
        return
      }
      if (reconnectingRef.current) {
        appendLog({ type: "session_client_resumed", clientId: clientIdRef.current })
      }

      const peer = new window.RTCPeerConnection({
        bundlePolicy: "max-bundle",
        rtcpMuxPolicy: "require",
      })
      peerConnectionRef.current = peer

      peer.addEventListener("connectionstatechange", () => {
        if (disposed || nextGeneration !== activeGenerationRef.current) {
          return
        }
        appendLog({ type: "connection_state", state: peer.connectionState })
        if (peer.connectionState === "connected") {
          void refreshSessionSnapshotSafely("resync")
          if (canSendReliably()) {
            setConnectionState("control-ready")
            setStatusText("Control ready")
          } else {
            setConnectionState("streaming")
            setStatusText("Streaming")
          }
          setDetailText("Receiving live WebRTC viewport stream")
          return
        }
        if (peer.connectionState === "failed" || peer.connectionState === "disconnected") {
          setDisconnectedUi("error", "Peer failed", `Peer connection ${peer.connectionState}.`)
          markTransportDegraded(`Peer connection ${peer.connectionState}.`)
          return
        }
        if (peer.connectionState === "closed") {
          setDisconnectedUi("idle", "Disconnected", "Peer connection closed.")
          markTransportDegraded("Peer connection closed.")
        }
      })

      peer.addEventListener("signalingstatechange", () => {
        if (disposed || nextGeneration !== activeGenerationRef.current) {
          return
        }
        appendLog({ type: "signaling_state", state: peer.signalingState })
      })

      peer.addEventListener("iceconnectionstatechange", () => {
        if (disposed || nextGeneration !== activeGenerationRef.current) {
          return
        }
        appendLog({ type: "ice_connection_state", state: peer.iceConnectionState })
        if (peer.iceConnectionState === "failed") {
          setDetailText("ICE failed. Reconnect after the server is ready.")
          markTransportDegraded("ICE failed. Reconnect after the server is ready.")
        }
      })

      peer.addEventListener("track", (event) => {
        if (disposed || nextGeneration !== activeGenerationRef.current) {
          return
        }

        appendLog({ type: "remote_track", kind: event.track.kind })
        const [stream] = event.streams
        if (stream && videoRef.current) {
          videoRef.current.srcObject = stream
          updateViewportMetrics()
          if (canSendReliably()) {
            setConnectionState("control-ready")
            setStatusText("Control ready")
          } else {
            setConnectionState("streaming")
            setStatusText("Streaming")
          }
          setDetailText("Receiving live WebRTC viewport stream")
          if (sessionReadyRef.current) {
            setSessionUi(
              "session-ready",
              "Session ready",
              "Viewport media resumed after reconnect."
            )
          }
        }
      })

      peer.addTransceiver("video", {
        direction: "recvonly",
      })
      appendLog({ type: "transceiver_added", kind: "video", direction: "recvonly" })

      peer.addEventListener("icecandidate", async (event) => {
        if (disposed || nextGeneration !== activeGenerationRef.current) {
          return
        }
        if (!event.candidate) {
          appendLog({ type: "ice_gathering_complete" })
          return
        }
        if (!event.candidate.candidate) {
          return
        }

        appendLog({ type: "local_ice_candidate", candidate: event.candidate.candidate })
        localIceQueueRef.current.push({
          candidate: event.candidate.candidate,
          sdpMid: event.candidate.sdpMid,
          sdpMLineIndex: event.candidate.sdpMLineIndex,
        })

        if (remoteDescriptionAppliedRef.current) {
          try {
            await flushLocalIceCandidates(nextGeneration)
          } catch (error) {
            const message =
              error instanceof Error ? error.message : "Failed to upload local ICE candidate."
            setDisconnectedUi("error", "ICE error", message)
          }
        }
      })

      reliableChannelRef.current = peer.createDataChannel("editor-events", {
        ordered: true,
      })
      wireDataChannel(reliableChannelRef.current, "editor-events")

      unreliableChannelRef.current = peer.createDataChannel("viewport-input", {
        ordered: false,
        maxRetransmits: 0,
      })
      wireDataChannel(unreliableChannelRef.current, "viewport-input")

      try {
        appendLog({ type: "creating_offer" })
        const offer = await peer.createOffer()
        appendLog({
          type: "offer_created",
          typeName: offer.type,
          sdpLength: offer.sdp ? offer.sdp.length : 0,
        })
        await peer.setLocalDescription(offer)
        appendLog({
          type: "local_description_set",
          signalingState: peer.signalingState,
        })

        const answer = await fetchJson<SessionDescriptionPayload>("/webrtc/offer", {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
          },
          body: JSON.stringify({
            type: offer.type,
            sdp: offer.sdp,
          }),
        })

        if (disposed || nextGeneration !== activeGenerationRef.current) {
          return
        }
        appendLog({ type: "offer_response", body: answer })
        if (!answer.sdp || answer.type !== "answer") {
          throw new Error("Server did not return a valid WebRTC answer.")
        }

        await peer.setRemoteDescription(answer)
        remoteDescriptionAppliedRef.current = true
        appendLog({
          type: "remote_description_set",
          signalingState: peer.signalingState,
        })
        await flushLocalIceCandidates(nextGeneration)
        await refreshSessionSnapshotSafely(reconnectingRef.current ? "reconnect" : "resync")

        setConnectionState("awaiting-media")
        setStatusText("Awaiting media")
        setDetailText("Offer accepted. Waiting for remote media...")
      } catch (error) {
        const message =
          error instanceof Error ? error.message : "WebRTC negotiation failed."
        setDisconnectedUi("error", "Negotiation failed", message)
        return
      }

      statusPollHandleRef.current = window.setInterval(() => {
        void pollStatus(nextGeneration)
      }, STATUS_POLL_INTERVAL_MS)
      icePollHandleRef.current = window.setInterval(() => {
        void pollIceCandidates(nextGeneration)
      }, ICE_POLL_INTERVAL_MS)
      sessionPollHandleRef.current = window.setInterval(() => {
        if (sessionReadyRef.current) {
          void refreshSessionSnapshotSafely("resync")
        }
      }, SESSION_POLL_INTERVAL_MS)

      void pollStatus(nextGeneration)
      void pollIceCandidates(nextGeneration)
      void refreshSessionSnapshotSafely("resync")
    }

    connectRef.current = connect
    registerActions({
      reconnect: connect,
      toggleLook: async () => {
        const nextValue = !isLookingRef.current
        isLookingRef.current = nextValue
        setIsLooking(nextValue)
        await sendCommand(
          {
            type: "set_look_active",
            isLooking: nextValue,
            cursorPosition: [cursorRef.current.x, cursorRef.current.y],
          },
          "reliable"
        )
      },
      refreshSessionSnapshot,
      selectObject: async (objectId) => {
        const accepted = await sendCommand(
          {
            type: "select_object",
            objectId,
          },
          "reliable"
        )
        if (accepted) {
          await refreshSessionSnapshotSafely("command")
        }
        return accepted
      },
      renameObject: async (objectId, displayName) => {
        const accepted = await sendCommand(
          {
            type: "rename_object",
            objectId,
            displayName,
          },
          "reliable"
        )
        if (accepted) {
          await refreshSessionSnapshotSafely("command")
        }
        return accepted
      },
      setObjectVisibility: async (objectId, visible) => {
        const accepted = await sendCommand(
          {
            type: "set_object_visibility",
            objectId,
            visible,
          },
          "reliable"
        )
        if (accepted) {
          await refreshSessionSnapshotSafely("command")
        }
        return accepted
      },
      goToParticipantCamera: async (userId) => {
        const participant = participantsRef.current.find((entry) => entry.userId === userId)
        if (!participant?.camera) {
          return false
        }
        const accepted = await sendCommand(
          {
            type: "set_viewport_camera_pose",
            position: participant.camera.position,
            yawDegrees: participant.camera.yawDegrees,
            pitchDegrees: participant.camera.pitchDegrees,
          },
          "reliable"
        )
        if (accepted) {
          await refreshSessionSnapshotSafely("command")
        }
        return accepted
      },
      updateTransform: async (details) => {
        const accepted = await sendCommand(
          {
            type: "set_transform",
            ...details,
          },
          "reliable"
        )
        if (accepted) {
          await refreshSessionSnapshotSafely("command")
        }
        return accepted
      },
      createObject: async (templateId) => {
        const accepted = await sendCommand(
          {
            type: "create_object",
            templateId,
          },
          "reliable"
        )
        if (accepted) {
          await refreshSessionSnapshotSafely("command")
        }
        return accepted
      },
      duplicateObject: async (objectId) => {
        const accepted = await sendCommand(
          {
            type: "duplicate_object",
            objectId,
          },
          "reliable"
        )
        if (accepted) {
          await refreshSessionSnapshotSafely("command")
        }
        return accepted
      },
      deleteObject: async (objectId) => {
        const accepted = await sendCommand(
          {
            type: "delete_object",
            objectId,
          },
          "reliable"
        )
        if (accepted) {
          await refreshSessionSnapshotSafely("command")
        }
        return accepted
      },
      setMode: async (nextMode) => {
        if (viewModeRef.current === nextMode) {
          return
        }
        viewModeRef.current = nextMode
        setViewMode(nextMode)
        await sendCommand(
          {
            type: "set_view_mode",
            viewMode: nextMode,
          },
          "reliable"
        )
      },
    })
    setServerOrigin(serverOrigin)
    setupClientIdClaimChannel()

    const video = videoRef.current
    const shell = viewportShellRef.current

    const handleLoadedMetadata = () => {
      updateViewportMetrics()
      if (videoRef.current) {
        appendLog({
          type: "video_loadedmetadata",
          width: videoRef.current.videoWidth,
          height: videoRef.current.videoHeight,
        })
      }
    }

    const handleResize = () => {
      updateViewportMetrics()
      if (videoRef.current) {
        appendLog({
          type: "video_resize",
          width: videoRef.current.videoWidth,
          height: videoRef.current.videoHeight,
        })
      }
    }

    const handleShellClick = async () => {
      if (!shell || pointerLockedRef.current) {
        return
      }
      try {
        await shell.requestPointerLock()
      } catch (error) {
        appendLog(`pointer lock failed: ${String(error)}`)
      }
    }

    const handlePointerLockChange = () => {
      pointerLockedRef.current = document.pointerLockElement === shell
      if (!pointerLockedRef.current) {
        keysRef.current.clear()
        if (isLookingRef.current) void setLookEnabled(false)
      } else if (!isLookingRef.current) {
        void setLookEnabled(true)
      }
    }

    const handleMouseMove = (event: MouseEvent) => {
      if (!pointerLockedRef.current) {
        return
      }
      pendingLookDeltaRef.current.x += event.movementX
      pendingLookDeltaRef.current.y += event.movementY
      if (isLookingRef.current) {
        sendViewportInputFrame()
      }
    }

    const handleKeyDown = (event: KeyboardEvent) => {
      if (!pointerLockedRef.current) return
      if (
        event.code === "Space" ||
        event.code === "ShiftLeft" ||
        event.code === "ShiftRight" ||
        event.code === "KeyW" ||
        event.code === "KeyA" ||
        event.code === "KeyS" ||
        event.code === "KeyD"
      ) {
        event.preventDefault()
      }
      keysRef.current.add(event.code)
    }

    const handleKeyUp = (event: KeyboardEvent) => {
      if (!pointerLockedRef.current) return
      if (
        event.code === "Space" ||
        event.code === "ShiftLeft" ||
        event.code === "ShiftRight" ||
        event.code === "KeyW" ||
        event.code === "KeyA" ||
        event.code === "KeyS" ||
        event.code === "KeyD"
      ) {
        event.preventDefault()
      }
      keysRef.current.delete(event.code)
    }

    const handleBeforeUnload = () => {
      // Let a same-tab refresh reclaim the single server peer without an unload
      // race closing the freshly negotiated replacement session underneath it.
      notifyServerOnDestroyRef.current = false
      void destroyPeerConnection("page_unload")
    }

    video?.addEventListener("loadedmetadata", handleLoadedMetadata)
    video?.addEventListener("resize", handleResize)
    shell?.addEventListener("click", handleShellClick)
    document.addEventListener("pointerlockchange", handlePointerLockChange)
    document.addEventListener("mousemove", handleMouseMove)
    document.addEventListener("keydown", handleKeyDown)
    document.addEventListener("keyup", handleKeyUp)
    window.addEventListener("beforeunload", handleBeforeUnload)

    void connect()

    return () => {
      disposed = true
      notifyServerOnDestroyRef.current = false
      stopViewportInputPump()
      video?.removeEventListener("loadedmetadata", handleLoadedMetadata)
      video?.removeEventListener("resize", handleResize)
      shell?.removeEventListener("click", handleShellClick)
      document.removeEventListener("pointerlockchange", handlePointerLockChange)
      document.removeEventListener("mousemove", handleMouseMove)
      document.removeEventListener("keydown", handleKeyDown)
      document.removeEventListener("keyup", handleKeyUp)
      window.removeEventListener("beforeunload", handleBeforeUnload)
      claimChannelRef.current?.close()
      claimChannelRef.current = null
      void destroyPeerConnection("component_unmount")
    }
  }, [appendEventLog, clearEventLog, registerActions, serverOrigin, setConnectionState, setDetailText, setFrameText, setIsLooking, setServerOrigin, setStatusText, setViewMode])

  async function toggleLook() {
    const nextValue = !isLookingRef.current
    isLookingRef.current = nextValue
    setIsLooking(nextValue)
    await sendCommandRef.current(
      {
        type: "set_look_active",
        isLooking: nextValue,
        cursorPosition: [cursorRef.current.x, cursorRef.current.y],
      },
      "reliable"
    )
  }

  async function setMode(nextMode: ViewMode) {
    if (viewModeRef.current === nextMode) {
      return
    }

    viewModeRef.current = nextMode
    setViewMode(nextMode)
    await sendCommandRef.current(
      {
        type: "set_view_mode",
        viewMode: nextMode,
      },
      "reliable"
    )
  }

  return (
    <div className="h-full flex flex-col bg-neutral-900">
      <div className="flex items-center justify-between h-8 bg-neutral-950 border-b border-neutral-800 px-2">
        <div className="flex items-center gap-2">
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Perspective
            <ChevronDown className="w-3 h-3" />
          </button>
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <button
                className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded"
                type="button"
              >
                {viewMode[0].toUpperCase() + viewMode.slice(1)}
                <ChevronDown className="w-3 h-3" />
              </button>
            </DropdownMenuTrigger>
            <DropdownMenuContent
              align="start"
              className="border-neutral-800 bg-neutral-950 text-neutral-200"
            >
              <DropdownMenuRadioGroup
                value={viewMode}
                onValueChange={(value) => void setMode(value as ViewMode)}
              >
                <DropdownMenuRadioItem value="lit">Lit</DropdownMenuRadioItem>
                <DropdownMenuRadioItem value="unlit">Unlit</DropdownMenuRadioItem>
                <DropdownMenuRadioItem value="wireframe">Wireframe</DropdownMenuRadioItem>
              </DropdownMenuRadioGroup>
            </DropdownMenuContent>
          </DropdownMenu>
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Show
            <ChevronDown className="w-3 h-3" />
          </button>
        </div>
        <div className="flex items-center gap-1">
          <ViewportButton icon={Grid3X3} />
          <ViewportButton icon={Eye} />
          <ViewportButton icon={Camera} onClick={() => void toggleLook()} />
          <ViewportButton icon={Maximize2} onClick={() => void connectRef.current()} />
        </div>
      </div>
      <div ref={viewportShellRef} className="relative flex-1 overflow-hidden bg-black">
        <video
          ref={videoRef}
          className="absolute inset-0 h-full w-full bg-black object-contain"
          autoPlay
          muted
          playsInline
        />
        {!isConnected && (
          <div className="absolute inset-0 flex items-center justify-center bg-neutral-900/95">
            <div className="text-center">
              <div className="mx-auto mb-4 flex h-16 w-16 items-center justify-center rounded-lg border-2 border-neutral-700">
                <Camera className="h-8 w-8 text-neutral-600" />
              </div>
              <p className="text-sm text-neutral-200">{statusText}</p>
              <p className="mt-1 text-xs text-neutral-500">{detailText}</p>
              <p className="mt-3 text-[11px] text-neutral-600">Server: {serverOrigin}</p>
            </div>
          </div>
        )}
        <div className="absolute bottom-2 left-2 flex items-center gap-2 text-xs text-neutral-500">
          <span>{frameText}</span>
          <span>|</span>
          <span>{statusText}</span>
          <span>|</span>
          <span>{sessionStatusText}</span>
        </div>
      </div>
    </div>
  )
}

function ViewportButton({
  icon: Icon,
  onClick,
}: {
  icon: ElementType
  onClick?: () => void
}) {
  return (
    <button
      className="rounded p-1.5 text-neutral-400 transition-colors hover:bg-neutral-800 hover:text-white"
      onClick={onClick}
      type="button"
    >
      <Icon className="h-3.5 w-3.5" />
    </button>
  )
}
