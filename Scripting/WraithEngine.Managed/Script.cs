namespace WraithEngine;

/// <summary>
/// Base class for all WraithEngine scripts. Subclass this in your user assembly
/// and attach the class name to an Actor via the editor details panel.
/// </summary>
public abstract class Script
{
    // Set by ScriptHost via ManagedObject.SetFieldValue immediately after
    // instantiation, before OnCreate() is called. C++ writes the objectId string
    // directly into this field using Coral's reflection-backed SetFieldValueRaw.
#pragma warning disable CS0649
    internal string _ObjectId = string.Empty;
#pragma warning restore CS0649

    private GameObject? _cachedOwner;

    /// <summary>
    /// The scene object this script is attached to.
    /// Always valid during OnCreate / OnTick / OnDestroy.
    /// </summary>
    protected GameObject GameObject => _cachedOwner ??= new GameObject(_ObjectId);

    // -----------------------------------------------------------------------
    // Lifecycle — override in your script class
    // -----------------------------------------------------------------------

    /// <summary>Called once when the script is first instantiated.</summary>
    public virtual void OnCreate() { }

    /// <summary>Called every engine tick while the script is alive.</summary>
    /// <param name="dt">Delta time in seconds since the last tick.</param>
    public virtual void OnTick(float dt) { }

    /// <summary>Called when the script is destroyed (object deleted or scripts reloaded).</summary>
    public virtual void OnDestroy() { }
}
