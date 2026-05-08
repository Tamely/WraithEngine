using System.Runtime.CompilerServices;
using Coral.Managed.Interop;

namespace WraithEngine;

/// <summary>
/// Handle to a scene object. Wraps a stable object ID string.
/// Internal call function pointer fields are registered by the C++ ScriptHost
/// at startup (Change 4). Until then, methods return safe defaults.
/// </summary>
public sealed class GameObject
{
    // -----------------------------------------------------------------------
    // Internal call function pointer fields.
    // Naming convention: "<Namespace.ClassName>+<FieldName>,<AssemblyName>"
    // Registered via ManagedAssembly::AddInternalCall on the C++ side.
    // CS0649 suppressed: fields are set via reflection by Coral at runtime.
    // -----------------------------------------------------------------------
#pragma warning disable CS0649
    private static unsafe delegate* unmanaged<NativeString, NativeString> s_GetName;
    private static unsafe delegate* unmanaged<NativeString, Transform*, void> s_GetTransform;
    private static unsafe delegate* unmanaged<NativeString, Transform*, void> s_SetTransform;
    private static unsafe delegate* unmanaged<NativeString, Bool32> s_GetVisible;
#pragma warning restore CS0649

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------
    private readonly string m_ObjectId;

    internal GameObject(string objectId)
    {
        m_ObjectId = objectId;
    }

    /// <summary>The stable ID that uniquely identifies this object in the session.</summary>
    public string ObjectId => m_ObjectId;

    /// <summary>Returns the display name shown in the outliner.</summary>
    public unsafe string GetName()
    {
        if (s_GetName == null)
            return m_ObjectId;

        using NativeString id = m_ObjectId;
        using NativeString result = s_GetName(id);
        return (string?)result ?? m_ObjectId;
    }

    /// <summary>Returns the object's local-space transform.</summary>
    public unsafe Transform GetTransform()
    {
        var t = Transform.Identity;
        if (s_GetTransform == null)
            return t;

        using NativeString id = m_ObjectId;
        s_GetTransform(id, &t);
        return t;
    }

    /// <summary>Submits a SetTransform command through the authoritative session.</summary>
    public unsafe void SetTransform(Transform transform)
    {
        if (s_SetTransform == null)
            return;

        using NativeString id = m_ObjectId;
        s_SetTransform(id, &transform);
    }

    /// <summary>Returns whether the object is currently visible in the scene.</summary>
    public unsafe bool GetVisible()
    {
        if (s_GetVisible == null)
            return true;

        using NativeString id = m_ObjectId;
        return s_GetVisible(id);
    }

    public override string ToString() => $"GameObject({m_ObjectId})";
}
