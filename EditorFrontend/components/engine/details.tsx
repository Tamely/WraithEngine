"use client"

import { useState } from "react"
import { ChevronRight, ChevronDown, Settings, Lock, Unlock } from "lucide-react"

interface DetailsProps {
  selectedItem: string | null
}

interface PropertySection {
  name: string
  expanded: boolean
  properties: Property[]
}

interface Property {
  name: string
  value: string | number | boolean
  type: "text" | "number" | "vector" | "bool" | "color"
}

const mockDetails: Record<string, PropertySection[]> = {
  PlayerCharacter: [
    {
      name: "Transform",
      expanded: true,
      properties: [
        { name: "Location", value: "X: 0.0, Y: 0.0, Z: 50.0", type: "vector" },
        { name: "Rotation", value: "X: 0.0, Y: 0.0, Z: 0.0", type: "vector" },
        { name: "Scale", value: "X: 1.0, Y: 1.0, Z: 1.0", type: "vector" },
      ],
    },
    {
      name: "Character",
      expanded: true,
      properties: [
        { name: "Health", value: 100, type: "number" },
        { name: "Speed", value: 600, type: "number" },
        { name: "Jump Height", value: 420, type: "number" },
        { name: "Can Jump", value: true, type: "bool" },
      ],
    },
    {
      name: "Rendering",
      expanded: false,
      properties: [
        { name: "Visible", value: true, type: "bool" },
        { name: "Cast Shadow", value: true, type: "bool" },
      ],
    },
  ],
}

export function Details({ selectedItem }: DetailsProps) {
  const [sections, setSections] = useState<PropertySection[]>(
    selectedItem && mockDetails[selectedItem] ? mockDetails[selectedItem] : []
  )
  const [isLocked, setIsLocked] = useState(false)

  const toggleSection = (sectionName: string) => {
    setSections((prev) =>
      prev.map((section) =>
        section.name === sectionName ? { ...section, expanded: !section.expanded } : section
      )
    )
  }

  const displayedSections =
    selectedItem && mockDetails[selectedItem] ? mockDetails[selectedItem] : sections

  return (
    <div className="flex-1 flex flex-col overflow-hidden">
      <div className="flex items-center justify-between h-8 bg-neutral-950 border-b border-neutral-800 px-2">
        <div className="flex items-center gap-1">
          <Settings className="w-4 h-4 text-neutral-400" />
          <span className="text-xs font-medium text-neutral-300">Details</span>
        </div>
        <button
          onClick={() => setIsLocked(!isLocked)}
          className="p-1 hover:bg-neutral-800 rounded"
        >
          {isLocked ? (
            <Lock className="w-3.5 h-3.5 text-neutral-400" />
          ) : (
            <Unlock className="w-3.5 h-3.5 text-neutral-500" />
          )}
        </button>
      </div>
      <div className="flex-1 overflow-y-auto">
        {selectedItem ? (
          <>
            <div className="p-2 border-b border-neutral-800">
              <span className="text-xs text-neutral-400">Selected: </span>
              <span className="text-xs text-white font-medium">{selectedItem}</span>
            </div>
            {displayedSections.map((section) => (
              <div key={section.name} className="border-b border-neutral-800">
                <button
                  onClick={() => toggleSection(section.name)}
                  className="flex items-center gap-1 w-full px-2 py-1.5 hover:bg-neutral-800"
                >
                  {section.expanded ? (
                    <ChevronDown className="w-3 h-3 text-neutral-400" />
                  ) : (
                    <ChevronRight className="w-3 h-3 text-neutral-400" />
                  )}
                  <span className="text-xs font-medium text-neutral-300">{section.name}</span>
                </button>
                {section.expanded && (
                  <div className="pb-2">
                    {section.properties.map((prop) => (
                      <div
                        key={prop.name}
                        className="flex items-center gap-2 px-4 py-1 hover:bg-neutral-800/50"
                      >
                        <span className="text-xs text-neutral-500 w-24 truncate">{prop.name}</span>
                        <div className="flex-1">
                          {prop.type === "bool" ? (
                            <input
                              type="checkbox"
                              checked={prop.value as boolean}
                              readOnly
                              className="w-4 h-4 rounded bg-neutral-800 border-neutral-600"
                            />
                          ) : prop.type === "color" ? (
                            <div className="flex items-center gap-2">
                              <div
                                className="w-6 h-6 rounded border border-neutral-600"
                                style={{ backgroundColor: prop.value as string }}
                              />
                              <span className="text-xs text-neutral-300">{prop.value}</span>
                            </div>
                          ) : (
                            <input
                              type="text"
                              value={String(prop.value)}
                              readOnly
                              className="w-full px-2 py-0.5 text-xs bg-neutral-900 border border-neutral-700 rounded text-neutral-300 outline-none focus:border-neutral-500"
                            />
                          )}
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            ))}
          </>
        ) : (
          <div className="flex items-center justify-center h-full text-neutral-600 text-xs">
            Select an object to view details
          </div>
        )}
      </div>
    </div>
  )
}
