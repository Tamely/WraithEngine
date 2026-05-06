"use client"

import { useEffect, useRef, useState, type ElementType } from "react"
import {
  Maximize2,
  Grid3X3,
  Eye,
  Camera,
  ChevronDown,
  RotateCcw,
} from "lucide-react"

const DEFAULT_AXIOM_SERVER_ORIGIN = "http://127.0.0.1:8080"
const STATUS_POLL_INTERVAL_MS = 1500
const ICE_POLL_INTERVAL_MS = 1000
const MAX_LOG_LINES = 10

type ConnectionState =
  | "idle"
  | "connecting"
  | "awaiting-backend"
  | "awaiting-media"
  | "streaming"
  | "control-ready"
  | "unsupported"
  | "error"

type ViewMode = "lit" | "unlit" | "wireframe"
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

function getServerOrigin() {
  const configuredOrigin = process.env.NEXT_PUBLIC_AXIOM_SERVER_ORIGIN?.trim()
  return configuredOrigin && configuredOrigin.length > 0
    ? configuredOrigin
    : DEFAULT_AXIOM_SERVER_ORIGIN
}

function formatLogEntry(value: unknown) {
  return typeof value === "string" ? value : JSON.stringify(value)
}

function nextViewMode(current: ViewMode): ViewMode {
  if (current === "lit") {
    return "unlit"
  }
  if (current === "unlit") {
    return "wireframe"
  }
  return "lit"
}

export function Viewport() {
  const videoRef = useRef<HTMLVideoElement>(null)
  const viewportShellRef = useRef<HTMLDivElement>(null)
  const peerConnectionRef = useRef<RTCPeerConnection | null>(null)
  const reliableChannelRef = useRef<RTCDataChannel | null>(null)
  const unreliableChannelRef = useRef<RTCDataChannel | null>(null)
  const statusPollHandleRef = useRef<number | null>(null)
  const icePollHandleRef = useRef<number | null>(null)
  const inputFrameHandleRef = useRef<number | null>(null)
  const localIceQueueRef = useRef<IceCandidatePayload[]>([])
  const remoteDescriptionAppliedRef = useRef(false)
  const activeGenerationRef = useRef(0)
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
  const [connectionState, setConnectionState] = useState<ConnectionState>("idle")
  const [statusText, setStatusText] = useState("Ready to connect")
  const [detailText, setDetailText] = useState("Waiting for remote viewport stream")
  const [frameText, setFrameText] = useState("No stream metadata yet")
  const [viewMode, setViewMode] = useState<ViewMode>("lit")
  const [isLooking, setIsLooking] = useState(false)
  const [eventLog, setEventLog] = useState<string[]>([])
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
    let disposed = false

    function appendLog(value: unknown) {
      if (disposed) {
        return
      }
      const line = formatLogEntry(value)
      setEventLog((current) => [line, ...current].slice(0, MAX_LOG_LINES))
    }

    function setDisconnectedUi(nextStatus: ConnectionState, title: string, detail: string) {
      if (disposed) {
        return
      }
      setConnectionState(nextStatus)
      setStatusText(title)
      setDetailText(detail)
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
      const response = await fetch(`${serverOrigin}${path}`, init)
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
          return
        }

        setConnectionState("awaiting-media")
        setStatusText("Negotiating")
        setDetailText(status.detail ?? "Offer accepted. Waiting for remote media...")
      } catch (error) {
        const message =
          error instanceof Error ? error.message : "Failed to reach remote viewport server."
        setDisconnectedUi("error", "Status error", message)
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
      }
    }

    function wireDataChannel(channel: RTCDataChannel, label: string) {
      channel.addEventListener("open", () => {
        appendLog({ type: "data_channel_open", label })
        if (label === "editor-events") {
          setConnectionState("control-ready")
          setStatusText("Control ready")
        }
      })
      channel.addEventListener("close", () => {
        appendLog({ type: "data_channel_closed", label })
      })
      channel.addEventListener("error", (event) => {
        appendLog({
          type: "data_channel_error",
          label,
          detail: String(event.type || "error"),
        })
      })
      channel.addEventListener("message", (event) => {
        try {
          appendLog(JSON.parse(event.data))
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

      setConnectionState("connecting")
      setStatusText("Connecting")
      setDetailText("Creating browser WebRTC offer...")
      setFrameText("No stream metadata yet")
      setEventLog([])
      startViewportInputPump()

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
          return
        }
        if (peer.connectionState === "closed") {
          setDisconnectedUi("idle", "Disconnected", "Peer connection closed.")
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

      void pollStatus(nextGeneration)
      void pollIceCandidates(nextGeneration)
    }

    connectRef.current = connect

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
      if (!pointerLockedRef.current && isLookingRef.current) {
        void setLookEnabled(false)
      } else if (pointerLockedRef.current && !isLookingRef.current) {
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
      notifyServerOnDestroyRef.current = true
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
      void destroyPeerConnection("component_unmount")
    }
  }, [serverOrigin])

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

  async function cycleMode() {
    const nextMode = nextViewMode(viewModeRef.current)
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

  async function reconnect() {
    await connectRef.current()
  }

  return (
    <div className="h-full flex flex-col bg-neutral-900">
      <div className="flex items-center justify-between h-8 bg-neutral-950 border-b border-neutral-800 px-2">
        <div className="flex items-center gap-2">
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Perspective
            <ChevronDown className="w-3 h-3" />
          </button>
          <button
            className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded"
            onClick={() => void cycleMode()}
            type="button"
          >
            {viewMode[0].toUpperCase() + viewMode.slice(1)}
            <ChevronDown className="w-3 h-3" />
          </button>
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Show
            <ChevronDown className="w-3 h-3" />
          </button>
        </div>
        <div className="flex items-center gap-1">
          <ViewportButton icon={Grid3X3} />
          <ViewportButton icon={Eye} onClick={() => void cycleMode()} />
          <ViewportButton icon={Camera} onClick={() => void toggleLook()} />
          <ViewportButton icon={Maximize2} onClick={() => void reconnect()} />
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
        </div>
        <div className="absolute right-2 top-2 w-72 rounded-lg border border-neutral-800 bg-black/65 p-3 text-xs text-neutral-300 backdrop-blur-sm">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-[11px] uppercase tracking-[0.18em] text-neutral-500">
                Remote Viewport
              </p>
              <p className="mt-1 text-sm text-white">{statusText}</p>
            </div>
            <div className="flex items-center gap-1">
              <IconButton icon={RotateCcw} label="Reconnect" onClick={() => void reconnect()} />
              <IconButton
                icon={Camera}
                label={isLooking ? "Disable look" : "Enable look"}
                onClick={() => void toggleLook()}
              />
            </div>
          </div>
          <p className="mt-2 text-neutral-400">{detailText}</p>
          <div className="mt-3 grid grid-cols-2 gap-2 text-[11px] text-neutral-400">
            <div className="rounded border border-neutral-800 bg-neutral-950/70 px-2 py-1.5">
              <span className="block text-neutral-500">View Mode</span>
              <button
                className="mt-1 text-left text-neutral-200 hover:text-white"
                onClick={() => void cycleMode()}
                type="button"
              >
                {viewMode}
              </button>
            </div>
            <div className="rounded border border-neutral-800 bg-neutral-950/70 px-2 py-1.5">
              <span className="block text-neutral-500">Controls</span>
              <span className="mt-1 block text-neutral-200">WASD + click to look</span>
            </div>
            <div className="rounded border border-neutral-800 bg-neutral-950/70 px-2 py-1.5">
              <span className="block text-neutral-500">Transport</span>
              <span className="mt-1 block text-neutral-200">{frameText}</span>
            </div>
            <div className="rounded border border-neutral-800 bg-neutral-950/70 px-2 py-1.5">
              <span className="block text-neutral-500">Server</span>
              <span className="mt-1 block truncate text-neutral-200">{serverOrigin}</span>
            </div>
          </div>
          <div className="mt-3 rounded border border-neutral-800 bg-neutral-950/70 p-2">
            <div className="mb-2 flex items-center justify-between">
              <p className="text-[11px] uppercase tracking-[0.16em] text-neutral-500">
                Session Log
              </p>
              <button
                className="text-[11px] text-neutral-500 hover:text-neutral-200"
                onClick={() => setEventLog([])}
                type="button"
              >
                Clear
              </button>
            </div>
            <div className="max-h-28 space-y-1 overflow-auto font-mono text-[10px] text-neutral-400">
              {eventLog.length === 0 ? (
                <p>No events yet</p>
              ) : (
                eventLog.map((line, index) => <p key={`${index}-${line}`}>{line}</p>)
              )}
            </div>
          </div>
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

function IconButton({
  icon: Icon,
  label,
  onClick,
}: {
  icon: ElementType
  label: string
  onClick: () => void
}) {
  return (
    <button
      aria-label={label}
      className="rounded border border-neutral-800 bg-neutral-950/70 p-1.5 text-neutral-300 hover:border-neutral-700 hover:text-white"
      onClick={onClick}
      type="button"
    >
      <Icon className="h-3.5 w-3.5" />
    </button>
  )
}
