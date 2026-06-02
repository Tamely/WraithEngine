import test from "node:test"
import assert from "node:assert/strict"

import { buildViewportWorldMovement } from "./viewport-movement.js"

function nearlyEqual(actual, expected, epsilon = 1e-6) {
  assert.equal(actual.length, expected.length)
  for (let index = 0; index < actual.length; index += 1) {
    assert.ok(
      Math.abs(actual[index] - expected[index]) <= epsilon,
      `component ${index} expected ${expected[index]}, got ${actual[index]}`
    )
  }
}

test("W moves forward relative to default editor camera yaw", () => {
  const movement = buildViewportWorldMovement({
    pressedKeys: new Set(["KeyW"]),
    yawDegrees: -90,
    pitchDegrees: 0,
  })

  nearlyEqual(movement, [0, 0, -0.08])
})

test("S moves backward relative to default editor camera yaw", () => {
  const movement = buildViewportWorldMovement({
    pressedKeys: new Set(["KeyS"]),
    yawDegrees: -90,
    pitchDegrees: 0,
  })

  nearlyEqual(movement, [0, 0, 0.08])
})

test("movement rotates with camera yaw instead of staying on world axes", () => {
  const movement = buildViewportWorldMovement({
    pressedKeys: new Set(["KeyW"]),
    yawDegrees: 0,
    pitchDegrees: 0,
  })

  nearlyEqual(movement, [0.08, 0, 0])
})

test("strafing uses the camera-relative right vector", () => {
  const movement = buildViewportWorldMovement({
    pressedKeys: new Set(["KeyD"]),
    yawDegrees: -90,
    pitchDegrees: 0,
  })

  nearlyEqual(movement, [0.08, 0, 0])
})
