"use client"

import { useState, useMemo } from "react"
import {
  Box,
  Lightbulb,
  Camera,
  User,
  Folder,
  Search,
  Layers,
} from "lucide-react"
import { useRemoteViewport } from "@/components/engine/remote-viewport-context"

type CategoryId = "all" | "geometry" | "lights" | "cameras" | "actors" | "utility"

interface PlaceableItem {
  id: string
  label: string
  description: string
  templateId: string
  categoryId: CategoryId
  Icon: React.ComponentType<{ className?: string }>
}

const ITEMS: PlaceableItem[] = [
  {
    id: "mesh",
    label: "Mesh",
    description: "Empty mesh object",
    templateId: "Mesh",
    categoryId: "geometry",
    Icon: Box,
  },
  {
    id: "light",
    label: "Light",
    description: "Scene light source",
    templateId: "Light",
    categoryId: "lights",
    Icon: Lightbulb,
  },
  {
    id: "camera",
    label: "Camera",
    description: "Camera actor",
    templateId: "Camera",
    categoryId: "cameras",
    Icon: Camera,
  },
  {
    id: "actor",
    label: "Actor",
    description: "Scriptable game object",
    templateId: "Actor",
    categoryId: "actors",
    Icon: User,
  },
  {
    id: "folder",
    label: "Folder",
    description: "Organizer / group",
    templateId: "Folder",
    categoryId: "utility",
    Icon: Folder,
  },
]

interface Category {
  id: CategoryId
  label: string
  Icon: React.ComponentType<{ className?: string }>
}

const CATEGORIES: Category[] = [
  { id: "all", label: "All", Icon: Layers },
  { id: "geometry", label: "Geometry", Icon: Box },
  { id: "lights", label: "Lights", Icon: Lightbulb },
  { id: "cameras", label: "Cameras", Icon: Camera },
  { id: "actors", label: "Actors", Icon: User },
  { id: "utility", label: "Utility", Icon: Folder },
]

export function PlaceActorsPanel() {
  const { placeActor } = useRemoteViewport()
  const [search, setSearch] = useState("")
  const [activeCategory, setActiveCategory] = useState<CategoryId>("all")
  const [placingId, setPlacingId] = useState<string | null>(null)

  const filtered = useMemo(() => {
    const q = search.toLowerCase().trim()
    return ITEMS.filter((item) => {
      const matchesCategory =
        activeCategory === "all" || item.categoryId === activeCategory
      const matchesSearch =
        !q || item.label.toLowerCase().includes(q) || item.description.toLowerCase().includes(q)
      return matchesCategory && matchesSearch
    })
  }, [search, activeCategory])

  async function handlePlace(item: PlaceableItem) {
    if (placingId) return
    setPlacingId(item.id)
    try {
      await placeActor(item.templateId, -1, -1)
    } finally {
      setPlacingId(null)
    }
  }

  function handleDragStart(event: React.DragEvent, item: PlaceableItem) {
    event.dataTransfer.setData("application/x-place-actor", item.templateId)
    event.dataTransfer.effectAllowed = "copy"
  }

  return (
    <div className="flex h-full flex-col overflow-hidden bg-neutral-950">
      {/* Header */}
      <div className="flex h-8 shrink-0 items-center gap-1.5 border-b border-neutral-800 px-2">
        <Layers className="h-4 w-4 text-neutral-400" />
        <span className="text-xs font-medium text-neutral-300">Place Actors</span>
      </div>

      {/* Search */}
      <div className="shrink-0 border-b border-neutral-800 p-2">
        <div className="flex items-center gap-1.5 rounded border border-neutral-800 bg-neutral-900 px-2 py-1 focus-within:border-neutral-600">
          <Search className="h-3 w-3 shrink-0 text-neutral-500" />
          <input
            type="text"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            placeholder="Search..."
            className="min-w-0 flex-1 bg-transparent text-xs text-neutral-300 placeholder-neutral-600 outline-none"
          />
        </div>
      </div>

      {/* Category tabs */}
      <div className="shrink-0 overflow-x-auto border-b border-neutral-800">
        <div className="flex min-w-max">
          {CATEGORIES.map((cat) => {
            const active = activeCategory === cat.id
            return (
              <button
                key={cat.id}
                onClick={() => setActiveCategory(cat.id)}
                className={`flex items-center gap-1 whitespace-nowrap px-3 py-1.5 text-xs transition-colors ${
                  active
                    ? "border-b border-neutral-300 text-neutral-200"
                    : "text-neutral-500 hover:text-neutral-300"
                }`}
              >
                <cat.Icon className="h-3 w-3" />
                {cat.label}
              </button>
            )
          })}
        </div>
      </div>

      {/* Item list */}
      <div className="flex-1 overflow-y-auto">
        {filtered.length === 0 ? (
          <div className="flex h-full items-center justify-center text-xs text-neutral-600">
            No results
          </div>
        ) : (
          <div className="p-1">
            {filtered.map((item) => {
              const isPlacing = placingId === item.id
              return (
                <button
                  key={item.id}
                  draggable
                  onDragStart={(e) => handleDragStart(e, item)}
                  onClick={() => void handlePlace(item)}
                  disabled={!!placingId}
                  className="group flex w-full items-center gap-3 rounded px-2 py-2 text-left transition-colors hover:bg-neutral-800 disabled:pointer-events-none"
                >
                  <div className="flex h-8 w-8 shrink-0 items-center justify-center rounded border border-neutral-800 bg-neutral-900 group-hover:border-neutral-700">
                    <item.Icon className="h-4 w-4 text-neutral-400 group-hover:text-neutral-300" />
                  </div>
                  <div className="min-w-0 flex-1">
                    <div className="truncate text-xs font-medium text-neutral-300 group-hover:text-neutral-100">
                      {isPlacing ? "Placing…" : item.label}
                    </div>
                    <div className="truncate text-[10px] text-neutral-600 group-hover:text-neutral-500">
                      {item.description}
                    </div>
                  </div>
                </button>
              )
            })}
          </div>
        )}
      </div>
    </div>
  )
}
