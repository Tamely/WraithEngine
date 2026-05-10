using System.Reflection;
using System.Runtime.Loader;
using Coral.Managed.Interop;

namespace WraithEngine.Internal;

/// <summary>
/// Bridge between the C++ trust-profile setting and managed-side security enforcement.
/// </summary>
internal static class ScriptSecurity
{
    // -----------------------------------------------------------------------
    // Internal call function pointer field.
    // Set by ScriptHost::RegisterInternalCalls() via AddInternalCall.
    // CS0649 suppressed: field is set via reflection by Coral at runtime.
    // -----------------------------------------------------------------------
#pragma warning disable CS0649
    private static unsafe delegate* unmanaged<Bool32> s_IsRestricted;
#pragma warning restore CS0649

    internal static unsafe bool IsRestricted() =>
        s_IsRestricted != null && s_IsRestricted();

    /// <summary>
    /// Validates that the user assembly at <paramref name="assemblyPath"/> does not
    /// reference any restricted assemblies. No-op when not in Restricted mode.
    ///
    /// The assembly must already be loaded (Coral loads it first); this method locates
    /// it by path in <see cref="AssemblyLoadContext.All"/> and then calls
    /// <see cref="Assembly.GetReferencedAssemblies"/> to check manifest references.
    ///
    /// Throws <see cref="NotSupportedException"/> if validation fails.
    /// </summary>
    internal static void ValidateUserAssembly(string assemblyPath)
    {
        if (!IsRestricted())
            return;

        // Find the assembly that was just loaded by Coral.
        Assembly? userAssembly = null;
        foreach (var alc in AssemblyLoadContext.All)
        {
            foreach (var asm in alc.Assemblies)
            {
                if (string.Equals(asm.Location, assemblyPath, StringComparison.OrdinalIgnoreCase))
                {
                    userAssembly = asm;
                    break;
                }
            }
            if (userAssembly is not null)
                break;
        }

        if (userAssembly is null)
        {
            // Shouldn't happen — Coral just loaded it — but treat as untrusted.
            throw new InvalidOperationException(
                $"Security validation failed: could not locate loaded assembly '{assemblyPath}' " +
                $"in any AssemblyLoadContext.");
        }

        RestrictedAssemblyLoadContext.ValidateAssemblyReferences(userAssembly);
    }
}
