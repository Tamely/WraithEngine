namespace WraithEngine;

/// <summary>
/// Base class for all WraithEngine scripts. Subclass this in your user assembly
/// and attach the class name to an Actor via the editor details panel.
/// </summary>
public abstract class Script
{
    // Set by the ScriptHost immediately after instantiation, before OnCreate().
    internal GameObject? Owner { get; set; }

    /// <summary>
    /// The scene object this script is attached to.
    /// Always valid during OnCreate / OnTick / OnDestroy.
    /// </summary>
    protected GameObject GameObject =>
        Owner ?? throw new InvalidOperationException(
            "Script.GameObject accessed before the script was attached to an object.");

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
