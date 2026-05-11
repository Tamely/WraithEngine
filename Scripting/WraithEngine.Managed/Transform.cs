using System.Runtime.InteropServices;

namespace WraithEngine;

/// <summary>
/// Object transform in local space. Layout matches EditorTransformDetails in the C++ engine.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Transform
{
    public Vector3 Location;
    public Vector3 RotationDegrees;
    public Vector3 Scale;

    public Transform(Vector3 location, Vector3 rotationDegrees, Vector3 scale)
    {
        Location        = location;
        RotationDegrees = rotationDegrees;
        Scale           = scale;
    }

    public static Transform Identity => new(Vector3.Zero, Vector3.Zero, Vector3.One);

    public override string ToString() =>
        $"T:{Location} R:{RotationDegrees} S:{Scale}";
}
