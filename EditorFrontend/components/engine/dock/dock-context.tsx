"use client"

import React, { createContext, useContext, useCallback, useState, useRef } from "react"

export type PanelId =
  | "viewport"
  | "outliner"
  | "details"
  | "content-browser"
  | "remote-viewport"
  | "script-editor"
  | "world-details"

export type DockZone = "left" | "right" | "top" | "bottom" | "center" | "tab"

export interface TabGroup {
    id: string
    panels: PanelId[]
    activePanel: PanelId
}

export interface DockCell {
    id: string
    type: "cell"
    tabGroup: TabGroup
    weight: number // flex weight (size proportion)
}

export interface DockRow {
    id: string
    type: "row" // horizontal split
    children: DockNode[]
    weight?: number
}

export interface DockColumn {
    id: string
    type: "column" // vertical split
    children: DockNode[]
    weight?: number
}

export type DockNode = DockCell | DockRow | DockColumn

export interface FloatingPanel {
    panelId: PanelId
    x: number
    y: number
    width: number
    height: number
}

export interface DockState {
    root: DockNode
    floatingPanels: FloatingPanel[]
}

interface DragState {
    panelId: PanelId | null
    sourceTabGroupId: string | null
    isFloating: boolean
    previewX: number
    previewY: number
}

interface DockContextValue {
    layout: DockState
    dragState: DragState
    setDragState: React.Dispatch<React.SetStateAction<DragState>>
    dropPanel: (panelId: PanelId, targetTabGroupId: string, zone: DockZone) => void
    floatPanel: (panelId: PanelId, x: number, y: number) => void
    dockFloatingPanel: (panelId: PanelId, targetTabGroupId: string, zone: DockZone) => void
    setActiveTab: (tabGroupId: string, panelId: PanelId) => void
    updateWeight: (nodeId: string, weight: number) => void
    floatingDragRef: React.MutableRefObject<{ panelId: PanelId; offsetX: number; offsetY: number } | null>
    setFloatingPosition: (panelId: PanelId, x: number, y: number) => void
}

const DockContext = createContext<DockContextValue | null>(null)

const defaultDragState: DragState = {
    panelId: null,
    sourceTabGroupId: null,
    isFloating: false,
    previewX: 0,
    previewY: 0,
}

export function useDock() {
    const ctx = useContext(DockContext)
    if (!ctx) throw new Error("useDock must be used within DockProvider")
    return ctx
}

// ─── helpers ──────────────────────────────────────────────────────────────────

function uid() {
    return Math.random().toString(36).slice(2, 9)
}

function removePanel(node: DockNode, panelId: PanelId): { node: DockNode | null; removed: boolean } {
    if (node.type === "cell") {
        const panels = node.tabGroup.panels.filter((p) => p !== panelId)
        if (panels.length === node.tabGroup.panels.length) return { node, removed: false }
        if (panels.length === 0) return { node: null, removed: true }
        return {
            node: {
                ...node,
                tabGroup: {
                    ...node.tabGroup,
                    panels,
                    activePanel: panels.includes(node.tabGroup.activePanel) ? node.tabGroup.activePanel : panels[0],
                },
            },
            removed: true,
        }
    }
    const children: DockNode[] = []
    let removed = false
    for (const child of node.children) {
        const result = removePanel(child, panelId)
        if (result.removed) removed = true
        if (result.node) children.push(result.node)
    }
    if (!removed) return { node, removed: false }
    if (children.length === 0) return { node: null, removed: true }
    if (children.length === 1) {
        // unwrap single-child containers
        return { node: children[0], removed: true }
    }
    return { node: { ...node, children } as DockRow | DockColumn, removed: true }
}

function insertPanel(
    node: DockNode,
    targetTabGroupId: string,
    panelId: PanelId,
    zone: DockZone
): DockNode {
    if (node.type === "cell") {
        if (node.tabGroup.id !== targetTabGroupId) return node

        if (zone === "tab" || zone === "center") {
            // Add as a tab in the same group
            return {
                ...node,
                tabGroup: {
                    ...node.tabGroup,
                    panels: [...node.tabGroup.panels, panelId],
                    activePanel: panelId,
                },
            }
        }

        // Split: create a new cell for the panel
        const newCell: DockCell = {
            id: uid(),
            type: "cell",
            weight: 1,
            tabGroup: { id: uid(), panels: [panelId], activePanel: panelId },
        }

        if (zone === "left" || zone === "right") {
            const row: DockRow = {
                id: uid(),
                type: "row",
                children: zone === "left" ? [newCell, { ...node, weight: 3 }] : [{ ...node, weight: 3 }, newCell],
            }
            return row
        } else {
            // top / bottom
            const col: DockColumn = {
                id: uid(),
                type: "column",
                children: zone === "top" ? [newCell, { ...node, weight: 3 }] : [{ ...node, weight: 3 }, newCell],
            }
            return col
        }
    }

    return {
        ...node,
        children: node.children.map((child) => insertPanel(child, targetTabGroupId, panelId, zone)),
    } as DockRow | DockColumn
}

function updateNodeWeight(node: DockNode, nodeId: string, weight: number): DockNode {
    if (node.id === nodeId) return { ...node, weight } as DockNode
    if (node.type === "cell") return node
    return {
        ...node,
        children: node.children.map((c) => updateNodeWeight(c, nodeId, weight)),
    } as DockRow | DockColumn
}

function setActiveTabInTree(node: DockNode, tabGroupId: string, panelId: PanelId): DockNode {
    if (node.type === "cell") {
        if (node.tabGroup.id !== tabGroupId) return node
        return { ...node, tabGroup: { ...node.tabGroup, activePanel: panelId } }
    }
    return {
        ...node,
        children: node.children.map((c) => setActiveTabInTree(c, tabGroupId, panelId)),
    } as DockRow | DockColumn
}

// ─── initial layout ────────────────────────────────────────────────────────────

const initialLayout: DockState = {
    root: {
        id: "root-row",
        type: "row",
        children: [
            {
                id: "left-col",
                type: "column",
                weight: 4,
                children: [
                    {
                        id: "viewport-cell",
                        type: "cell",
                        weight: 3,
                        tabGroup: {
                            id: "tg-viewport",
                            panels: ["viewport", "script-editor"],
                            activePanel: "viewport",
                        },
                    },
                    {
                        id: "content-cell",
                        type: "cell",
                        weight: 1,
                        tabGroup: {
                            id: "tg-content",
                            panels: ["content-browser", "remote-viewport"],
                            activePanel: "content-browser",
                        },
                    },
                ],
            } as DockColumn,
            {
                id: "right-col",
                type: "column",
                weight: 1,
                children: [
                    {
                        id: "outliner-cell",
                        type: "cell",
                        weight: 1,
                        tabGroup: { id: "tg-outliner", panels: ["outliner"], activePanel: "outliner" },
                    },
                    {
                        id: "details-cell",
                        type: "cell",
                        weight: 1,
                        tabGroup: { id: "tg-details", panels: ["details", "world-details"], activePanel: "details" },
                    },
                ],
            } as DockColumn,
        ],
    } as DockRow,
    floatingPanels: [],
}

// ─── provider ─────────────────────────────────────────────────────────────────

export function DockProvider({ children }: { children: React.ReactNode }) {
    const [layout, setLayout] = useState<DockState>(initialLayout)
    const [dragState, setDragState] = useState<DragState>(defaultDragState)
    const floatingDragRef = useRef<{ panelId: PanelId; offsetX: number; offsetY: number } | null>(null)

    const dropPanel = useCallback((panelId: PanelId, targetTabGroupId: string, zone: DockZone) => {
        setLayout((prev) => {
            // Remove from current location
            const { node: afterRemove } = removePanel(prev.root, panelId)
            if (!afterRemove) return prev
            // Insert at target
            const afterInsert = insertPanel(afterRemove, targetTabGroupId, panelId, zone)
            return { ...prev, root: afterInsert }
        })
        setDragState(defaultDragState)
    }, [])

    const floatPanel = useCallback((panelId: PanelId, x: number, y: number) => {
        setLayout((prev) => {
            const { node: afterRemove } = removePanel(prev.root, panelId)
            if (!afterRemove) return prev
            const alreadyFloating = prev.floatingPanels.find((f) => f.panelId === panelId)
            return {
                root: afterRemove,
                floatingPanels: alreadyFloating
                    ? prev.floatingPanels
                    : [...prev.floatingPanels, { panelId, x, y, width: 340, height: 420 }],
            }
        })
        setDragState(defaultDragState)
    }, [])

    const dockFloatingPanel = useCallback((panelId: PanelId, targetTabGroupId: string, zone: DockZone) => {
        setLayout((prev) => {
            const floatingPanels = prev.floatingPanels.filter((f) => f.panelId !== panelId)
            const afterInsert = insertPanel(prev.root, targetTabGroupId, panelId, zone)
            return { root: afterInsert, floatingPanels }
        })
        setDragState(defaultDragState)
    }, [])

    const setActiveTab = useCallback((tabGroupId: string, panelId: PanelId) => {
        setLayout((prev) => ({
            ...prev,
            root: setActiveTabInTree(prev.root, tabGroupId, panelId),
        }))
    }, [])

    const updateWeight = useCallback((nodeId: string, weight: number) => {
        setLayout((prev) => ({ ...prev, root: updateNodeWeight(prev.root, nodeId, weight) }))
    }, [])

    const setFloatingPosition = useCallback((panelId: PanelId, x: number, y: number) => {
        setLayout((prev) => ({
            ...prev,
            floatingPanels: prev.floatingPanels.map((f) => (f.panelId === panelId ? { ...f, x, y } : f)),
        }))
    }, [])

    return (
        <DockContext.Provider
            value={{
                layout,
                dragState,
                setDragState,
                dropPanel,
                floatPanel,
                dockFloatingPanel,
                setActiveTab,
                updateWeight,
                floatingDragRef,
                setFloatingPosition,
            }}
        >
            {children}
        </DockContext.Provider>
    )
}
