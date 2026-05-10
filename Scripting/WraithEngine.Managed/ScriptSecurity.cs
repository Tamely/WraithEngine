using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
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
    /// reference any restricted assemblies.
    ///
    /// Uses <see cref="PEReader"/> to inspect the assembly manifest directly from the
    /// file without loading the assembly into any ALC.  This avoids the empty
    /// <c>Assembly.Location</c> problem that arises when Coral loads assemblies via
    /// <c>LoadFromStream</c> (MemoryMappedFile), and allows validation to run before
    /// the assembly is placed into any AssemblyLoadContext.
    ///
    /// Returns <see langword="null"/> when validation passes (or when not in Restricted
    /// mode). Returns a human-readable error string when validation fails.
    ///
    /// This method intentionally does NOT throw — it returns an error string so that
    /// the C++ caller can act on the result without relying on Coral's exception
    /// propagation (Coral routes managed exceptions through ExceptionCallback rather
    /// than rethrowing as C++ exceptions).
    /// </summary>
    internal static string? ValidateUserAssemblyResult(string assemblyPath)
    {
        if (!IsRestricted())
            return null;

        if (!System.IO.File.Exists(assemblyPath))
            return $"assembly file not found: '{assemblyPath}'";

        try
        {
            using var stream = System.IO.File.OpenRead(assemblyPath);
            using var peReader = new PEReader(stream);

            if (!peReader.HasMetadata)
                return $"assembly '{assemblyPath}' contains no CLI metadata";

            var metadataReader = peReader.GetMetadataReader();
            foreach (var handle in metadataReader.AssemblyReferences)
            {
                var reference = metadataReader.GetAssemblyReference(handle);
                var refName = metadataReader.GetString(reference.Name);
                foreach (var blocked in RestrictedAssemblyLoadContext.BlockedPrefixes)
                {
                    if (refName.StartsWith(blocked, StringComparison.OrdinalIgnoreCase))
                    {
                        return $"references restricted assembly '{refName}' " +
                               $"('{blocked}' is not permitted in Hosted/Restricted mode)";
                    }
                }
            }

            return null;
        }
        catch (Exception ex)
        {
            return $"could not inspect assembly '{assemblyPath}': {ex.Message}";
        }
    }
}
