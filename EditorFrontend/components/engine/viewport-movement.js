export function buildViewportWorldMovement({
  pressedKeys,
  yawDegrees,
  pitchDegrees,
  step = 0.08,
}) {
  const forwardInput =
    (pressedKeys.has("KeyW") ? 1 : 0) - (pressedKeys.has("KeyS") ? 1 : 0)
  const strafe =
    (pressedKeys.has("KeyD") ? 1 : 0) - (pressedKeys.has("KeyA") ? 1 : 0)
  const lift =
    (pressedKeys.has("Space") ? 1 : 0) -
    ((pressedKeys.has("ShiftLeft") || pressedKeys.has("ShiftRight")) ? 1 : 0)

  const yawRadians = (yawDegrees * Math.PI) / 180
  const pitchRadians = (pitchDegrees * Math.PI) / 180

  let forwardX = Math.cos(yawRadians) * Math.cos(pitchRadians)
  let forwardZ = Math.sin(yawRadians) * Math.cos(pitchRadians)
  const horizontalLength = Math.hypot(forwardX, forwardZ)

  if (horizontalLength > 0) {
    forwardX /= horizontalLength
    forwardZ /= horizontalLength
  } else {
    forwardX = 0
    forwardZ = -1
  }

  const rightX = -forwardZ
  const rightZ = forwardX

  return [
    (rightX * strafe + forwardX * forwardInput) * step,
    lift * step,
    (rightZ * strafe + forwardZ * forwardInput) * step,
  ]
}
