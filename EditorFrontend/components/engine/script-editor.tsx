"use client"

import { useCallback, useEffect, useMemo, useRef, useState, type ReactNode } from "react"
import {
  FileCode2,
  Folder,
  ChevronDown,
  ChevronRight,
  FilePlus2,
  Pencil,
  RefreshCw,
  Save,
  Trash2,
} from "lucide-react"
import { useProjectSession } from "./project-session-context"
import { useRemoteViewport } from "./remote-viewport-context"

interface ScriptListResponse {
  type: "scripts_list"
  files: string[]
}

interface ScriptFileResponse {
  type: "script_file"
  path: string
  content: string
}

interface ScriptMutationResponse {
  type: string
  path: string
}

interface ScriptTreeNode {
  name: string
  path: string
  children: ScriptTreeNode[]
  files: string[]
}

function buildScriptTree(files: string[]): ScriptTreeNode {
  const root: ScriptTreeNode = { name: "Scripts", path: "", children: [], files: [] }
  const nodes = new Map<string, ScriptTreeNode>([["", root]])

  for (const file of files) {
    const parts = file.split("/")
    if (parts.length === 1) {
      root.files.push(file)
      continue
    }

    for (let depth = 1; depth < parts.length; depth++) {
      const dirPath = parts.slice(0, depth).join("/")
      if (!nodes.has(dirPath)) {
        const parentPath = parts.slice(0, depth - 1).join("/")
        const node: ScriptTreeNode = {
          name: parts[depth - 1],
          path: dirPath,
          children: [],
          files: [],
        }
        nodes.set(dirPath, node)
        nodes.get(parentPath)!.children.push(node)
      }
    }

    const dirPath = parts.slice(0, -1).join("/")
    nodes.get(dirPath)!.files.push(file)
  }

  const sortNode = (node: ScriptTreeNode) => {
    node.children.sort((left, right) => left.name.localeCompare(right.name))
    node.files.sort((left, right) => left.localeCompare(right))
    node.children.forEach(sortNode)
  }
  sortNode(root)

  return root
}

type TokenStyle = "plain" | "comment" | "string" | "keyword" | "engine" | "number"

interface ScriptToken {
  text: string
  style: TokenStyle
}

const KEYWORDS = new Set([
  "namespace",
  "public",
  "class",
  "using",
  "return",
  "if",
  "else",
  "new",
  "override",
  "void",
  "float",
  "int",
  "bool",
  "string",
  "private",
  "protected",
  "internal",
  "static",
  "async",
  "await",
  "var",
])

const ENGINE_SYMBOLS = new Set([
  "OnCreate",
  "OnTick",
  "OnDestroy",
  "Script",
  "GameObject",
  "Transform",
  "Vector3",
])

function isIdentifierStart(char: string) {
  return /[A-Za-z_]/.test(char)
}

function isIdentifierPart(char: string) {
  return /[A-Za-z0-9_]/.test(char)
}

function isNumberStart(source: string, index: number) {
  const current = source[index]
  const next = source[index + 1] ?? ""
  if (!/\d/.test(current)) {
    return false
  }
  if (current !== ".") {
    return true
  }
  return /\d/.test(next)
}

function tokenizeCSharp(source: string): ScriptToken[] {
  const tokens: ScriptToken[] = []
  let index = 0

  while (index < source.length) {
    const current = source[index]
    const next = source[index + 1] ?? ""

    if (current === "/" && next === "/") {
      let end = index + 2
      while (end < source.length && source[end] !== "\n") {
        end += 1
      }
      tokens.push({ text: source.slice(index, end), style: "comment" })
      index = end
      continue
    }

    if (current === "/" && next === "*") {
      let end = index + 2
      while (end < source.length - 1 && !(source[end] === "*" && source[end + 1] === "/")) {
        end += 1
      }
      end = Math.min(source.length, end + 2)
      tokens.push({ text: source.slice(index, end), style: "comment" })
      index = end
      continue
    }

    if (current === "\"") {
      let end = index + 1
      while (end < source.length) {
        if (source[end] === "\\" && end + 1 < source.length) {
          end += 2
          continue
        }
        if (source[end] === "\"") {
          end += 1
          break
        }
        end += 1
      }
      tokens.push({ text: source.slice(index, end), style: "string" })
      index = end
      continue
    }

    if (isIdentifierStart(current)) {
      let end = index + 1
      while (end < source.length && isIdentifierPart(source[end])) {
        end += 1
      }
      const word = source.slice(index, end)
      let style: TokenStyle = "plain"
      if (KEYWORDS.has(word)) {
        style = "keyword"
      } else if (ENGINE_SYMBOLS.has(word)) {
        style = "engine"
      }
      tokens.push({ text: word, style })
      index = end
      continue
    }

    if (isNumberStart(source, index)) {
      let end = index + 1
      while (end < source.length && /[\d._fFmMdD]/.test(source[end])) {
        end += 1
      }
      tokens.push({ text: source.slice(index, end), style: "number" })
      index = end
      continue
    }

    tokens.push({ text: current, style: "plain" })
    index += 1
  }

  return tokens
}

function tokenClassName(style: TokenStyle) {
  switch (style) {
    case "comment":
      return "text-emerald-400/80"
    case "string":
      return "text-amber-300"
    case "keyword":
      return "text-sky-300"
    case "engine":
      return "text-fuchsia-300"
    case "number":
      return "text-cyan-300"
    default:
      return undefined
  }
}

function renderHighlightedCode(source: string): ReactNode[] {
  return tokenizeCSharp(source).map((token, index) => {
    const className = tokenClassName(token.style)
    if (!className) {
      return <span key={index}>{token.text}</span>
    }
    return (
      <span key={index} className={className}>
        {token.text}
      </span>
    )
  })
}

export function ScriptEditor() {
  const {
    activeProject,
    serverOrigin,
    requestedScriptPath,
    clearRequestedScriptPath,
  } = useProjectSession()
  const { reloadScripts, reloadStatus } = useRemoteViewport()
  const [files, setFiles] = useState<string[]>([])
  const [selectedPath, setSelectedPath] = useState<string | null>(null)
  const [loadedPath, setLoadedPath] = useState<string | null>(null)
  const [content, setContent] = useState("")
  const [savedContent, setSavedContent] = useState("")
  const [loading, setLoading] = useState(true)
  const [saving, setSaving] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [expandedPaths, setExpandedPaths] = useState<Set<string>>(new Set([""]))
  const editorRef = useRef<HTMLTextAreaElement | null>(null)
  const highlightRef = useRef<HTMLPreElement | null>(null)

  const isDirty = content !== savedContent
  const tree = useMemo(() => buildScriptTree(files), [files])

  const fetchJson = useCallback(
    async <T,>(path: string, init?: RequestInit) => {
      const response = await fetch(`${serverOrigin}${path}`, {
        cache: "no-store",
        ...init,
      })
      const text = await response.text()
      const payload = text.length > 0 ? (JSON.parse(text) as T & { message?: string }) : null
      if (!response.ok) {
        throw new Error(payload?.message ?? `${response.status} ${response.statusText}`)
      }
      return payload as T
    },
    [serverOrigin]
  )

  const refreshScripts = useCallback(async () => {
    setLoading(true)
    setError(null)
    try {
      const payload = await fetchJson<ScriptListResponse>("/scripts")
      setFiles(payload.files)
      if (payload.files.length === 0) {
        setSelectedPath(null)
        setLoadedPath(null)
        setContent("")
        setSavedContent("")
      } else if (selectedPath === null || !payload.files.includes(selectedPath)) {
        setSelectedPath((current) =>
          current && payload.files.includes(current) ? current : payload.files[0]
        )
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err))
    } finally {
      setLoading(false)
    }
  }, [fetchJson, selectedPath])

  const loadFile = useCallback(
    async (path: string) => {
      setError(null)
      try {
        const payload = await fetchJson<ScriptFileResponse>(
          `/scripts/file?path=${encodeURIComponent(path)}`
        )
        setSelectedPath(payload.path)
        setLoadedPath(payload.path)
        setContent(payload.content)
        setSavedContent(payload.content)
      } catch (err) {
        setError(err instanceof Error ? err.message : String(err))
      }
    },
    [fetchJson]
  )

  useEffect(() => {
    void refreshScripts()
  }, [refreshScripts, activeProject.slug])

  useEffect(() => {
    if (!selectedPath) {
      return
    }
    if (selectedPath === loadedPath) {
      return
    }
    void loadFile(selectedPath)
  }, [selectedPath, loadedPath, loadFile])

  useEffect(() => {
    const handleBeforeUnload = (event: BeforeUnloadEvent) => {
      if (!isDirty) return
      event.preventDefault()
      event.returnValue = ""
    }
    window.addEventListener("beforeunload", handleBeforeUnload)
    return () => window.removeEventListener("beforeunload", handleBeforeUnload)
  }, [isDirty])

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (!(event.metaKey || event.ctrlKey) || event.key.toLowerCase() !== "s") {
        return
      }
      event.preventDefault()
      void handleSave()
    }
    window.addEventListener("keydown", handleKeyDown)
    return () => window.removeEventListener("keydown", handleKeyDown)
  })

  const ensureCanDiscard = useCallback(() => {
    if (!isDirty) {
      return true
    }
    return window.confirm("Discard unsaved script changes?")
  }, [isDirty])

  useEffect(() => {
    if (!requestedScriptPath) {
      return
    }
    if (requestedScriptPath === selectedPath) {
      clearRequestedScriptPath()
      return
    }
    if (!ensureCanDiscard()) {
      clearRequestedScriptPath()
      return
    }
    setSelectedPath(requestedScriptPath)
    clearRequestedScriptPath()
  }, [
    clearRequestedScriptPath,
    ensureCanDiscard,
    requestedScriptPath,
    selectedPath,
  ])

  const chooseFile = useCallback(
    (path: string) => {
      if (path === selectedPath) return
      if (!ensureCanDiscard()) return
      setSelectedPath(path)
    },
    [ensureCanDiscard, selectedPath]
  )

  async function handleSave() {
    if (!selectedPath) return
    setSaving(true)
    setError(null)
    try {
      await fetchJson<ScriptMutationResponse>("/scripts/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: selectedPath, content }),
      })
      setSavedContent(content)
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err))
    } finally {
      setSaving(false)
    }
  }

  async function handleCreate() {
    if (!ensureCanDiscard()) return
    const suggestedBase =
      activeProject.starterScriptClassName && files.length === 0
        ? activeProject.starterScriptClassName
        : "NewScript"
    const nextPath = window.prompt("New script path", `${suggestedBase}.cs`)
    if (!nextPath) return
    setError(null)
    try {
      const payload = await fetchJson<ScriptMutationResponse>("/scripts/create", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: nextPath }),
      })
      setExpandedPaths((current) => {
        const next = new Set(current)
        const path = payload.path.split("/")
        for (let index = 1; index < path.length; index++) {
          next.add(path.slice(0, index).join("/"))
        }
        return next
      })
      await refreshScripts()
      await loadFile(payload.path)
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err))
    }
  }

  async function handleRename() {
    if (!selectedPath) return
    if (!ensureCanDiscard()) return
    const nextPath = window.prompt("Rename script", selectedPath)
    if (!nextPath || nextPath === selectedPath) return
    setError(null)
    try {
      const payload = await fetchJson<ScriptMutationResponse>("/scripts/rename", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: selectedPath, newPath: nextPath }),
      })
      await refreshScripts()
      setSelectedPath(payload.path)
      setLoadedPath(null)
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err))
    }
  }

  async function handleDelete() {
    if (!selectedPath) return
    if (!ensureCanDiscard()) return
    if (!window.confirm(`Delete ${selectedPath}?`)) return
    setError(null)
    try {
      await fetchJson<ScriptMutationResponse>("/scripts/delete", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: selectedPath }),
      })
      setSelectedPath(null)
      setLoadedPath(null)
      setContent("")
      setSavedContent("")
      await refreshScripts()
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err))
    }
  }

  const renderNode = (node: ScriptTreeNode, depth = 0): React.ReactNode => {
    const isRoot = node.path === ""
    const isExpanded = expandedPaths.has(node.path)

    return (
      <div key={node.path || "root"}>
        {!isRoot ? (
          <button
            className="flex w-full items-center gap-1 rounded px-2 py-1 text-left text-xs text-neutral-300 hover:bg-neutral-800"
            style={{ paddingLeft: `${depth * 12 + 8}px` }}
            onClick={() =>
              setExpandedPaths((current) => {
                const next = new Set(current)
                if (next.has(node.path)) {
                  next.delete(node.path)
                } else {
                  next.add(node.path)
                }
                return next
              })
            }
            type="button"
          >
            {isExpanded ? (
              <ChevronDown className="h-3 w-3 text-neutral-500" />
            ) : (
              <ChevronRight className="h-3 w-3 text-neutral-500" />
            )}
            <Folder className="h-3.5 w-3.5 text-amber-300" />
            <span>{node.name}</span>
          </button>
        ) : null}

        {(isRoot || isExpanded) ? (
          <div>
            {node.children.map((child) => renderNode(child, depth + (isRoot ? 0 : 1)))}
            {node.files.map((file) => {
              const fileName = file.split("/").at(-1) ?? file
              const isSelected = selectedPath === file
              return (
                <button
                  key={file}
                  className={`flex w-full items-center gap-2 rounded px-2 py-1 text-left text-xs transition-colors ${
                    isSelected
                      ? "bg-sky-400/15 text-white"
                      : "text-neutral-300 hover:bg-neutral-800"
                  }`}
                  style={{ paddingLeft: `${(depth + 1) * 12 + 16}px` }}
                  onClick={() => chooseFile(file)}
                  type="button"
                >
                  <FileCode2 className="h-3.5 w-3.5 text-sky-300" />
                  <span className="truncate">{fileName}</span>
                  {loadedPath === file && isDirty ? (
                    <span className="ml-auto text-[10px] uppercase tracking-[0.18em] text-amber-300">
                      Unsaved
                    </span>
                  ) : null}
                </button>
              )
            })}
          </div>
        ) : null}
      </div>
    )
  }

  const highlightedContent = useMemo(() => renderHighlightedCode(content), [content])

  return (
    <div className="flex h-full min-h-0 bg-neutral-950">
      <aside className="flex w-64 shrink-0 flex-col border-r border-neutral-800">
        <div className="flex h-9 items-center justify-between border-b border-neutral-800 px-3">
          <div>
            <p className="text-[10px] uppercase tracking-[0.2em] text-neutral-500">Scripts</p>
            <p className="text-xs text-neutral-300">{activeProject.scriptAssemblyName}</p>
          </div>
          <button
            className="rounded p-1 text-neutral-400 hover:bg-neutral-800 hover:text-white"
            onClick={() => void refreshScripts()}
            type="button"
          >
            <RefreshCw className={`h-3.5 w-3.5 ${loading ? "animate-spin" : ""}`} />
          </button>
        </div>
        <div className="flex gap-1 border-b border-neutral-800 px-2 py-2">
          <button
            className="inline-flex items-center gap-1 rounded bg-neutral-900 px-2 py-1 text-[11px] text-neutral-300 hover:bg-neutral-800"
            onClick={() => void handleCreate()}
            type="button"
          >
            <FilePlus2 className="h-3.5 w-3.5" />
            New
          </button>
          <button
            className="inline-flex items-center gap-1 rounded px-2 py-1 text-[11px] text-neutral-400 hover:bg-neutral-800 hover:text-white disabled:opacity-40"
            disabled={!selectedPath}
            onClick={() => void handleRename()}
            type="button"
          >
            <Pencil className="h-3.5 w-3.5" />
            Rename
          </button>
          <button
            className="inline-flex items-center gap-1 rounded px-2 py-1 text-[11px] text-red-300 hover:bg-red-500/10 disabled:opacity-40"
            disabled={!selectedPath}
            onClick={() => void handleDelete()}
            type="button"
          >
            <Trash2 className="h-3.5 w-3.5" />
            Delete
          </button>
        </div>
        <div className="min-h-0 flex-1 overflow-y-auto px-2 py-2">
          {files.length === 0 && !loading ? (
            <div className="rounded-lg border border-dashed border-neutral-800 px-3 py-4 text-xs text-neutral-500">
              No C# files yet. Create one to start scripting this project.
            </div>
          ) : (
            renderNode(tree)
          )}
        </div>
      </aside>

      <section className="flex min-w-0 flex-1 flex-col">
        <div className="flex h-9 items-center justify-between border-b border-neutral-800 px-3">
          <div className="min-w-0">
            <p className="truncate text-xs text-neutral-200">
              {selectedPath ?? "Select a script file"}
            </p>
            <p className="truncate text-[10px] uppercase tracking-[0.18em] text-neutral-500">
              {activeProject.scriptRootNamespace}
            </p>
          </div>
          <div className="flex items-center gap-2">
            <button
              className="inline-flex items-center gap-1 rounded px-2 py-1 text-[11px] text-neutral-300 hover:bg-neutral-800"
              onClick={() => void reloadScripts()}
              type="button"
            >
              <RefreshCw
                className={`h-3.5 w-3.5 ${reloadStatus === "reloading" ? "animate-spin" : ""}`}
              />
              Reload
            </button>
            <button
              className="inline-flex items-center gap-1 rounded bg-sky-300 px-2 py-1 text-[11px] font-medium text-black disabled:opacity-50"
              disabled={!selectedPath || saving || !isDirty}
              onClick={() => void handleSave()}
              type="button"
            >
              <Save className="h-3.5 w-3.5" />
              {saving ? "Saving..." : "Save"}
            </button>
          </div>
        </div>

        {error ? (
          <div className="border-b border-red-500/20 bg-red-500/10 px-3 py-2 text-xs text-red-200">
            {error}
          </div>
        ) : null}

        <div className="relative min-h-0 flex-1 bg-[#05070a]">
          {selectedPath ? (
            <>
              <pre
                ref={highlightRef}
                aria-hidden="true"
                className="pointer-events-none absolute inset-0 overflow-auto whitespace-pre-wrap break-words px-4 py-3 font-mono text-[13px] leading-6 text-neutral-200"
              >
                {highlightedContent}
                {"\n"}
              </pre>
              <textarea
                ref={editorRef}
                className="absolute inset-0 resize-none overflow-auto bg-transparent px-4 py-3 font-mono text-[13px] leading-6 text-transparent caret-white outline-none selection:bg-sky-400/30"
                spellCheck={false}
                value={content}
                onChange={(event) => setContent(event.target.value)}
                onKeyDown={(event) => {
                  if (event.key === "Tab") {
                    event.preventDefault()
                    const target = event.currentTarget
                    const start = target.selectionStart
                    const end = target.selectionEnd
                    const next = `${content.slice(0, start)}    ${content.slice(end)}`
                    setContent(next)
                    requestAnimationFrame(() => {
                      target.selectionStart = start + 4
                      target.selectionEnd = start + 4
                    })
                  }
                }}
                onScroll={(event) => {
                  if (!highlightRef.current) return
                  highlightRef.current.scrollTop = event.currentTarget.scrollTop
                  highlightRef.current.scrollLeft = event.currentTarget.scrollLeft
                }}
              />
            </>
          ) : (
            <div className="flex h-full items-center justify-center px-6 text-center text-sm text-neutral-500">
              Choose a script file from the project tree to edit it here.
            </div>
          )}
        </div>
      </section>
    </div>
  )
}
