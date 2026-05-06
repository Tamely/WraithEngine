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
} from "lucide-react"
import { useRemoteViewport, type SessionSceneItem, type SessionSceneItemKind } from "./remote-viewport-context"

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
  const { sceneTree, selectedObjectId, selectObject, participants, sessionState } =
    useRemoteViewport()
  const [searchQuery, setSearchQuery] = useState("")
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(
    new Set(["world", "lighting", "geometry"])
  )

  const visibleTree = useMemo(
    () => sceneTree.filter((item) => matchesSearch(item, searchQuery)),
    [sceneTree, searchQuery]
  )

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

    return (
      <div key={item.id}>
        <div
          className={`group flex cursor-pointer items-center gap-1 px-2 py-1 hover:bg-neutral-800 ${
            isSelected ? "bg-neutral-700" : ""
          }`}
          style={{ paddingLeft: `${depth * 16 + 8}px` }}
          onClick={() => void selectObject(item.id)}
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
          <span className="flex-1 truncate text-xs text-neutral-300">
            {item.displayName}
          </span>
          {selectedBy.length > 0 && (
            <span className="max-w-28 truncate text-[10px] text-amber-300/80">
              {selectedBy.join(", ")}
            </span>
          )}
          <button
            className="rounded p-0.5 opacity-0 hover:bg-neutral-700 group-hover:opacity-100"
            type="button"
          >
            {item.visible ? (
              <Eye className="h-3 w-3 text-neutral-500" />
            ) : (
              <EyeOff className="h-3 w-3 text-neutral-600" />
            )}
          </button>
        </div>
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
