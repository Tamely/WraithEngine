using System.Net.Http;
using WraithEngine;

namespace WraithRestrictedScript;

/// <summary>
/// Test script whose assembly manifest references System.Net.Http.
///
/// This file exists solely so that "System.Net.Http" appears in
/// <see cref="System.Reflection.Assembly.GetReferencedAssemblies"/> for this
/// assembly. The RestrictedProfileBlocks test loads this assembly in Restricted
/// mode and expects ScriptHost to reject it before any scripts are instantiated.
/// </summary>
public class NetworkScript : Script
{
    public override void OnCreate()
    {
        // Reference HttpClient to force System.Net.Http into the manifest.
        _ = typeof(HttpClient).FullName;
    }
}
