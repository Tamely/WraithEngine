"use client"

import { useState } from "react"
import {
  Search,
  Plus,
  FolderOpen,
  ChevronRight,
  ChevronDown,
  Folder,
  FileImage,
  Box,
  Settings,
  Filter,
  Grid3X3,
  List,
} from "lucide-react"

interface FolderItem {
  id: string
  name: string
  children?: FolderItem[]
}

interface Asset {
  id: string
  name: string
  type: "material" | "texture" | "mesh" | "blueprint"
  color?: string
  thumbnail?: string
}

const folderStructure: FolderItem[] = [
  {
    id: "content",
    name: "Content",
    children: [
      {
        id: "materials",
        name: "Materials",
        children: [
          { id: "metals", name: "Metals" },
          { id: "fabrics", name: "Fabrics" },
        ],
      },
      { id: "meshes", name: "Meshes" },
      { id: "textures", name: "Textures" },
      { id: "blueprints", name: "Blueprints" },
    ],
  },
]

const assets: Asset[] = [
  { id: "m1", name: "M_Chrome", type: "material", color: "#888888" },
  { id: "m2", name: "M_Gold", type: "material", color: "#FFD700" },
  { id: "m3", name: "M_Copper", type: "material", color: "#B87333" },
  { id: "m4", name: "M_Silver", type: "material", color: "#C0C0C0" },
  { id: "m5", name: "M_Brushed", type: "material", color: "#A8A8A8" },
  { id: "m6", name: "M_Plastic_Red", type: "material", color: "#E74C3C" },
  { id: "m7", name: "M_Plastic_Green", type: "material", color: "#2ECC71" },
  { id: "m8", name: "M_Plastic_Blue", type: "material", color: "#3498DB" },
  { id: "m9", name: "M_Ceramic", type: "material", color: "#ECF0F1" },
  { id: "m10", name: "M_Glass", type: "material", color: "#85C1E9" },
  { id: "m11", name: "M_Rubber", type: "material", color: "#2C3E50" },
  { id: "m12", name: "M_Wood", type: "material", color: "#8B4513" },
]

export function ContentBrowser() {
  const [searchQuery, setSearchQuery] = useState("")
  const [selectedFolder, setSelectedFolder] = useState("materials")
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(new Set(["content", "materials"]))
  const [viewMode, setViewMode] = useState<"grid" | "list">("grid")
  const [selectedAsset, setSelectedAsset] = useState<string | null>(null)

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

  const renderFolder = (folder: FolderItem, depth = 0) => {
    const isExpanded = expandedFolders.has(folder.id)
    const isSelected = selectedFolder === folder.id
    const hasChildren = folder.children && folder.children.length > 0

    return (
      <div key={folder.id}>
        <div
          className={`flex items-center gap-1 px-2 py-1 cursor-pointer hover:bg-neutral-800 ${isSelected ? "bg-neutral-700" : ""
            }`}
          style={{ paddingLeft: `${depth * 12 + 8}px` }}
          onClick={() => setSelectedFolder(folder.id)}
        >
          {hasChildren ? (
            <button
              onClick={(e) => {
                e.stopPropagation()
                toggleFolder(folder.id)
              }}
              className="p-0.5"
            >
              {isExpanded ? (
                <ChevronDown className="w-3 h-3 text-neutral-500" />
              ) : (
                <ChevronRight className="w-3 h-3 text-neutral-500" />
              )}
            </button>
          ) : (
            <div className="w-4" />
          )}
          <Folder className="w-4 h-4 text-yellow-500" />
          <span className="text-xs text-neutral-300">{folder.name}</span>
        </div>
        {hasChildren && isExpanded && folder.children!.map((child) => renderFolder(child, depth + 1))}
      </div>
    )
  }

  return (
    <div className="h-full flex flex-col bg-neutral-950">
      <div className="flex items-center h-8 border-b border-neutral-800 px-2 gap-2">
        <button className="flex items-center gap-1 px-2 py-1 text-xs bg-neutral-800 hover:bg-neutral-700 text-neutral-300 rounded">
          <Plus className="w-3 h-3" />
          Add
        </button>
        <button className="flex items-center gap-1 px-2 py-1 text-xs hover:bg-neutral-800 text-neutral-400 rounded">
          <FolderOpen className="w-3 h-3" />
          Import
        </button>
        <button className="flex items-center gap-1 px-2 py-1 text-xs hover:bg-neutral-800 text-neutral-400 rounded">
          Save All
        </button>
        <div className="flex-1" />
        <div className="flex items-center gap-1 text-xs text-neutral-500">
          <span>Content</span>
          <ChevronRight className="w-3 h-3" />
          <span>Materials</span>
        </div>
      </div>
      <div className="flex flex-1 overflow-hidden">
        <div className="w-48 border-r border-neutral-800 overflow-y-auto">
          {folderStructure.map((folder) => renderFolder(folder))}
        </div>
        <div className="flex-1 flex flex-col">
          <div className="flex items-center gap-2 p-2 border-b border-neutral-800">
            <div className="flex items-center gap-2 flex-1 px-2 py-1 bg-neutral-900 border border-neutral-700 rounded">
              <Search className="w-3.5 h-3.5 text-neutral-500" />
              <input
                type="text"
                placeholder="Search assets..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="flex-1 bg-transparent text-xs text-neutral-300 outline-none placeholder:text-neutral-600"
              />
            </div>
            <button className="p-1.5 hover:bg-neutral-800 rounded">
              <Filter className="w-3.5 h-3.5 text-neutral-500" />
            </button>
            <button
              onClick={() => setViewMode("grid")}
              className={`p-1.5 rounded ${viewMode === "grid" ? "bg-neutral-700" : "hover:bg-neutral-800"}`}
            >
              <Grid3X3 className="w-3.5 h-3.5 text-neutral-400" />
            </button>
            <button
              onClick={() => setViewMode("list")}
              className={`p-1.5 rounded ${viewMode === "list" ? "bg-neutral-700" : "hover:bg-neutral-800"}`}
            >
              <List className="w-3.5 h-3.5 text-neutral-400" />
            </button>
          </div>
          <div className="flex-1 overflow-y-auto p-2">
            <div className="grid grid-cols-8 gap-2">
              {assets.map((asset) => (
                <div
                  key={asset.id}
                  onClick={() => setSelectedAsset(asset.id)}
                  className={`flex flex-col items-center p-1 rounded cursor-pointer hover:bg-neutral-800 ${selectedAsset === asset.id ? "bg-neutral-700 ring-1 ring-white/30" : ""
                    }`}
                >
                  <div
                    className="w-12 h-12 rounded-full mb-1 border border-neutral-700"
                    style={{
                      background: `radial-gradient(circle at 30% 30%, ${asset.color}, ${asset.color}88)`,
                      boxShadow: `inset -4px -4px 8px rgba(0,0,0,0.4), inset 2px 2px 4px rgba(255,255,255,0.2)`,
                    }}
                  />
                  <span className="text-[10px] text-neutral-400 text-center truncate w-full">
                    {asset.name}
                  </span>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
