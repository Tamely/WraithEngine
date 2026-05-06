"use client"

import { useState } from "react"
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

interface OutlinerProps {
  selectedItem: string | null
  onSelectItem: (item: string) => void
}

interface SceneItem {
  id: string
  name: string
  type: "folder" | "mesh" | "light" | "camera" | "actor"
  visible: boolean
  children?: SceneItem[]
}

const sceneHierarchy: SceneItem[] = [
  {
    id: "world",
    name: "World",
    type: "folder",
    visible: true,
    children: [
      {
        id: "lighting",
        name: "Lighting",
        type: "folder",
        visible: true,
        children: [
          { id: "directional-light", name: "DirectionalLight", type: "light", visible: true },
          { id: "sky-light", name: "SkyLight", type: "light", visible: true },
        ],
      },
      {
        id: "geometry",
        name: "Geometry",
        type: "folder",
        visible: true,
        children: [
          { id: "floor", name: "Floor_Platform", type: "mesh", visible: true },
          { id: "crate-1", name: "Crate_01", type: "mesh", visible: true },
          { id: "crate-2", name: "Crate_02", type: "mesh", visible: true },
          { id: "wall-panel", name: "WallPanel_SciFi", type: "mesh", visible: true },
        ],
      },
      { id: "PlayerCharacter", name: "PlayerCharacter", type: "actor", visible: true },
      { id: "main-camera", name: "MainCamera", type: "camera", visible: true },
    ],
  },
]

export function Outliner({ selectedItem, onSelectItem }: OutlinerProps) {
  const [searchQuery, setSearchQuery] = useState("")
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(
    new Set(["world", "lighting", "geometry"])
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

  const getIcon = (type: SceneItem["type"]) => {
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

  const renderItem = (item: SceneItem, depth = 0) => {
    const Icon = getIcon(item.type)
    const isExpanded = expandedFolders.has(item.id)
    const isSelected = selectedItem === item.id
    const hasChildren = item.children && item.children.length > 0

    return (
      <div key={item.id}>
        <div
          className={`flex items-center gap-1 px-2 py-1 cursor-pointer hover:bg-neutral-800 ${isSelected ? "bg-neutral-700" : ""
            }`}
          style={{ paddingLeft: `${depth * 16 + 8}px` }}
          onClick={() => onSelectItem(item.id)}
        >
          {hasChildren ? (
            <button
              onClick={(e) => {
                e.stopPropagation()
                toggleFolder(item.id)
              }}
              className="p-0.5 hover:bg-neutral-700 rounded"
            >
              {isExpanded ? (
                <ChevronDown className="w-3 h-3 text-neutral-400" />
              ) : (
                <ChevronRight className="w-3 h-3 text-neutral-400" />
              )}
            </button>
          ) : (
            <div className="w-4" />
          )}
          <Icon className="w-4 h-4 text-neutral-400" />
          <span className="text-xs text-neutral-300 flex-1 truncate">{item.name}</span>
          <button className="p-0.5 opacity-0 group-hover:opacity-100 hover:bg-neutral-700 rounded">
            {item.visible ? (
              <Eye className="w-3 h-3 text-neutral-500" />
            ) : (
              <EyeOff className="w-3 h-3 text-neutral-600" />
            )}
          </button>
        </div>
        {hasChildren && isExpanded && item.children!.map((child) => renderItem(child, depth + 1))}
      </div>
    )
  }

  return (
    <div className="h-full flex flex-col overflow-hidden">
      <div className="flex items-center justify-between h-8 bg-neutral-950 border-b border-neutral-800 px-2">
        <div className="flex items-center gap-1">
          <Layers className="w-4 h-4 text-neutral-400" />
          <span className="text-xs font-medium text-neutral-300">Outliner</span>
        </div>
      </div>
      <div className="p-2">
        <div className="flex items-center gap-2 px-2 py-1.5 bg-neutral-900 border border-neutral-700 rounded">
          <Search className="w-3.5 h-3.5 text-neutral-500" />
          <input
            type="text"
            placeholder="Search..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="flex-1 bg-transparent text-xs text-neutral-300 outline-none placeholder:text-neutral-600"
          />
        </div>
      </div>
      <div className="flex-1 overflow-y-auto">
        {sceneHierarchy.map((item) => renderItem(item))}
      </div>
    </div>
  )
}
