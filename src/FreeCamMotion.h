#pragma once

#include <unordered_map>

#include "IPluginInterface.h"

#include <Glacier/ZEntity.h>
#include <Glacier/ZInput.h>

class FreeCamMotion : public IPluginInterface
{
public:
    FreeCamMotion();
    ~FreeCamMotion() override;

    void Init() override;
    void OnEngineInitialized() override;
    void OnDrawMenu() override;
    void OnDrawUI(bool p_HasFocus) override;

private:
    void OnFrameUpdate(const SGameUpdateEvent& p_UpdateEvent);
    void ToggleFreecam();
    void EnableFreecam();
    void DisableFreecam();

    void SetCameraDestination();
    void SetCameraStart();
    void BeginCameraMove();
    void EndCameraMove();
    void CamMoveUpdate(float p_Progress);
    void CamJumpToDestination();

private:
    enum CamMoveMode
    {
        LINEAR,
        RAMP_UP,
        RAMP_DOWN,
        RAMP_UP_DOWN,
        NUM_MOVE_MODES
    };

    DECLARE_PLUGIN_DETOUR(FreeCamMotion, bool, ZInputAction_Digital, ZInputAction* th, int a2);
    DECLARE_PLUGIN_DETOUR(FreeCamMotion, void, OnLoadScene, ZEntitySceneContext*, ZSceneData&);
    DECLARE_PLUGIN_DETOUR(FreeCamMotion, void, OnClearScene, ZEntitySceneContext*, bool);

private:
    volatile bool m_FreeCamActive;
    volatile bool m_ShouldToggle;
    volatile bool m_FreeCamFrozen;
    ZEntityRef m_OriginalCam;
    ZInputAction m_ToggleFreeCamAction;
    ZInputAction m_FreezeFreeCamActionGc;
    ZInputAction m_FreezeFreeCamActionKb;
    bool m_ControlsVisible;
    bool m_SettingsVisible;
    bool m_HasToggledFreecamBefore;
    std::unordered_map<std::string, std::string> m_PcControls;
    std::unordered_map<std::string, std::string> m_ControllerControls;

    bool m_CamDestinationSet;
    bool m_CamStartSet;
    bool m_CamMoveActive;
    float4 m_CamDestPosition;
    float4 m_CamStartPosition;
    float4 m_CamMovePosDelta;
    float m_CamDestOrientation[3];
    float m_CamStartOrientation[3];
    float m_CamMoveOrientationDelta[3];
    float m_CamMoveProgress;
    float m_CamMoveDuration;
    float m_CamMoveSpeed;

    CamMoveMode m_CamMoveMode;
    const char* m_MoveModeText[NUM_MOVE_MODES] = { "Linear", "Ramp Up", "Ramp Down", "Ramp Up and Down" };
};

DECLARE_ZHM_PLUGIN(FreeCamMotion)
