#include "FreeCamMotion.h"

#include <Logging.h>
#include <IconsMaterialDesign.h>
#include <Globals.h>

#include <Glacier/ZGameLoopManager.h>
#include <Glacier/ZScene.h>

void FreeCamMotion::OnEngineInitialized() {
    Logger::Info("FreeCamMotion has been initialized!");

    // Register a function to be called on every game frame while the game is in play mode.
    const ZMemberDelegate<FreeCamMotion, void(const SGameUpdateEvent&)> s_Delegate(this, &FreeCamMotion::OnFrameUpdate);
    Globals::GameLoopManager->RegisterFrameUpdate(s_Delegate, 1, EUpdateMode::eUpdatePlayMode);

    // Install a hook to print the name of the scene every time the game loads a new one.
    Hooks::ZEntitySceneContext_LoadScene->AddDetour(this, &FreeCamMotion::OnLoadScene);
}

FreeCamMotion::~FreeCamMotion() {
    // Unregister our frame update function when the mod unloads.
    const ZMemberDelegate<FreeCamMotion, void(const SGameUpdateEvent&)> s_Delegate(this, &FreeCamMotion::OnFrameUpdate);
    Globals::GameLoopManager->UnregisterFrameUpdate(s_Delegate, 1, EUpdateMode::eUpdatePlayMode);
}

void FreeCamMotion::OnDrawMenu() {
    // Toggle our message when the user presses our button.
    if (ImGui::Button(ICON_MD_LOCAL_FIRE_DEPARTMENT " FreeCamMotion")) {
        m_ShowMessage = !m_ShowMessage;
    }
}

void FreeCamMotion::OnDrawUI(bool p_HasFocus) {
    if (m_ShowMessage) {
        // Show a window for our mod.
        if (ImGui::Begin("FreeCamMotion", &m_ShowMessage)) {
            // Only show these when the window is expanded.
            ImGui::Text("Hello from FreeCamMotion!");
        }
        ImGui::End();
    }
}

void FreeCamMotion::OnFrameUpdate(const SGameUpdateEvent &p_UpdateEvent) {
    // This function is called every frame while the game is in play mode.
}

DEFINE_PLUGIN_DETOUR(FreeCamMotion, void, OnLoadScene, ZEntitySceneContext* th, ZSceneData& p_SceneData) {
    Logger::Debug("Loading scene: {}", p_SceneData.m_sceneName);
    return HookResult<void>(HookAction::Continue());
}

DECLARE_ZHM_PLUGIN(FreeCamMotion);
