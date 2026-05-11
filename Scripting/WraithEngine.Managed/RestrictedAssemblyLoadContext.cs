using System.Reflection;
using System.Runtime.Loader;

namespace WraithEngine.Internal;

/// <summary>
/// AssemblyLoadContext used for user scripts running in Restricted (hosted) mode.
///
/// Restrictions enforced:
///   • <see cref="Load"/> throws <see cref="NotSupportedException"/> for any attempt to
///     dynamically load assemblies whose names begin with a blocked prefix
///     (System.Net.*, System.Reflection.Emit, System.Diagnostics.Process, …).
///   • <see cref="ValidateAssemblyReferences"/> scans the references of an already-loaded
///     user assembly and throws before any scripts are instantiated if blocked assemblies
///     appear in the manifest.
///
/// Limitation: BCL types compiled into System.Private.CoreLib (e.g. System.IO.File)
/// are always present in the shared runtime and cannot be blocked via ALC restrictions
/// alone. Full I/O isolation requires OS-level sandboxing (seccomp, process isolation).
/// </summary>
internal sealed class RestrictedAssemblyLoadContext : AssemblyLoadContext
{
    internal static readonly string[] BlockedPrefixes =
    [
        "System.Net.Http",
        "System.Net.Sockets",
        "System.Net.WebSockets",
        "System.Net.Security",
        "System.Net.NetworkInformation",
        "System.Reflection.Emit",
        "System.Diagnostics.Process",
    ];

    internal RestrictedAssemblyLoadContext()
        : base("UserScripts-Restricted", isCollectible: true) { }

    /// <summary>
    /// Block dynamic loading of restricted assemblies. Permitted assemblies
    /// are delegated to the default ALC (returns null).
    /// </summary>
    protected override Assembly? Load(AssemblyName assemblyName)
    {
        var name = assemblyName.Name ?? string.Empty;
        foreach (var blocked in BlockedPrefixes)
        {
            if (name.StartsWith(blocked, StringComparison.OrdinalIgnoreCase))
            {
                throw new NotSupportedException(
                    $"Script attempted to load restricted assembly '{name}'. " +
                    $"Dynamic loading of '{blocked}' assemblies is not permitted " +
                    $"in Hosted (Restricted) mode.");
            }
        }
        return null;
    }

    /// <summary>
    /// Validates that <paramref name="userAssembly"/> does not reference any blocked
    /// assemblies. Throws <see cref="NotSupportedException"/> if validation fails.
    /// Safe to call on an already-loaded assembly — uses reflection only, executes
    /// no user code.
    /// </summary>
    internal static void ValidateAssemblyReferences(Assembly userAssembly)
    {
        foreach (var reference in userAssembly.GetReferencedAssemblies())
        {
            var refName = reference.Name ?? string.Empty;
            foreach (var blocked in BlockedPrefixes)
            {
                if (refName.StartsWith(blocked, StringComparison.OrdinalIgnoreCase))
                {
                    throw new NotSupportedException(
                        $"User script assembly references restricted assembly '{refName}'. " +
                        $"Access to '{blocked}' is not permitted in Hosted (Restricted) mode.");
                }
            }
        }
    }
}
