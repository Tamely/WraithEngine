"use client"

import { useState, useRef, useCallback, useEffect } from "react"
import {
  Search,
  Plus,
  FolderOpen,
  ChevronRight,
  ChevronDown,
  Folder,
  Box,
  ImageIcon,
  Filter,
  Grid3X3,
  List,
} from "lucide-react"
import { useRemoteViewport, type SessionAssetDescriptor, type SessionSceneItem } from "./remote-viewport-context"

type AssetKindFilter = "all" | "mesh" | "texture"

interface FolderItem {
  id: AssetKindFilter
  name: string
  children?: FolderItem[]
}

const folderStructure: FolderItem[] = [
  {
    id: "all",
    name: "Content",
    children: [
      { id: "mesh", name: "Meshes" },
      { id: "texture", name: "Textures" },
    ],
  },
]

function AssetIcon({ kind }: { kind: SessionAssetDescriptor["kind"] }) {
  if (kind === "texture") return <ImageIcon className="w-8 h-8 text-blue-400" />
  return <Box className="w-8 h-8 text-orange-400" />
}

export function ContentBrowser() {
  const { assets, listAssets, connectionState, selectedObject, setMeshAsset } = useRemoteViewport()
  const [searchQuery, setSearchQuery] = useState("")
  const [selectedFolder, setSelectedFolder] = useState<AssetKindFilter>("all")
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(
    new Set(["all"])
  )
  const [viewMode, setViewMode] = useState<"grid" | "list">("grid")
  const [selectedAsset, setSelectedAsset] = useState<number | null>(null)
  const [folderWidth, setFolderWidth] = useState(192)
  const isDraggingFolderRef = useRef(false)
  const startXRef = useRef(0)
  const startWidthRef = useRef(0)

  useEffect(() => {
    if (connectionState === "control-ready" || connectionState === "streaming") {
      void listAssets()
    }
  }, [connectionState, listAssets])

  const canAssignToSelection =
    selectedObject?.kind === "mesh" && selectedObject.id !== undefined

  const assignAssetToSelection = useCallback(
    async (asset: SessionAssetDescriptor) => {
      if (!canAssignToSelection || asset.kind !== "mesh" || !selectedObject) return
      await setMeshAsset(selectedObject.id, asset.path)
    },
    [canAssignToSelection, selectedObject, setMeshAsset]
  )

  const filteredAssets = assets.filter((a) => {
    const matchesFolder = selectedFolder === "all" || a.kind === selectedFolder
    const matchesSearch =
      searchQuery === "" ||
      a.name.toLowerCase().includes(searchQuery.toLowerCase())
    return matchesFolder && matchesSearch
  })

  const onFolderSplitterMouseDown = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault()
      isDraggingFolderRef.current = true
      startXRef.current = e.clientX
      startWidthRef.current = folderWidth

      const onMouseMove = (ev: MouseEvent) => {
        if (!isDraggingFolderRef.current) return
        const delta = ev.clientX - startXRef.current
        setFolderWidth(Math.max(100, Math.min(400, startWidthRef.current + delta)))
      }
      const onMouseUp = () => {
        isDraggingFolderRef.current = false
        window.removeEventListener("mousemove", onMouseMove)
        window.removeEventListener("mouseup", onMouseUp)
      }
      window.addEventListener("mousemove", onMouseMove)
      window.addEventListener("mouseup", onMouseUp)
    },
    [folderWidth]
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

  const renderFolder = (folder: FolderItem, depth = 0) => {
    const isExpanded = expandedFolders.has(folder.id)
    const isSelected = selectedFolder === folder.id
    const hasChildren = !!folder.children?.length

    return (
      <div key={folder.id}>
        <div
          className={`flex items-center gap-1 px-2 py-1 cursor-pointer hover:bg-neutral-800 ${
            isSelected ? "bg-neutral-700" : ""
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
        {hasChildren &&
          isExpanded &&
          folder.children!.map((child) => renderFolder(child, depth + 1))}
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
        <button
          onClick={() => void listAssets()}
          className="flex items-center gap-1 px-2 py-1 text-xs hover:bg-neutral-800 text-neutral-400 rounded"
        >
          Refresh
        </button>
        <div className="flex-1" />
        <div className="flex items-center gap-1 text-xs text-neutral-500">
          <span>Content</span>
          {selectedFolder !== "all" && (
            <>
              <ChevronRight className="w-3 h-3" />
              <span className="capitalize">{selectedFolder}s</span>
            </>
          )}
        </div>
      </div>
      <div className="flex flex-1 overflow-hidden">
        <div
          className="shrink-0 overflow-y-auto border-r border-neutral-800"
          style={{ width: folderWidth }}
        >
          {folderStructure.map((folder) => renderFolder(folder))}
        </div>
        <div
          className="w-1 shrink-0 bg-transparent hover:bg-white/20 active:bg-white/30 cursor-col-resize transition-colors relative group"
          onMouseDown={onFolderSplitterMouseDown}
        >
          <div className="absolute inset-y-0 -left-0.5 -right-0.5 group-hover:bg-white/10" />
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
            {filteredAssets.length === 0 ? (
              <div className="flex items-center justify-center h-full text-xs text-neutral-600">
                {assets.length === 0 ? "No assets loaded" : "No results"}
              </div>
            ) : viewMode === "grid" ? (
              <div className="grid grid-cols-8 gap-2">
                {filteredAssets.map((asset) => (
                  <div
                    key={asset.id}
                    draggable={asset.kind === "mesh"}
                    onClick={() => setSelectedAsset(asset.id)}
                    onDoubleClick={() => void assignAssetToSelection(asset)}
                    onDragStart={(e) => {
                      e.dataTransfer.setData("axiom/asset-path", asset.path)
                      e.dataTransfer.setData("axiom/asset-kind", asset.kind)
                      e.dataTransfer.effectAllowed = "copy"
                    }}
                    title={canAssignToSelection && asset.kind === "mesh" ? "Double-click or drag to assign to a mesh object" : asset.path}
                    className={`flex flex-col items-center p-1 rounded cursor-pointer hover:bg-neutral-800 ${
                      selectedAsset === asset.id
                        ? "bg-neutral-700 ring-1 ring-white/30"
                        : ""
                    }`}
                  >
                    <div className="w-12 h-12 flex items-center justify-center mb-1">
                      <AssetIcon kind={asset.kind} />
                    </div>
                    <span className="text-[10px] text-neutral-400 text-center truncate w-full">
                      {asset.name}
                    </span>
                  </div>
                ))}
              </div>
            ) : (
              <div className="flex flex-col gap-0.5">
                {filteredAssets.map((asset) => (
                  <div
                    key={asset.id}
                    draggable={asset.kind === "mesh"}
                    onClick={() => setSelectedAsset(asset.id)}
                    onDoubleClick={() => void assignAssetToSelection(asset)}
                    onDragStart={(e) => {
                      e.dataTransfer.setData("axiom/asset-path", asset.path)
                      e.dataTransfer.setData("axiom/asset-kind", asset.kind)
                      e.dataTransfer.effectAllowed = "copy"
                    }}
                    title={canAssignToSelection && asset.kind === "mesh" ? "Double-click or drag to assign to a mesh object" : asset.path}
                    className={`flex items-center gap-2 px-2 py-1 rounded cursor-pointer hover:bg-neutral-800 ${
                      selectedAsset === asset.id
                        ? "bg-neutral-700 ring-1 ring-white/30"
                        : ""
                    }`}
                  >
                    <AssetIcon kind={asset.kind} />
                    <div className="flex-1 min-w-0">
                      <div className="text-xs text-neutral-300 truncate">
                        {asset.name}
                      </div>
                      <div className="text-[10px] text-neutral-600 truncate">
                        {asset.path}
                      </div>
                    </div>
                    <span className="text-[10px] text-neutral-600 capitalize shrink-0">
                      {asset.kind}
                    </span>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  )
}
