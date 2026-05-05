"use client"

interface ResizeHandleProps {
    direction: "horizontal" | "vertical"
    onMouseDown: (e: React.MouseEvent) => void
}

export function ResizeHandle({ direction, onMouseDown }: ResizeHandleProps) {
    const isHorizontal = direction === "horizontal"

    return (
        <div
            onMouseDown={onMouseDown}
            className={`
        group relative flex items-center justify-center shrink-0 bg-neutral-900
        hover:bg-neutral-700 active:bg-white/10 transition-colors z-10
        ${isHorizontal
                    ? "w-1 cursor-col-resize hover:w-1"
                    : "h-1 cursor-row-resize hover:h-1"
                }
      `}
        >
            <div
                className={`
          bg-neutral-700 group-hover:bg-neutral-400 group-active:bg-white transition-colors rounded-full
          ${isHorizontal ? "w-0.5 h-8" : "h-0.5 w-8"}
        `}
            />
        </div>
    )
}