// ScriptingTests.cpp
// End-to-end tests for the Coral C# scripting subsystem (Change 10).
//
// All 5 tests share a single Coral HostInstance (initialised once in
// SetUpTestSuite) because the .NET runtime can only be loaded once per
// process.  Individual tests call LoadUserAssembly(), which internally
// calls DestroyAllScripts() first, giving each test a clean script slate.

#include <gtest/gtest.h>

#if AXIOM_SCRIPTING_ENABLED

#include <Scripting/ScriptHost.h>
#include <Session/EditorSession.h>
#include <Session/EditorCommand.h>

#include <Core/Log.h>
#include <filesystem>

namespace {

// -----------------------------------------------------------------------
// Path constants (injected by CMake at configure time)
// -----------------------------------------------------------------------
static constexpr const char *kCoralManagedDir    = AXIOM_CORAL_MANAGED_DIR;
static constexpr const char *kEngineManagedDir   = AXIOM_MANAGED_DIR;
static constexpr const char *kTestScriptsDir     = AXIOM_TEST_SCRIPTS_DIR;
static constexpr const char *kRestrictedScriptsDir = AXIOM_RESTRICTED_SCRIPTS_DIR;

Axiom::CommandContext HostContext(uint64_t FrameIndex = 1) {
    return {
        .Session = Axiom::SessionId{1},
        .User = Axiom::SessionUserId{1},
        .FrameIndex = FrameIndex,
        .DeltaTimeSeconds = 1.0f / 60.0f,
    };
}

// -----------------------------------------------------------------------
// Fixture — one Coral runtime for the entire test binary
// -----------------------------------------------------------------------

class ScriptingTest : public ::testing::Test {
public:
    // Shared across all tests in this suite.
    static Axiom::ScriptHost    *s_Host;
    static Axiom::EditorSession *s_Session;

    static void SetUpTestSuite() {
        // Initialise the spdlog logger — ScriptHost::Initialize fires Coral
        // message callbacks that call A_CORE_* macros, which crash if the
        // logger hasn't been set up yet.
        Axiom::Log::Init();

        // Build a session with a world folder and a single Actor that has
        // ScriptClass pre-wired to WraithTestScripts.TickScript.
        // LoadUserAssembly() reads this and re-instantiates scripts for
        // every Actor that has a ScriptClass set.
        s_Session = new Axiom::EditorSession(Axiom::SessionId{1});
        s_Session->SetSceneItems({{
            .Id          = "world",
            .DisplayName = "World",
            .Kind        = Axiom::EditorSceneItemKind::Folder,
            .Visible     = true,
            .Children    = {},
        }});
        s_Session->SetObjectDetails({
            {
                .ObjectId          = "world",
                .DisplayName       = "World",
                .Kind              = Axiom::EditorSceneItemKind::Folder,
                .Visible           = true,
                .SupportsTransform = false,
                .TransformReadOnly = true,
            },
            {
                .ObjectId          = "actor1",
                .DisplayName       = "TestActor",
                .Kind              = Axiom::EditorSceneItemKind::Actor,
                .Visible           = true,
                .SupportsTransform = true,
                .TransformReadOnly = false,
                .Transform         = Axiom::EditorTransformDetails{},
                .ScriptClass       = "WraithTestScripts.TickScript",
            },
        });

        // Initialise with Restricted profile so that RestrictedProfileBlocks
        // is tested accurately. WraithTestScripts itself doesn't reference any
        // blocked assemblies, so it loads cleanly in Restricted mode.
        s_Host = new Axiom::ScriptHost();
        s_Host->Initialize(kCoralManagedDir,
                           Axiom::ScriptTrustProfile::Restricted);
        s_Host->LoadEngineAssembly(kEngineManagedDir);
        s_Host->RegisterInternalCalls(*s_Session,
                                      Axiom::SessionId{1},
                                      Axiom::SessionUserId{1});
        s_Session->Subscribe(s_Host);
    }

    static void TearDownTestSuite() {
        if (s_Session && s_Host)
            s_Session->Unsubscribe(s_Host);
        if (s_Host) {
            s_Host->Shutdown();
            delete s_Host;
            s_Host = nullptr;
        }
        delete s_Session;
        s_Session = nullptr;
    }
};

Axiom::ScriptHost    *ScriptingTest::s_Host    = nullptr;
Axiom::EditorSession *ScriptingTest::s_Session  = nullptr;

} // anonymous namespace

// -----------------------------------------------------------------------
// 1. ScriptHostLifecycle
// Confirm Initialize() and LoadEngineAssembly() succeed without crashing.
// -----------------------------------------------------------------------

TEST_F(ScriptingTest, ScriptHostLifecycle) {
    if (s_Session->GetRuntimeState() != Axiom::EditorRuntimeState::Edit) {
        s_Session->Submit(HostContext(100), {.Payload = Axiom::StopSessionCommand{}});
        s_Session->Tick();
    }
    EXPECT_TRUE(s_Host->IsInitialized());
    EXPECT_TRUE(s_Host->IsEngineAssemblyLoaded());
}

// -----------------------------------------------------------------------
// 2. InternalCallRoundTrip
// TickScript.OnTick() calls GameObject.SetTransform({1,2,3}) via an
// internal call. After Tick() + Session.Tick() the transform should be
// visible in the session state.
// -----------------------------------------------------------------------

TEST_F(ScriptingTest, InternalCallRoundTrip) {
    if (s_Session->GetRuntimeState() != Axiom::EditorRuntimeState::Edit) {
        s_Session->Submit(HostContext(101), {.Payload = Axiom::StopSessionCommand{}});
        s_Session->Tick();
    }
    s_Host->LoadUserAssembly(
        std::filesystem::path(kTestScriptsDir) / "WraithTestScripts.dll");
    ASSERT_TRUE(s_Host->IsUserAssemblyLoaded());

    s_Session->Submit(HostContext(1), {.Payload = Axiom::PlaySessionCommand{}});
    s_Session->Tick();

    // Drive OnTick — queues a SetTransformCommand on the session.
    s_Host->Tick(1.0f / 60.0f);

    // Flush the command queue so the transform is written into session state.
    s_Session->Tick();

    const auto *Details = s_Session->FindObjectDetails("actor1");
    ASSERT_NE(Details, nullptr);
    ASSERT_TRUE(Details->Transform.has_value());

    EXPECT_FLOAT_EQ(Details->Transform->Location.x, 1.0f);
    EXPECT_FLOAT_EQ(Details->Transform->Location.y, 2.0f);
    EXPECT_FLOAT_EQ(Details->Transform->Location.z, 3.0f);
    s_Session->Submit(HostContext(102), {.Payload = Axiom::StopSessionCommand{}});
    s_Session->Tick();
}

// -----------------------------------------------------------------------
// 3. ScriptLifecycle
// Verify that scripts are instantiated on LoadUserAssembly() and that
// their OnTick() side-effects are visible after a session tick.
// -----------------------------------------------------------------------

TEST_F(ScriptingTest, ScriptLifecycle) {
    if (s_Session->GetRuntimeState() != Axiom::EditorRuntimeState::Edit) {
        s_Session->Submit(HostContext(103), {.Payload = Axiom::StopSessionCommand{}});
        s_Session->Tick();
    }
    // LoadUserAssembly destroys any live scripts first, then re-instantiates
    // TickScript for actor1 (which has ScriptClass set).
    s_Host->LoadUserAssembly(
        std::filesystem::path(kTestScriptsDir) / "WraithTestScripts.dll");
    ASSERT_TRUE(s_Host->IsUserAssemblyLoaded());

    s_Session->Submit(HostContext(2), {.Payload = Axiom::PlaySessionCommand{}});
    s_Session->Tick();

    s_Host->Tick(1.0f / 60.0f);
    s_Session->Tick();

    const auto *Details = s_Session->FindObjectDetails("actor1");
    ASSERT_NE(Details, nullptr);
    ASSERT_TRUE(Details->Transform.has_value());
    // OnTick sets Location.x to 1 — confirm the script ran.
    EXPECT_FLOAT_EQ(Details->Transform->Location.x, 1.0f);
    s_Session->Submit(HostContext(104), {.Payload = Axiom::StopSessionCommand{}});
    s_Session->Tick();
}

// -----------------------------------------------------------------------
// 4. HotReload
// Reload the user assembly without restarting the server. Scripts should
// be re-instantiated and continue to produce the same side-effects.
// -----------------------------------------------------------------------

TEST_F(ScriptingTest, HotReload) {
    if (s_Session->GetRuntimeState() != Axiom::EditorRuntimeState::Edit) {
        s_Session->Submit(HostContext(105), {.Payload = Axiom::StopSessionCommand{}});
        s_Session->Tick();
    }
    const auto AssemblyPath =
        std::filesystem::path(kTestScriptsDir) / "WraithTestScripts.dll";

    s_Host->LoadUserAssembly(AssemblyPath);
    ASSERT_TRUE(s_Host->IsUserAssemblyLoaded());

    s_Session->Submit(HostContext(3), {.Payload = Axiom::PlaySessionCommand{}});
    s_Session->Tick();

    // Simulate a hot reload: unload ALC, reload assembly, re-instantiate.
    s_Host->ReloadUserAssembly();
    EXPECT_TRUE(s_Host->IsUserAssemblyLoaded());

    // Script should be live again after reload.
    s_Host->Tick(1.0f / 60.0f);
    s_Session->Tick();

    const auto *Details = s_Session->FindObjectDetails("actor1");
    ASSERT_NE(Details, nullptr);
    ASSERT_TRUE(Details->Transform.has_value());
    EXPECT_FLOAT_EQ(Details->Transform->Location.x, 1.0f);
    s_Session->Submit(HostContext(106), {.Payload = Axiom::StopSessionCommand{}});
    s_Session->Tick();
}

// -----------------------------------------------------------------------
// 5. RestrictedProfileBlocks
// Loading an assembly that references System.Net.Http must be rejected
// in Restricted mode. The user assembly must NOT be marked as loaded.
// -----------------------------------------------------------------------

TEST_F(ScriptingTest, RestrictedProfileBlocks) {
    if (s_Session->GetRuntimeState() != Axiom::EditorRuntimeState::Edit) {
        s_Session->Submit(HostContext(107), {.Payload = Axiom::StopSessionCommand{}});
        s_Session->Tick();
    }
    ASSERT_TRUE(s_Host->IsRestricted())
        << "Host must be in Restricted mode for this test to be meaningful";

    s_Host->LoadUserAssembly(
        std::filesystem::path(kRestrictedScriptsDir) /
        "WraithRestrictedScript.dll");

    // ValidateUserAssembly should have caught the System.Net.Http reference
    // and unloaded the ALC — no user assembly should be live.
    EXPECT_FALSE(s_Host->IsUserAssemblyLoaded());
}

TEST_F(ScriptingTest, EditModeDoesNotTickScripts) {
    if (s_Session->GetRuntimeState() != Axiom::EditorRuntimeState::Edit) {
        s_Session->Submit(HostContext(108), {.Payload = Axiom::StopSessionCommand{}});
        s_Session->Tick();
    }
    s_Host->LoadUserAssembly(
        std::filesystem::path(kTestScriptsDir) / "WraithTestScripts.dll");
    ASSERT_TRUE(s_Host->IsUserAssemblyLoaded());

    const auto *Before = s_Session->FindObjectDetails("actor1");
    ASSERT_NE(Before, nullptr);
    ASSERT_TRUE(Before->Transform.has_value());
    EXPECT_FLOAT_EQ(Before->Transform->Location.x, 0.0f);

    s_Host->Tick(1.0f / 60.0f);
    s_Session->Tick();

    const auto *After = s_Session->FindObjectDetails("actor1");
    ASSERT_NE(After, nullptr);
    ASSERT_TRUE(After->Transform.has_value());
    EXPECT_FLOAT_EQ(After->Transform->Location.x, 0.0f);
}

TEST_F(ScriptingTest, PauseFreezesScriptTicks) {
    if (s_Session->GetRuntimeState() != Axiom::EditorRuntimeState::Edit) {
        s_Session->Submit(HostContext(109), {.Payload = Axiom::StopSessionCommand{}});
        s_Session->Tick();
    }
    s_Host->LoadUserAssembly(
        std::filesystem::path(kTestScriptsDir) / "WraithTestScripts.dll");
    ASSERT_TRUE(s_Host->IsUserAssemblyLoaded());

    s_Session->Submit(HostContext(4), {.Payload = Axiom::PlaySessionCommand{}});
    s_Session->Tick();
    s_Host->Tick(1.0f / 60.0f);
    s_Session->Tick();

    s_Session->Submit(HostContext(5), {.Payload = Axiom::PauseSessionCommand{}});
    s_Session->Tick();

    auto ManualCtx = HostContext(6);
    ManualCtx.IsScriptContext = true;
    s_Session->Submit(ManualCtx,
        {.Payload = Axiom::SetTransformCommand{
            .ObjectId = "actor1",
            .Location = {0.0f, 0.0f, 0.0f},
            .RotationDegrees = {0.0f, 0.0f, 0.0f},
            .Scale = {1.0f, 1.0f, 1.0f},
        }});
    s_Session->Tick();

    s_Host->Tick(1.0f / 60.0f);
    s_Session->Tick();

    const auto *Details = s_Session->FindObjectDetails("actor1");
    ASSERT_NE(Details, nullptr);
    ASSERT_TRUE(Details->Transform.has_value());
    EXPECT_FLOAT_EQ(Details->Transform->Location.x, 0.0f);
    EXPECT_FLOAT_EQ(Details->Transform->Location.y, 0.0f);
    EXPECT_FLOAT_EQ(Details->Transform->Location.z, 0.0f);
    s_Session->Submit(HostContext(110), {.Payload = Axiom::StopSessionCommand{}});
    s_Session->Tick();
}

#endif // AXIOM_SCRIPTING_ENABLED
