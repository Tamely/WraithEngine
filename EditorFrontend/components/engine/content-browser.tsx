"use client"

import { useState, useRef, useCallback, useEffect, useMemo } from "react"
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
import { useRemoteViewport, type SessionAssetDescriptor } from "./remote-viewport-context"

function TextureThumbnail({
  assetPath,
  serverOrigin,
  size = "md",
}: {
  assetPath: string
  serverOrigin: string
  size?: "md" | "sm"
}) {
  const [src, setSrc] = useState<string | null>(null)
  const [failed, setFailed] = useState(false)

  useEffect(() => {
    if (!serverOrigin) return
    setSrc(`${serverOrigin}/assets/thumbnail?path=${encodeURIComponent(assetPath)}`)
    setFailed(false)
  }, [assetPath, serverOrigin])

  const iconClass = size === "sm" ? "w-4 h-4" : "w-8 h-8"
  const imgClass = size === "sm" ? "w-4 h-4 object-cover rounded" : "w-12 h-12 object-cover rounded"

  if (!src || failed) {
    return <ImageIcon className={`${iconClass} text-blue-400`} />
  }

  return (
    <img
      src={src}
      alt={assetPath}
      className={imgClass}
      onError={() => setFailed(true)}
    />
  )
}

function AssetIcon({
  kind,
  path,
  serverOrigin,
  size = "md",
}: {
  kind: SessionAssetDescriptor["kind"]
  path: string
  serverOrigin: string
  size?: "md" | "sm"
}) {
  if (kind === "texture") {
    return <TextureThumbnail assetPath={path} serverOrigin={serverOrigin} size={size} />
  }
  const cls = size === "sm" ? "w-4 h-4 text-orange-400" : "w-8 h-8 text-orange-400"
  return <Box className={cls} />
}

interface FolderNode {
  name: string
  path: string
  children: FolderNode[]
}

function buildFolderTree(assets: SessionAssetDescriptor[]): FolderNode {
  const root: FolderNode = { name: "Content", path: "", children: [] }
  const nodesByPath = new Map<string, FolderNode>([["", root]])

  for (const asset of assets) {
    const parts = asset.path.split("/")
    for (let depth = 1; depth < parts.length; depth++) {
      const fullPath = parts.slice(0, depth).join("/")
      if (!nodesByPath.has(fullPath)) {
        const parentPath = parts.slice(0, depth - 1).join("/")
        const node: FolderNode = { name: parts[depth - 1], path: fullPath, children: [] }
        nodesByPath.set(fullPath, node)
        nodesByPath.get(parentPath)!.children.push(node)
      }
    }
  }

  return root
}

function getDirectoryContents(assets: SessionAssetDescriptor[], currentPath: string) {
  const subdirSet = new Set<string>()
  const files: SessionAssetDescriptor[] = []

  for (const asset of assets) {
    const lastSlash = asset.path.lastIndexOf("/")
    const dir = lastSlash === -1 ? "" : asset.path.substring(0, lastSlash)

    if (dir === currentPath) {
      files.push(asset)
    } else {
      const prefix = currentPath === "" ? "" : currentPath + "/"
      if (currentPath === "" ? dir.length > 0 : dir.startsWith(prefix)) {
        const rest = currentPath === "" ? dir : dir.substring(prefix.length)
        const immediateChild = rest.split("/")[0]
        if (immediateChild) subdirSet.add(immediateChild)
      }
    }
  }

  return { subdirs: Array.from(subdirSet).sort(), files }
}

export function ContentBrowser() {
  const {
    assets,
    listAssets,
    connectionState,
    selectedObject,
    setMeshAsset,
    setMaterialTexture,
    serverOrigin,
  } = useRemoteViewport()
  const [searchQuery, setSearchQuery] = useState("")
  const [currentPath, setCurrentPath] = useState("")
  const [expandedPaths, setExpandedPaths] = useState<Set<string>>(new Set([""]))
  const [viewMode, setViewMode] = useState<"grid" | "list">("grid")
  const [selectedAsset, setSelectedAsset] = useState<number | null>(null)
  const [folderWidth, setFolderWidth] = useState(192)
  const [isExternalDragOver, setIsExternalDragOver] = useState(false)
  const isDraggingFolderRef = useRef(false)
  const startXRef = useRef(0)
  const startWidthRef = useRef(0)

  const uploadFiles = useCallback(
    async (fileList: FileList, targetDir: string) => {
      if (!serverOrigin || fileList.length === 0) return
      const form = new FormData()
      for (const file of fileList) {
        form.append("file", file, file.name)
      }
      const dirParam = targetDir ? `?dir=${encodeURIComponent(targetDir)}` : ""
      try {
        await fetch(`${serverOrigin}/assets/upload${dirParam}`, {
          method: "POST",
          body: form,
        })
        void listAssets()
      } catch {
        // upload failed silently — listAssets refresh will show what made it
      }
    },
    [serverOrigin, listAssets]
  )

  const isExternalDrag = (e: React.DragEvent) =>
    e.dataTransfer.types.includes("Files") &&
    !e.dataTransfer.types.includes("axiom/asset-path")

  useEffect(() => {
    if (connectionState === "control-ready" || connectionState === "streaming") {
      void listAssets()
    }
  }, [connectionState, listAssets])

  const folderTree = useMemo(() => buildFolderTree(assets), [assets])

  const { subdirs, files } = useMemo(() => {
    if (searchQuery) {
      const q = searchQuery.toLowerCase()
      return {
        subdirs: [],
        files: assets.filter((a) => a.name.toLowerCase().includes(q)),
      }
    }
    return getDirectoryContents(assets, currentPath)
  }, [assets, currentPath, searchQuery])

  const canAssignToSelection =
    selectedObject?.kind === "mesh" && selectedObject.id !== undefined

  const assignAssetToSelection = useCallback(
    async (asset: SessionAssetDescriptor) => {
      if (!canAssignToSelection || !selectedObject) return
      if (asset.kind === "mesh") {
        await setMeshAsset(selectedObject.id, asset.path)
      } else if (asset.kind === "texture") {
        await setMaterialTexture(selectedObject.id, asset.path)
      }
    },
    [canAssignToSelection, selectedObject, setMeshAsset, setMaterialTexture]
  )

  const navigateTo = (path: string) => {
    setCurrentPath(path)
    setSelectedAsset(null)
    setExpandedPaths((prev) => {
      const next = new Set(prev)
      next.add(path)
      return next
    })
  }

  const breadcrumbs = useMemo(() => {
    const crumbs: { name: string; path: string }[] = [{ name: "Content", path: "" }]
    if (currentPath) {
      const parts = currentPath.split("/")
      parts.forEach((part, i) => {
        crumbs.push({ name: part, path: parts.slice(0, i + 1).join("/") })
      })
    }
    return crumbs
  }, [currentPath])

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

  const toggleExpand = (path: string, e: React.MouseEvent) => {
    e.stopPropagation()
    setExpandedPaths((prev) => {
      const next = new Set(prev)
      if (next.has(path)) {
        next.delete(path)
      } else {
        next.add(path)
      }
      return next
    })
  }

  const renderFolderNode = (node: FolderNode, depth = 0): React.ReactNode => {
    const isExpanded = expandedPaths.has(node.path)
    const isSelected = currentPath === node.path
    const hasChildren = node.children.length > 0

    return (
      <div key={node.path}>
        <div
          className={`flex items-center gap-1 py-1 cursor-pointer hover:bg-neutral-800 ${
            isSelected ? "bg-neutral-700" : ""
          }`}
          style={{ paddingLeft: `${depth * 12 + 8}px` }}
          onClick={() => navigateTo(node.path)}
        >
          {hasChildren ? (
            <button className="p-0.5" onClick={(e) => toggleExpand(node.path, e)}>
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
          <span className="text-xs text-neutral-300">{node.name}</span>
        </div>
        {hasChildren &&
          isExpanded &&
          node.children.map((child) => renderFolderNode(child, depth + 1))}
      </div>
    )
  }

  const isEmpty = subdirs.length === 0 && files.length === 0

  return (
    <div className="h-full flex flex-col bg-neutral-950">
      <div className="flex items-center h-8 border-b border-neutral-800 px-2 gap-2">
        <button className="flex items-center gap-1 px-2 py-1 text-xs bg-neutral-800 hover:bg-neutral-700 text-neutral-300 rounded">
          <Plus className="w-3 h-3" />
          Add
        </button>
        <label className="flex items-center gap-1 px-2 py-1 text-xs hover:bg-neutral-800 text-neutral-400 rounded cursor-pointer">
          <FolderOpen className="w-3 h-3" />
          Import
          <input
            type="file"
            multiple
            accept=".glb,.gltf,.fbx,.obj,.png,.jpg,.jpeg"
            className="sr-only"
            onChange={(e) => {
              if (e.target.files && e.target.files.length > 0) {
                void uploadFiles(e.target.files, currentPath)
                e.target.value = ""
              }
            }}
          />
        </label>
        <button
          onClick={() => void listAssets()}
          className="flex items-center gap-1 px-2 py-1 text-xs hover:bg-neutral-800 text-neutral-400 rounded"
        >
          Refresh
        </button>
        <div className="flex-1" />
        {/* Breadcrumb */}
        <div className="flex items-center gap-1 text-xs text-neutral-500">
          {breadcrumbs.map((crumb, i) => (
            <span key={crumb.path} className="flex items-center gap-1">
              {i > 0 && <ChevronRight className="w-3 h-3" />}
              <button
                onClick={() => navigateTo(crumb.path)}
                className={`hover:text-neutral-300 ${
                  i === breadcrumbs.length - 1 ? "text-neutral-300" : ""
                }`}
              >
                {crumb.name}
              </button>
            </span>
          ))}
        </div>
      </div>
      <div className="flex flex-1 overflow-hidden">
        <div
          className="shrink-0 overflow-y-auto border-r border-neutral-800"
          style={{ width: folderWidth }}
        >
          {renderFolderNode(folderTree)}
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
          <div
            className={`flex-1 overflow-y-auto p-2 relative ${isExternalDragOver ? "ring-2 ring-inset ring-blue-500/60" : ""}`}
            onDragOver={(e) => {
              if (!isExternalDrag(e)) return
              e.preventDefault()
              e.dataTransfer.dropEffect = "copy"
              setIsExternalDragOver(true)
            }}
            onDragLeave={(e) => {
              if (e.currentTarget.contains(e.relatedTarget as Node)) return
              setIsExternalDragOver(false)
            }}
            onDrop={(e) => {
              if (!isExternalDrag(e)) return
              e.preventDefault()
              setIsExternalDragOver(false)
              if (e.dataTransfer.files.length > 0) {
                void uploadFiles(e.dataTransfer.files, currentPath)
              }
            }}
          >
            {isExternalDragOver && (
              <div className="pointer-events-none absolute inset-0 flex items-center justify-center z-10">
                <span className="rounded bg-blue-500/20 px-3 py-1.5 text-xs text-blue-300 backdrop-blur-sm">
                  Drop to import
                </span>
              </div>
            )}
            {isEmpty ? (
              <div className="flex items-center justify-center h-full text-xs text-neutral-600">
                {assets.length === 0 ? "No assets loaded" : "No results"}
              </div>
            ) : viewMode === "grid" ? (
              <div className="grid grid-cols-8 gap-2">
                {/* Subdirectory entries */}
                {subdirs.map((dir) => {
                  const fullPath = currentPath ? `${currentPath}/${dir}` : dir
                  return (
                    <div
                      key={fullPath}
                      onDoubleClick={() => navigateTo(fullPath)}
                      title={fullPath}
                      className="flex flex-col items-center p-1 rounded cursor-pointer hover:bg-neutral-800"
                    >
                      <div className="w-12 h-12 flex items-center justify-center mb-1">
                        <Folder className="w-8 h-8 text-yellow-500" />
                      </div>
                      <span className="text-[10px] text-neutral-400 text-center truncate w-full">
                        {dir}
                      </span>
                    </div>
                  )
                })}
                {/* File entries */}
                {files.map((asset) => (
                  <div
                    key={asset.id}
                    draggable={asset.kind === "mesh" || asset.kind === "texture"}
                    onClick={() => setSelectedAsset(asset.id)}
                    onDoubleClick={() => void assignAssetToSelection(asset)}
                    onDragStart={(e) => {
                      e.dataTransfer.setData("axiom/asset-path", asset.path)
                      e.dataTransfer.setData("axiom/asset-kind", asset.kind)
                      e.dataTransfer.effectAllowed = "copy"
                    }}
                    title={
                      canAssignToSelection && asset.kind === "mesh"
                        ? "Double-click or drag to assign to a mesh object"
                        : canAssignToSelection && asset.kind === "texture"
                          ? "Double-click or drag to assign texture to a mesh object"
                          : asset.path
                    }
                    className={`flex flex-col items-center p-1 rounded cursor-pointer hover:bg-neutral-800 ${
                      selectedAsset === asset.id ? "bg-neutral-700 ring-1 ring-white/30" : ""
                    }`}
                  >
                    <div className="w-12 h-12 flex items-center justify-center mb-1">
                      <AssetIcon kind={asset.kind} path={asset.path} serverOrigin={serverOrigin} />
                    </div>
                    <span className="text-[10px] text-neutral-400 text-center truncate w-full">
                      {asset.name}
                    </span>
                  </div>
                ))}
              </div>
            ) : (
              <div className="flex flex-col gap-0.5">
                {/* Subdirectory entries */}
                {subdirs.map((dir) => {
                  const fullPath = currentPath ? `${currentPath}/${dir}` : dir
                  return (
                    <div
                      key={fullPath}
                      onDoubleClick={() => navigateTo(fullPath)}
                      title={fullPath}
                      className="flex items-center gap-2 px-2 py-1 rounded cursor-pointer hover:bg-neutral-800"
                    >
                      <Folder className="w-4 h-4 text-yellow-500 shrink-0" />
                      <div className="flex-1 min-w-0">
                        <div className="text-xs text-neutral-300 truncate">{dir}</div>
                      </div>
                      <span className="text-[10px] text-neutral-600 shrink-0">folder</span>
                    </div>
                  )
                })}
                {/* File entries */}
                {files.map((asset) => (
                  <div
                    key={asset.id}
                    draggable={asset.kind === "mesh" || asset.kind === "texture"}
                    onClick={() => setSelectedAsset(asset.id)}
                    onDoubleClick={() => void assignAssetToSelection(asset)}
                    onDragStart={(e) => {
                      e.dataTransfer.setData("axiom/asset-path", asset.path)
                      e.dataTransfer.setData("axiom/asset-kind", asset.kind)
                      e.dataTransfer.effectAllowed = "copy"
                    }}
                    title={
                      canAssignToSelection && asset.kind === "mesh"
                        ? "Double-click or drag to assign to a mesh object"
                        : canAssignToSelection && asset.kind === "texture"
                          ? "Double-click or drag to assign texture to a mesh object"
                          : asset.path
                    }
                    className={`flex items-center gap-2 px-2 py-1 rounded cursor-pointer hover:bg-neutral-800 ${
                      selectedAsset === asset.id ? "bg-neutral-700 ring-1 ring-white/30" : ""
                    }`}
                  >
                    <AssetIcon
                      kind={asset.kind}
                      path={asset.path}
                      serverOrigin={serverOrigin}
                      size="sm"
                    />
                    <div className="flex-1 min-w-0">
                      <div className="text-xs text-neutral-300 truncate">{asset.name}</div>
                      <div className="text-[10px] text-neutral-600 truncate">{asset.path}</div>
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
