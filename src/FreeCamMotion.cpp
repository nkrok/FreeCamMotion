#include "FreeCamMotion.h"

#include <random>

#include "Events.h"
#include "Functions.h"
#include "Logging.h"

#include <Glacier/ZActor.h>
#include <Glacier/SGameUpdateEvent.h>
#include <Glacier/ZObject.h>
#include <Glacier/ZCameraEntity.h>
#include <Glacier/ZApplicationEngineWin32.h>
#include <Glacier/ZEngineAppCommon.h>
#include <Glacier/ZFreeCamera.h>
#include <Glacier/ZRender.h>
#include <Glacier/ZGameLoopManager.h>
#include <Glacier/ZHitman5.h>
#include <Glacier/ZHM5InputManager.h>

#include "IconsMaterialDesign.h"

FreeCamMotion::FreeCamMotion() :
    m_FreeCamActive(false),
    m_ShouldToggle(false),
    m_FreeCamFrozen(false),
    m_ToggleFreeCamAction("KBMButtonX"),
    m_FreezeFreeCamActionGc("ActivateGameControl0"),
    m_FreezeFreeCamActionKb("KBMInspectNode"),
    m_ControlsVisible(false),
    m_HasToggledFreecamBefore(false),
    m_CamMoveActive(false),
    m_CamDestinationSet(false)
{
    m_PcControls = {
        { "P", "Toggle freecam" },
        { "F3", "Lock camera and enable 47 input" },
        { "Ctrl + W/S", "Change FOV" },
        { "Ctrl + A/D", "Roll camera" },
        { "Alt + W/S", "Change camera speed" },
        { "Space + Q/E", "Change camera height" },
        { "Space + W/S", "Move camera on axis" },
        { "Shift", "Increase camera speed" },
    };

    m_ControllerControls = {
        { "Y + L", "Change FOV" },
        { "A + L", "Roll camera" },
        { "A + L press", "Reset rotation" },
        { "B + R", "Change camera speed" },
        { "RT", "Increase camera speed" },
        { "LB", "Lock camera and enable 47 input" },
        { "LT + R", "Change camera height" },
    };
}

FreeCamMotion::~FreeCamMotion()
{
    const ZMemberDelegate<FreeCamMotion, void(const SGameUpdateEvent&)> s_Delegate(this, &FreeCamMotion::OnFrameUpdate);
    Globals::GameLoopManager->UnregisterFrameUpdate(s_Delegate, 1, EUpdateMode::eUpdatePlayMode);

    // Reset the camera to default when unloading with freecam active.
    if (m_FreeCamActive)
    {
        TEntityRef<IRenderDestinationEntity> s_RenderDest;
        Functions::ZCameraManager_GetActiveRenderDestinationEntity->Call(Globals::CameraManager, &s_RenderDest);

        s_RenderDest.m_pInterfaceRef->SetSource(&m_OriginalCam);

        // Enable Hitman input.
        TEntityRef<ZHitman5> s_LocalHitman;
        Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);

        if (s_LocalHitman)
        {
            auto* s_InputControl = Functions::ZHM5InputManager_GetInputControlForLocalPlayer->Call(Globals::InputManager);

            if (s_InputControl)
            {
                Logger::Debug("Got local hitman entity and input control! Enabling input. {} {}", fmt::ptr(s_InputControl), fmt::ptr(s_LocalHitman.m_pInterfaceRef));
                s_InputControl->m_bActive = true;
            }
        }
    }
}

void FreeCamMotion::Init()
{
    Hooks::ZInputAction_Digital->AddDetour(this, &FreeCamMotion::ZInputAction_Digital);
    Hooks::ZEntitySceneContext_LoadScene->AddDetour(this, &FreeCamMotion::OnLoadScene);
    Hooks::ZEntitySceneContext_ClearScene->AddDetour(this, &FreeCamMotion::OnClearScene);
}

void FreeCamMotion::OnEngineInitialized()
{
    const ZMemberDelegate<FreeCamMotion, void(const SGameUpdateEvent&)> s_Delegate(this, &FreeCamMotion::OnFrameUpdate);
    Globals::GameLoopManager->RegisterFrameUpdate(s_Delegate, 1, EUpdateMode::eUpdatePlayMode);
}

void FreeCamMotion::OnFrameUpdate(const SGameUpdateEvent& p_UpdateEvent)
{
    if (!*Globals::ApplicationEngineWin32)
        return;

    if (!(*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01.m_pInterfaceRef)
    {
        Logger::Debug("Creating free camera.");
        Functions::ZEngineAppCommon_CreateFreeCamera->Call(&(*Globals::ApplicationEngineWin32)->m_pEngineAppCommon);

        // If freecam was active we need to toggle.
        // This can happen after level restarts / changes.
        if (m_FreeCamActive)
            m_ShouldToggle = true;
    }

    (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCameraControl01.m_pInterfaceRef->SetActive(m_FreeCamActive);

    if (Functions::ZInputAction_Digital->Call(&m_ToggleFreeCamAction, -1))
    {
        ToggleFreecam();
    }

    if (m_ShouldToggle)
    {
        m_ShouldToggle = false;

        if (m_FreeCamActive)
            EnableFreecam();
        else
            DisableFreecam();
    }

    if (m_FreeCamActive)
    {
        // Determine freecam freeze status
        if (Functions::ZInputAction_Digital->Call(&m_FreezeFreeCamActionKb, -1))
            m_FreeCamFrozen = !m_FreeCamFrozen;

        const bool s_FreezeFreeCam = Functions::ZInputAction_Digital->Call(&m_FreezeFreeCamActionGc, -1) || m_FreeCamFrozen;

        (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCameraControl01.m_pInterfaceRef->m_bFreezeCamera = s_FreezeFreeCam;

        // Update camera move if active
        if (m_CamMoveActive)
        {
            // TODO: User-configured dynamic move speed
            m_CamMoveProgress += 0.2f * p_UpdateEvent.m_GameTimeDelta.ToSeconds();

            if (m_CamMoveProgress > 1.0f)
            {
                EndCameraMove();
                m_CamMoveProgress = 1.0f;
            }

            CamMoveUpdate(m_CamMoveProgress);
        }

        // While freecam is active, only enable hitman input when the "freeze camera" button is pressed.
        TEntityRef<ZHitman5> s_LocalHitman;
        Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);

        if (s_LocalHitman)
        {
            auto* s_InputControl = Functions::ZHM5InputManager_GetInputControlForLocalPlayer->Call(Globals::InputManager);

            if (s_InputControl)
                s_InputControl->m_bActive = s_FreezeFreeCam;
        }
    }
}

void FreeCamMotion::SetCameraDestination()
{
    auto s_CameraTrans = (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01.m_pInterfaceRef->GetWorldMatrix();

    m_CamDestPosition = s_CameraTrans.Trans;

    m_CamDestOrientation[0] = atan2f(s_CameraTrans.YAxis.z, s_CameraTrans.ZAxis.z);
    //m_CamDestOrientation[1] = atan2f(-s_CameraTrans.XAxis.z, sqrtf(s_CameraTrans.YAxis.z * s_CameraTrans.YAxis.z + s_CameraTrans.ZAxis.z * s_CameraTrans.ZAxis.z));
    m_CamDestOrientation[1] = 0.f;
    m_CamDestOrientation[2] = atan2f(s_CameraTrans.XAxis.y, s_CameraTrans.XAxis.x);

    m_CamDestinationSet = true;
}

void FreeCamMotion::SetCameraStart()
{
    auto s_CameraTrans = (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01.m_pInterfaceRef->GetWorldMatrix();

    m_CamStartPosition = s_CameraTrans.Trans;

    m_CamStartOrientation[0] = atan2f(s_CameraTrans.YAxis.z, s_CameraTrans.ZAxis.z);
    m_CamStartOrientation[1] = 0.f;
    m_CamStartOrientation[2] = atan2f(s_CameraTrans.XAxis.y, s_CameraTrans.XAxis.x);
}

void FreeCamMotion::BeginCameraMove()
{
    if (!m_CamDestinationSet)
        return;

    SetCameraStart();

    m_CamMovePosDelta = m_CamDestPosition - m_CamStartPosition;
    m_CamMoveOrientationDelta[0] = m_CamDestOrientation[0] - m_CamStartOrientation[0];

    // TODO: Properly determine yaw angle difference
    m_CamMoveOrientationDelta[2] = m_CamDestOrientation[2] - m_CamStartOrientation[2];

    m_CamMoveProgress = 0.f;
    m_CamMoveActive = true;
}

void FreeCamMotion::EndCameraMove()
{
    m_CamMoveActive = false;
}

void FreeCamMotion::CamMoveUpdate(float p_Progress)
{
    SMatrix s_CameraTrans;

    // Interpolate camera translation (position)
    s_CameraTrans.Trans = m_CamStartPosition + (m_CamMovePosDelta * p_Progress);

    // Calculate X rotaion matrix from pitch
    float s_Pitch = m_CamStartOrientation[0] + (m_CamMoveOrientationDelta[0] * p_Progress);
    SMatrix s_RotationX;

    s_RotationX.YAxis.y = cos(s_Pitch);
    s_RotationX.YAxis.z = sin(s_Pitch);

    s_RotationX.ZAxis.y = -sin(s_Pitch);
    s_RotationX.ZAxis.z = cos(s_Pitch);

    // Calculate Z rotation matrix from yaw
    float s_Yaw = m_CamStartOrientation[2] + (m_CamMoveOrientationDelta[2] * p_Progress);
    SMatrix s_RotationZ;

    s_RotationZ.XAxis.x = cos(s_Yaw);
    s_RotationZ.XAxis.y = sin(s_Yaw);

    s_RotationZ.YAxis.x = -sin(s_Yaw);
    s_RotationZ.YAxis.y = cos(s_Yaw);

    // Multiply X rotation and Z rotation matrices
    s_CameraTrans.XAxis = s_RotationZ * s_RotationX.XAxis;
    s_CameraTrans.YAxis = s_RotationZ * s_RotationX.YAxis;
    s_CameraTrans.ZAxis = s_RotationZ * s_RotationX.ZAxis;

    // Apply matrix to Freecam
    (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01.m_pInterfaceRef->SetWorldMatrix(s_CameraTrans);
}

void FreeCamMotion::CamJumpToDestination()
{
    if (!m_CamDestinationSet || m_CamMoveActive)
        return;

    SMatrix s_CameraTrans;

    // Copy translation to camera matrix
    s_CameraTrans.Trans = m_CamDestPosition;

    // Calculate X rotaion matrix from pitch
    SMatrix s_RotationX;

    s_RotationX.YAxis.y = cos(m_CamDestOrientation[0]);
    s_RotationX.YAxis.z = sin(m_CamDestOrientation[0]);

    s_RotationX.ZAxis.y = -sin(m_CamDestOrientation[0]);
    s_RotationX.ZAxis.z = cos(m_CamDestOrientation[0]);

    // Calculate Z rotation matrix from yaw
    SMatrix s_RotationZ;

    s_RotationZ.XAxis.x = cos(m_CamDestOrientation[2]);
    s_RotationZ.XAxis.y = sin(m_CamDestOrientation[2]);

    s_RotationZ.YAxis.x = -sin(m_CamDestOrientation[2]);
    s_RotationZ.YAxis.y = cos(m_CamDestOrientation[2]);

    // Multiply X rotation and Z rotation matrices
    s_CameraTrans.XAxis = s_RotationZ * s_RotationX.XAxis;
    s_CameraTrans.YAxis = s_RotationZ * s_RotationX.YAxis;
    s_CameraTrans.ZAxis = s_RotationZ * s_RotationX.ZAxis;

    // Apply matrix to Freecam
    (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01.m_pInterfaceRef->SetWorldMatrix(s_CameraTrans);
}

void FreeCamMotion::OnDrawMenu()
{
    bool s_FreeCamActive = m_FreeCamActive;
    if (ImGui::Checkbox(ICON_MD_PHOTO_CAMERA " FREECAM", &s_FreeCamActive))
    {
        ToggleFreecam();
    }

    if (ImGui::Button(ICON_MD_SPORTS_ESPORTS " FREECAM CONTROLS"))
        m_ControlsVisible = !m_ControlsVisible;

    if (ImGui::Button("Set Cam Destination"))
        if (m_FreeCamActive)
            SetCameraDestination();

    if (ImGui::Button("Begin Camera Move"))
        if (m_FreeCamActive)
            BeginCameraMove();

    if (ImGui::Button("Jump to Cam Destination"))
        if (m_FreeCamActive)
            CamJumpToDestination();
}

void FreeCamMotion::ToggleFreecam()
{
    m_FreeCamActive = !m_FreeCamActive;
    m_ShouldToggle = true;
    m_HasToggledFreecamBefore = true;
}

void FreeCamMotion::EnableFreecam()
{
    auto s_Camera = (*Globals::ApplicationEngineWin32)->m_pEngineAppCommon.m_pFreeCamera01;

    TEntityRef<IRenderDestinationEntity> s_RenderDest;
    Functions::ZCameraManager_GetActiveRenderDestinationEntity->Call(Globals::CameraManager, &s_RenderDest);

    m_OriginalCam = *s_RenderDest.m_pInterfaceRef->GetSource();

    const auto s_CurrentCamera = Functions::GetCurrentCamera->Call();

    // testing
    auto s_CameraTrans = s_CurrentCamera->GetWorldMatrix();
    //s_CameraTrans.Trans += s_CameraTrans.Up * -0.1f;

    s_Camera.m_pInterfaceRef->SetWorldMatrix(s_CameraTrans);

    Logger::Debug("Camera trans: {}", fmt::ptr(&s_Camera.m_pInterfaceRef->m_mTransform.Trans));

    s_RenderDest.m_pInterfaceRef->SetSource(&s_Camera.m_ref);
}

void FreeCamMotion::DisableFreecam()
{
    TEntityRef<IRenderDestinationEntity> s_RenderDest;
    Functions::ZCameraManager_GetActiveRenderDestinationEntity->Call(Globals::CameraManager, &s_RenderDest);

    s_RenderDest.m_pInterfaceRef->SetSource(&m_OriginalCam);

    // Enable Hitman input.
    TEntityRef<ZHitman5> s_LocalHitman;
    Functions::ZPlayerRegistry_GetLocalPlayer->Call(Globals::PlayerRegistry, &s_LocalHitman);

    if (s_LocalHitman)
    {
        auto* s_InputControl = Functions::ZHM5InputManager_GetInputControlForLocalPlayer->Call(Globals::InputManager);

        if (s_InputControl)
        {
            Logger::Debug("Got local hitman entity and input control! Enabling input. {} {}", fmt::ptr(s_InputControl), fmt::ptr(s_LocalHitman.m_pInterfaceRef));
            s_InputControl->m_bActive = true;
        }
    }
}

void FreeCamMotion::OnDrawUI(bool p_HasFocus)
{
    if (m_ControlsVisible)
    {
        ImGui::PushFont(SDK()->GetImGuiBlackFont());
        const auto s_ControlsExpanded = ImGui::Begin(ICON_MD_PHOTO_CAMERA " FreeCam Controls", &m_ControlsVisible);
        ImGui::PushFont(SDK()->GetImGuiRegularFont());

        if (s_ControlsExpanded)
        {
            ImGui::TextUnformatted("PC Controls");

            ImGui::BeginTable("FreeCamControlsPc", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);

            for (auto& [s_Key, s_Description] : m_PcControls)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(s_Key.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(s_Description.c_str());
            }

            ImGui::EndTable();

            ImGui::TextUnformatted("Controller Controls");

            ImGui::BeginTable("FreeCamControlsController", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);

            for (auto& [s_Key, s_Description] : m_ControllerControls)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(s_Key.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(s_Description.c_str());
            }

            ImGui::EndTable();
        }

        ImGui::PopFont();
        ImGui::End();
        ImGui::PopFont();
    }
}

DEFINE_PLUGIN_DETOUR(FreeCamMotion, bool, ZInputAction_Digital, ZInputAction* th, int a2)
{
    if (!m_FreeCamActive)
        return HookResult<bool>(HookAction::Continue());

    if (strcmp(th->m_szName, "ActivateGameControl0") == 0 && m_FreeCamFrozen)
        return HookResult(HookAction::Return(), true);

    return HookResult<bool>(HookAction::Continue());
}

DEFINE_PLUGIN_DETOUR(FreeCamMotion, void, OnLoadScene, ZEntitySceneContext* th, ZSceneData&)
{
    if (m_FreeCamActive)
        DisableFreecam();

    m_FreeCamActive = false;
    m_ShouldToggle = false;

    return HookResult<void>(HookAction::Continue());
}

DEFINE_PLUGIN_DETOUR(FreeCamMotion, void, OnClearScene, ZEntitySceneContext* th, bool)
{
    if (m_FreeCamActive)
        DisableFreecam();

    m_FreeCamActive = false;
    m_ShouldToggle = false;

    return HookResult<void>(HookAction::Continue());
}

DEFINE_ZHM_PLUGIN(FreeCamMotion);
