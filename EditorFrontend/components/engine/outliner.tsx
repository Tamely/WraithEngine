"use client"

import { useMemo, useState } from "react"
import {
  ChevronRight,
  ChevronDown,
  Search,
  Eye,
  EyeOff,
  Folder,
  Box,
  Lightbulb,
  Camera,
  User,
  Layers,
  Plus,
  Trash2,
  Copy,
} from "lucide-react"
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuItem,
  ContextMenuSeparator,
  ContextMenuTrigger,
} from "@/components/ui/context-menu"
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu"
import { useRemoteViewport, type SessionSceneItem, type SessionSceneItemKind } from "./remote-viewport-context"

type EditingState = { id: string; name: string } | null

const OBJECT_TEMPLATES = [
  { id: "Folder", label: "Folder", Icon: Folder },
  { id: "Mesh", label: "Mesh", Icon: Box },
  { id: "Light", label: "Light", Icon: Lightbulb },
  { id: "Camera", label: "Camera", Icon: Camera },
  { id: "Actor", label: "Actor", Icon: User },
] as const

function matchesSearch(item: SessionSceneItem, query: string): boolean {
  if (query.length === 0) {
    return true
  }

  const normalized = query.toLowerCase()
  if (
    item.displayName.toLowerCase().includes(normalized) ||
    item.id.toLowerCase().includes(normalized)
  ) {
    return true
  }

  return item.children.some((child) => matchesSearch(child, query))
}

export function Outliner() {
  const {
    sceneTree,
    selectedObjectId,
    selectObject,
    setObjectVisibility,
    goToParticipantCamera,
    createObject,
    duplicateObject,
    deleteObject,
    renameObject,
    participants,
    sessionState,
  } = useRemoteViewport()
  const [searchQuery, setSearchQuery] = useState("")
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(
    new Set(["world", "lighting", "geometry"])
  )
  const [editing, setEditing] = useState<EditingState>(null)

  const visibleTree = useMemo(() => {
    const collaboratorItems: SessionSceneItem[] = participants
      .filter((participant) => participant.userId !== 1 && participant.camera !== null)
      .map((participant) => ({
        id: `participant-camera-${participant.userId}`,
        displayName: participant.displayName,
        kind: "camera" as const,
        visible: true,
        children: [],
      }))

    const treeWithParticipants = [...collaboratorItems, ...sceneTree]

    return treeWithParticipants.filter((item) => matchesSearch(item, searchQuery))
  }, [participants, sceneTree, searchQuery])

  const toggleFolder = (id: string) => {
    setExpandedFolders((prev) => {
      const next = new Set(prev)
      if (next.has(id)) {
        next.delete(id)
      } else {
        next.add(id)
      }
      return next
    })
  }

  const getIcon = (type: SessionSceneItemKind) => {
    switch (type) {
      case "folder":
        return Folder
      case "mesh":
        return Box
      case "light":
        return Lightbulb
      case "camera":
        return Camera
      case "actor":
        return User
      default:
        return Box
    }
  }

  const renderItem = (item: SessionSceneItem, depth = 0) => {
    const Icon = getIcon(item.kind)
    const isExpanded = expandedFolders.has(item.id)
    const isSelected = selectedObjectId === item.id
    const hasChildren = item.children.length > 0
    const selectedBy = participants
      .filter((participant) => participant.selectionObjectId === item.id)
      .map((participant) => {
        return participant.displayName
      })

    if (searchQuery.length > 0 && !matchesSearch(item, searchQuery)) {
      return null
    }

    const participantCameraUserId = item.id.startsWith("participant-camera-")
      ? Number(item.id.replace("participant-camera-", ""))
      : null

    const isEditing = editing !== null && editing.id === item.id
    const isParticipantCamera = participantCameraUserId !== null && Number.isFinite(participantCameraUserId)

    function startEditing() {
      setEditing({ id: item.id, name: item.displayName })
    }

    function commitEdit() {
      if (editing === null || editing.id !== item.id) return
      const trimmed = editing.name.trim()
      if (trimmed.length > 0 && trimmed !== item.displayName) {
        void renameObject(item.id, trimmed)
      }
      setEditing(null)
    }

    function cancelEdit() {
      setEditing(null)
    }

    const row = (
      <div
        className={`group flex cursor-pointer items-center gap-1 px-2 py-1 hover:bg-neutral-800 ${
          isSelected ? "bg-neutral-700" : ""
        }`}
        style={{ paddingLeft: `${depth * 16 + 8}px` }}
        onClick={() => {
          if (isParticipantCamera || isEditing) return
          void selectObject(item.id)
        }}
      >
        {hasChildren ? (
          <button
            className="rounded p-0.5 hover:bg-neutral-700"
            onClick={(event) => {
              event.stopPropagation()
              toggleFolder(item.id)
            }}
            type="button"
          >
            {isExpanded ? (
              <ChevronDown className="h-3 w-3 text-neutral-400" />
            ) : (
              <ChevronRight className="h-3 w-3 text-neutral-400" />
            )}
          </button>
        ) : (
          <div className="w-4" />
        )}
        <Icon className="h-4 w-4 text-neutral-400" />
        {isEditing ? (
          <input
            autoFocus
            className="flex-1 rounded bg-neutral-900 px-1 text-xs text-neutral-100 outline outline-1 outline-blue-500"
            onBlur={commitEdit}
            onChange={(e) => setEditing({ id: item.id, name: e.target.value })}
            onClick={(e) => e.stopPropagation()}
            onFocus={(e) => e.target.select()}
            onKeyDown={(e) => {
              if (e.key === "Enter") {
                e.preventDefault()
                commitEdit()
              } else if (e.key === "Escape") {
                e.preventDefault()
                cancelEdit()
              }
            }}
            type="text"
            value={editing.name}
          />
        ) : (
          <span
            className="flex-1 truncate text-xs text-neutral-300"
            onDoubleClick={(e) => {
              if (isParticipantCamera) return
              e.stopPropagation()
              startEditing()
            }}
          >
            {item.displayName}
          </span>
        )}
        {!isEditing && selectedBy.length > 0 && (
          <span className="max-w-28 truncate text-[10px] text-amber-300/80">
            {selectedBy.join(", ")}
          </span>
        )}
        <button
          className="rounded p-0.5 opacity-0 hover:bg-neutral-700 group-hover:opacity-100"
          onClick={(event) => {
            event.stopPropagation()
            if (isParticipantCamera) return
            void setObjectVisibility(item.id, !item.visible)
          }}
          type="button"
        >
          {item.visible ? (
            <Eye className="h-3 w-3 text-neutral-500" />
          ) : (
            <EyeOff className="h-3 w-3 text-neutral-600" />
          )}
        </button>
      </div>
    )

    return (
      <div key={item.id}>
        <ContextMenu>
          <ContextMenuTrigger asChild>{row}</ContextMenuTrigger>
          <ContextMenuContent className="border-neutral-800 bg-neutral-950 text-neutral-200">
            {participantCameraUserId !== null && Number.isFinite(participantCameraUserId) ? (
              <ContextMenuItem
                onClick={() => {
                  void goToParticipantCamera(participantCameraUserId)
                }}
              >
                Go to Camera
              </ContextMenuItem>
            ) : (
              <>
                <ContextMenuItem
                  onClick={() => {
                    void duplicateObject(item.id)
                  }}
                >
                  <Copy className="mr-2 h-3.5 w-3.5" />
                  Duplicate
                </ContextMenuItem>
                <ContextMenuSeparator className="bg-neutral-800" />
                <ContextMenuItem
                  className="text-red-400 focus:text-red-400"
                  onClick={() => {
                    void deleteObject(item.id)
                  }}
                >
                  <Trash2 className="mr-2 h-3.5 w-3.5" />
                  Delete
                </ContextMenuItem>
              </>
            )}
          </ContextMenuContent>
        </ContextMenu>
        {hasChildren && isExpanded && item.children.map((child) => renderItem(child, depth + 1))}
      </div>
    )
  }

  return (
    <div className="flex h-full flex-col overflow-hidden">
      <div className="flex h-8 items-center justify-between border-b border-neutral-800 bg-neutral-950 px-2">
        <div className="flex items-center gap-1">
          <Layers className="h-4 w-4 text-neutral-400" />
          <span className="text-xs font-medium text-neutral-300">Outliner</span>
        </div>
        <div className="flex items-center gap-1">
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <button
                className="flex items-center gap-0.5 rounded px-1.5 py-0.5 text-xs text-neutral-400 hover:bg-neutral-800 hover:text-neutral-200"
                type="button"
              >
                <Plus className="h-3 w-3" />
                Add
              </button>
            </DropdownMenuTrigger>
            <DropdownMenuContent className="border-neutral-800 bg-neutral-950 text-neutral-200">
              {OBJECT_TEMPLATES.map(({ id, label, Icon }) => (
                <DropdownMenuItem
                  key={id}
                  onClick={() => {
                    void createObject(id)
                  }}
                >
                  <Icon className="mr-2 h-3.5 w-3.5 text-neutral-400" />
                  {label}
                </DropdownMenuItem>
              ))}
            </DropdownMenuContent>
          </DropdownMenu>
          <button
            className="rounded p-0.5 text-neutral-400 hover:bg-neutral-800 hover:text-red-400 disabled:cursor-not-allowed disabled:opacity-30"
            disabled={selectedObjectId === null}
            onClick={() => {
              if (selectedObjectId !== null) {
                void deleteObject(selectedObjectId)
              }
            }}
            title="Delete selected"
            type="button"
          >
            <Trash2 className="h-3.5 w-3.5" />
          </button>
        </div>
      </div>
      <div className="p-2">
        <div className="flex items-center gap-2 rounded border border-neutral-700 bg-neutral-900 px-2 py-1.5">
          <Search className="h-3.5 w-3.5 text-neutral-500" />
          <input
            className="flex-1 bg-transparent text-xs text-neutral-300 outline-none placeholder:text-neutral-600"
            onChange={(event) => setSearchQuery(event.target.value)}
            placeholder="Search..."
            type="text"
            value={searchQuery}
          />
        </div>
      </div>
      <div className="flex-1 overflow-y-auto">
        {visibleTree.length === 0 ? (
          <div className="px-3 py-2 text-xs text-neutral-600">
            {sessionState === "snapshot-loading" || sessionState === "connecting"
              ? "Loading authoritative scene data..."
              : sessionState === "reconnecting"
                ? "Reconnecting to authoritative session..."
                : "No session scene data yet"}
          </div>
        ) : (
          visibleTree.map((item) => renderItem(item))
        )}
      </div>
    </div>
  )
}
