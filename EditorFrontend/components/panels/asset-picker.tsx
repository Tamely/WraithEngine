"use client"

import { useState } from "react"
import { FolderOpen } from "lucide-react"
import {
  Command,
  CommandEmpty,
  CommandInput,
  CommandItem,
  CommandList,
} from "@/components/ui/command"
import { Popover, PopoverContent, PopoverTrigger } from "@/components/ui/popover"

export interface AssetPickerItem {
  /** Stable identifier used as React key and CommandItem value. */
  key: string
  /** Bold-rendered display label (file name, class name, etc.). */
  label: string
  /** Optional muted line under the label (typically the asset path). */
  sublabel?: string
  /** Value passed back to onSelect when this row is chosen. */
  selectValue: string
}

interface AssetPickerButtonProps {
  items: AssetPickerItem[]
  onSelect: (value: string) => void
  onOpen?: () => void
  /** Tooltip + aria-label for the trigger button. */
  triggerLabel?: string
  emptyMessage?: string
  searchPlaceholder?: string
  /** Additional classes for the trigger button (positioning, etc.). */
  className?: string
}

export function AssetPickerButton({
  items,
  onSelect,
  onOpen,
  triggerLabel = "Browse",
  emptyMessage = "No matching assets",
  searchPlaceholder = "Search...",
  className,
}: AssetPickerButtonProps) {
  const [open, setOpen] = useState(false)

  return (
    <Popover
      open={open}
      onOpenChange={(next) => {
        setOpen(next)
        if (next) onOpen?.()
      }}
    >
      <PopoverTrigger asChild>
        <button
          type="button"
          aria-label={triggerLabel}
          title={triggerLabel}
          className={
            className ??
            "inline-flex h-5 w-5 items-center justify-center rounded-sm text-neutral-400 transition-colors hover:bg-neutral-800 hover:text-neutral-200"
          }
        >
          <FolderOpen className="h-3.5 w-3.5" />
        </button>
      </PopoverTrigger>
      <PopoverContent
        align="end"
        side="bottom"
        sideOffset={4}
        className="dark w-72 border-neutral-800 bg-neutral-950 p-0 text-neutral-200"
      >
        <Command className="bg-neutral-950 text-neutral-200">
          <CommandInput
            placeholder={searchPlaceholder}
            className="h-8 text-xs placeholder:text-neutral-500"
          />
          <CommandList className="max-h-64">
            <CommandEmpty className="px-3 py-4 text-center text-xs text-neutral-500">
              {emptyMessage}
            </CommandEmpty>
            {items.map((item) => (
              <CommandItem
                key={item.key}
                value={`${item.label} ${item.sublabel ?? ""} ${item.selectValue}`}
                onSelect={() => {
                  onSelect(item.selectValue)
                  setOpen(false)
                }}
                className="flex flex-col items-start gap-0.5 px-2 py-1.5 text-xs data-[selected=true]:bg-neutral-800 data-[selected=true]:text-neutral-100"
              >
                <span className="font-medium text-neutral-200">{item.label}</span>
                {item.sublabel ? (
                  <span className="truncate text-[10px] text-neutral-500">
                    {item.sublabel}
                  </span>
                ) : null}
              </CommandItem>
            ))}
          </CommandList>
        </Command>
      </PopoverContent>
    </Popover>
  )
}
