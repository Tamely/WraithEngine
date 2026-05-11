using System.Reflection;
using System.Runtime.Loader;

namespace WraithEngine.Internal;

/// <summary>
/// AssemblyLoadContext used for user scripts running in Trusted mode (local / native editor).
/// Imposes no additional restrictions; user scripts have access to the full BCL and may
/// load any assembly resolvable from the default context.
/// </summary>
internal sealed class TrustedAssemblyLoadContext : AssemblyLoadContext
{
    internal TrustedAssemblyLoadContext()
        : base("UserScripts-Trusted", isCollectible: true) { }

    /// <summary>Delegate all resolution to the default ALC — no restrictions.</summary>
    protected override Assembly? Load(AssemblyName assemblyName) => null;
}
