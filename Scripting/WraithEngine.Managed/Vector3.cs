using System.Runtime.InteropServices;

namespace WraithEngine;

/// <summary>
/// Three-component float vector. Layout matches glm::vec3 in the C++ engine.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector3
{
    public float X;
    public float Y;
    public float Z;

    public Vector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public static Vector3 Zero  => new(0f, 0f, 0f);
    public static Vector3 One   => new(1f, 1f, 1f);
    public static Vector3 Up    => new(0f, 1f, 0f);
    public static Vector3 Right => new(1f, 0f, 0f);

    public override string ToString() => $"({X:F3}, {Y:F3}, {Z:F3})";
}
