"use client"

import { useCallback, useEffect, useRef, useState } from "react"

type Direction = "horizontal" | "vertical"

interface UseResizeOptions {
    direction: Direction
    initialSize: number
    min: number
    max: number
    reverse?: boolean
}

export function useResize({ direction, initialSize, min, max, reverse = false }: UseResizeOptions) {
    const [size, setSize] = useState(initialSize)
    const dragging = useRef(false)
    const startPos = useRef(0)
    const startSize = useRef(0)

    const onMouseDown = useCallback(
        (e: React.MouseEvent) => {
            e.preventDefault()
            dragging.current = true
            startPos.current = direction === "horizontal" ? e.clientX : e.clientY
            startSize.current = size
            document.body.style.cursor = direction === "horizontal" ? "col-resize" : "row-resize"
            document.body.style.userSelect = "none"
        },
        [direction, size]
    )

    useEffect(() => {
        const onMouseMove = (e: MouseEvent) => {
            if (!dragging.current) return
            const current = direction === "horizontal" ? e.clientX : e.clientY
            const delta = reverse ? startPos.current - current : current - startPos.current
            const next = Math.min(max, Math.max(min, startSize.current + delta))
            setSize(next)
        }

        const onMouseUp = () => {
            if (!dragging.current) return
            dragging.current = false
            document.body.style.cursor = ""
            document.body.style.userSelect = ""
        }

        window.addEventListener("mousemove", onMouseMove)
        window.addEventListener("mouseup", onMouseUp)
        return () => {
            window.removeEventListener("mousemove", onMouseMove)
            window.removeEventListener("mouseup", onMouseUp)
        }
    }, [direction, min, max, reverse])

    return { size, onMouseDown }
}