"use client"

import { useEffect, useRef, useState, type ElementType } from "react"
import {
  Maximize2,
  Grid3X3,
  Eye,
  Camera,
  ChevronDown,
} from "lucide-react"

const DEFAULT_AXIOM_SERVER_ORIGIN = "http://127.0.0.1:8080"
const STATUS_POLL_INTERVAL_MS = 1500
const ICE_POLL_INTERVAL_MS = 1000

type ConnectionState =
  | "idle"
  | "connecting"
  | "awaiting-backend"
  | "awaiting-media"
  | "streaming"
  | "unsupported"
  | "error"

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
  signalingState?: string
  connectionState?: string
  sessionId?: string
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

function getServerOrigin() {
  const configuredOrigin = process.env.NEXT_PUBLIC_AXIOM_SERVER_ORIGIN?.trim()
  return configuredOrigin && configuredOrigin.length > 0
    ? configuredOrigin
    : DEFAULT_AXIOM_SERVER_ORIGIN
}

export function Viewport() {
  const videoRef = useRef<HTMLVideoElement>(null)
  const peerConnectionRef = useRef<RTCPeerConnection | null>(null)
  const statusPollHandleRef = useRef<number | null>(null)
  const icePollHandleRef = useRef<number | null>(null)
  const localIceQueueRef = useRef<IceCandidatePayload[]>([])
  const remoteDescriptionAppliedRef = useRef(false)
  const activeGenerationRef = useRef(0)
  const [connectionState, setConnectionState] = useState<ConnectionState>("idle")
  const [statusText, setStatusText] = useState("Ready to connect")
  const [detailText, setDetailText] = useState("Waiting for remote viewport stream")
  const [frameText, setFrameText] = useState("No stream metadata yet")
  const [serverOrigin] = useState(getServerOrigin)

  const isConnected = connectionState === "streaming"

  useEffect(() => {
    let disposed = false

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

    function setDisconnectedUi(nextStatus: ConnectionState, title: string, detail: string) {
      if (disposed) {
        return
      }
      setConnectionState(nextStatus)
      setStatusText(title)
      setDetailText(detail)
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
        } catch (error) {
          localIceQueueRef.current.unshift(candidate)
          throw error
        }
      }
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
          setConnectionState("streaming")
          setStatusText("Streaming")
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
        }
      } catch (error) {
        const message =
          error instanceof Error ? error.message : "Failed to poll remote ICE candidates."
        setDisconnectedUi("error", "ICE error", message)
      }
    }

    async function destroyPeerConnection() {
      clearPolling()

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
    }

    async function connect() {
      const nextGeneration = activeGenerationRef.current + 1
      activeGenerationRef.current = nextGeneration

      await destroyPeerConnection()
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

      const peer = new window.RTCPeerConnection({
        bundlePolicy: "max-bundle",
        rtcpMuxPolicy: "require",
      })
      peerConnectionRef.current = peer

      peer.addEventListener("connectionstatechange", () => {
        if (disposed || nextGeneration !== activeGenerationRef.current) {
          return
        }

        if (peer.connectionState === "connected") {
          setConnectionState("streaming")
          setStatusText("Streaming")
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

      peer.addEventListener("track", (event) => {
        if (disposed || nextGeneration !== activeGenerationRef.current) {
          return
        }

        const [stream] = event.streams
        if (stream && videoRef.current) {
          videoRef.current.srcObject = stream
          setConnectionState("streaming")
          setStatusText("Streaming")
          setDetailText("Receiving live WebRTC viewport stream")
        }
      })

      peer.addTransceiver("video", {
        direction: "recvonly",
      })

      peer.addEventListener("icecandidate", async (event) => {
        if (disposed || nextGeneration !== activeGenerationRef.current || !event.candidate) {
          return
        }
        if (!event.candidate.candidate) {
          return
        }

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

      try {
        const offer = await peer.createOffer()
        await peer.setLocalDescription(offer)

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
        if (!answer.sdp || answer.type !== "answer") {
          throw new Error("Server did not return a valid WebRTC answer.")
        }

        await peer.setRemoteDescription(answer)
        remoteDescriptionAppliedRef.current = true
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

    void connect()

    return () => {
      disposed = true
      void destroyPeerConnection()
    }
  }, [serverOrigin])

  return (
    <div className="h-full flex flex-col bg-neutral-900">
      <div className="flex items-center justify-between h-8 bg-neutral-950 border-b border-neutral-800 px-2">
        <div className="flex items-center gap-2">
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Perspective
            <ChevronDown className="w-3 h-3" />
          </button>
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Lit
            <ChevronDown className="w-3 h-3" />
          </button>
          <button className="flex items-center gap-1 px-2 py-1 text-xs text-neutral-300 hover:bg-neutral-800 rounded">
            Show
            <ChevronDown className="w-3 h-3" />
          </button>
        </div>
        <div className="flex items-center gap-1">
          <ViewportButton icon={Grid3X3} />
          <ViewportButton icon={Eye} />
          <ViewportButton icon={Camera} />
          <ViewportButton icon={Maximize2} />
        </div>
      </div>
      <div className="flex-1 relative">
        <video
          ref={videoRef}
          className="absolute inset-0 w-full h-full object-contain bg-black"
          autoPlay
          playsInline
          muted
        />
        {!isConnected && (
          <div className="absolute inset-0 flex items-center justify-center bg-neutral-900/95">
            <div className="text-center">
              <div className="w-16 h-16 mx-auto mb-4 border-2 border-neutral-700 rounded-lg flex items-center justify-center">
                <Camera className="w-8 h-8 text-neutral-600" />
              </div>
              <p className="text-neutral-200 text-sm">{statusText}</p>
              <p className="text-neutral-500 text-xs mt-1">{detailText}</p>
              <p className="text-neutral-600 text-[11px] mt-3">
                Server: {serverOrigin}
              </p>
            </div>
          </div>
        )}
        <div className="absolute bottom-2 left-2 flex items-center gap-2 text-xs text-neutral-500">
          <span>{frameText}</span>
          <span>|</span>
          <span>{statusText}</span>
        </div>
        <div className="absolute top-2 right-2 flex items-center gap-2">
          <div className="px-2 py-1 bg-black/50 rounded text-xs text-neutral-400">
            {serverOrigin}
          </div>
        </div>
      </div>
    </div>
  )
}

function ViewportButton({ icon: Icon }: { icon: ElementType }) {
  return (
    <button className="p-1.5 text-neutral-400 hover:text-white hover:bg-neutral-800 rounded transition-colors">
      <Icon className="w-3.5 h-3.5" />
    </button>
  )
}
