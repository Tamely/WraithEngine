using WraithEngine;

namespace WraithTestScripts;

/// <summary>
/// Test script used by ScriptingTests.
/// Each OnTick call submits a SetTransform that moves the owning Actor to (1, 2, 3).
/// Tests verify this side-effect to confirm internal calls and script lifecycle work.
/// </summary>
public class TickScript : Script
{
    public override void OnTick(float dt)
    {
        GameObject.SetTransform(new Transform(
            new Vector3(1f, 2f, 3f),
            Vector3.Zero,
            Vector3.One));
    }
}
