// MM game-side VR glue (SPEC.md Phases 5-6, first slice): motion-controller pad merge and the
// stick-click item wheel, in the shape of BanjoKazooie-VR's VrGame.cpp adapted to MM's systems.
//
// Pad merge: OpenXR controller state (vr.cpp's action set) is OR-ed into N64 pad 0 inside the
// padmgr read, ahead of the game's edge detection, so motion controllers drive gameplay and every
// menu exactly like a gamepad - and a real gamepad keeps working alongside.
//
// Item wheel (the interaction the OoT VR mod shipped): click the RIGHT stick in gameplay to open a
// radial of the current inventory page, deflect the stick to highlight a slot, click again (or
// right trigger) to equip it to C-Left; either grip cycles Items 1 / Items 2 / Masks 1 / Masks 2.
// Sector order matches the pause-menu grid (rows of 6, read left to right), so pause-screen muscle
// memory carries over. The wheel draws through ImGui's foreground list; BenPort presents it on the
// head-locked VR menu panel so it floats over the live stereo world.
#include "vr.h"
#include "VrGame.h"

#if defined(ENABLE_VR) && defined(_WIN32)

#include <math.h>
#include <libultraship/bridge.h>
#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <fast/Fast3dGui.h> // GetTextureByName lives on the Fast3D Gui subclass
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

// ---- stereo routing gates ---------------------------------------------------------------------

extern "C" bool VrGame_StereoEligible(void) {
    return gPlayState != NULL;
}

extern "C" bool VrGame_StereoActive(void) {
    // Theater is the flat-panel comfort mode; title/file select (no play state) ride the panel
    // too, exactly like the donor ports route their non-gameplay screens.
    return vr_is_active() && vr_get_view_mode() != VR_VIEW_THEATER && VrGame_StereoEligible();
}

// ---- item wheel -------------------------------------------------------------------------------

namespace {

constexpr int kWheelSectors = 12; // one inventory grid page (two rows of 6) per wheel
constexpr int kWheelPages = 4;    // Items 1, Items 2, Masks 1, Masks 2

bool sWheelOpen = false;
int sWheelPage = 0;
int sWheelHover = -1;
unsigned sPrevTickVb = 0;

const char* WheelPageName(int page) {
    switch (page & 3) {
        case 0: return "Items 1";
        case 1: return "Items 2";
        case 2: return "Masks 1";
        default: return "Masks 2";
    }
}

u8 WheelSlotItem(int page, int sector) {
    const int slot = (page & 3) * kWheelSectors + sector; // 0..47 spans items then masks
    return gSaveContext.save.saveInfo.inventory.items[slot];
}

void WheelEquipHovered() {
    if (sWheelHover < 0) {
        return;
    }
    const int slot = (sWheelPage & 3) * kWheelSectors + sWheelHover;
    const u8 item = gSaveContext.save.saveInfo.inventory.items[slot];
    if (item == ITEM_NONE) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    // Already on a C button: it is one press away - just confirm, no re-equip shuffle.
    for (s32 btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if (BUTTON_ITEM_EQUIP(0, btn) == item) {
            Audio_PlaySfx(NA_SE_SY_DECIDE);
            vr_controller_rumble(0.3f, 0.04f);
            return;
        }
    }
    // Same writes the pause menu's equip performs (see Enhancements/Equipment/ItemUnequip.cpp for
    // the mirrored unequip): the button item, WHICH inventory slot it points at (mask slots carry
    // their >= ITEM_NUM_SLOTS index), the button's enabled state, and the HUD icon reload.
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT) = item;
    C_SLOT_EQUIP(0, EQUIP_SLOT_C_LEFT) = slot;
    gSaveContext.buttonStatus[EQUIP_SLOT_C_LEFT] = BTN_ENABLED;
    Interface_LoadItemIconImpl(gPlayState, EQUIP_SLOT_C_LEFT);
    Audio_PlaySfx(NA_SE_SY_DECIDE);
    vr_controller_rumble(0.35f, 0.05f);
}

// One tick of wheel input, on the game state update (game thread - the equip writes touch
// gSaveContext and the interface, which must never race the game).
void WheelTick() {
    if (!vr_is_active() || !vr_controllers_active() || gPlayState == NULL ||
        gPlayState->pauseCtx.state != PAUSE_STATE_OFF || CVarGetInteger("gVRMotionControls", 1) == 0) {
        sWheelOpen = false;
        sPrevTickVb = vr_controller_buttons();
        return;
    }
    const unsigned vb = vr_controller_buttons();
    const unsigned edge = vb & ~sPrevTickVb;
    sPrevTickVb = vb;

    if (edge & VR_BTN_RSTICK) {
        if (!sWheelOpen) {
            sWheelOpen = true;
            sWheelHover = -1;
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        } else {
            WheelEquipHovered(); // no hover = plain close
            sWheelOpen = false;
        }
        return;
    }
    if (!sWheelOpen) {
        return;
    }
    if (edge & (VR_BTN_LGRIP | VR_BTN_RGRIP)) {
        sWheelPage = (sWheelPage + 1) & 3;
        sWheelHover = -1;
        Audio_PlaySfx(NA_SE_SY_CURSOR);
    }
    if ((edge & VR_BTN_RTRIGGER) && sWheelHover >= 0) {
        WheelEquipHovered();
        sWheelOpen = false;
        return;
    }
    float st[2];
    vr_controller_stick(1, st);
    const float mag = sqrtf(st[0] * st[0] + st[1] * st[1]);
    if (mag > 0.4f) {
        // Sector 0 sits at 12 o'clock, clockwise - atan2(x, y) is 0 at up, positive to the right.
        float ang = atan2f(st[0], st[1]);
        if (ang < 0.0f) {
            ang += 2.0f * (float)M_PI;
        }
        int idx = (int)floorf(ang / (2.0f * (float)M_PI) * kWheelSectors + 0.5f) % kWheelSectors;
        if (idx != sWheelHover) {
            sWheelHover = idx;
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        }
    }
}

// ---- gesture combat + aimed items -------------------------------------------------------------
//
// Sword swipe: a fast right-hand movement presses B for a couple of ticks - MM's sword is
// animation-driven with authored arcs, so the gesture TRIGGERS the swing rather than tracing it
// (the OoT VR mod's approach). Shield: holding the left hand up near head height holds R, with
// hysteresis so a wandering hand doesn't flicker the guard. Aimed items: while the player is
// first-person aiming a projectile (func_800B7128: bow, hookshot, Zora boomerang, Deku bubble),
// the right hand's angular velocity is synthesized into aim-stick input - rotate the hand and the
// reticle tracks it 1:1 (gyro-style), through the game's own aim integration, clamps and camera
// modes, for every aimable item uniformly.
static int sSwingTicks = 0;    // press B while > 0
static int sSwingCooldown = 0; // ticks until the next swipe may fire
static bool sShieldHold = false;
static bool sAimActive = false;

static void GesturesTick() {
    if (sSwingCooldown > 0) {
        sSwingCooldown--;
    }
    if (sSwingTicks > 0) {
        sSwingTicks--;
    }
    if (!vr_is_active() || !vr_controllers_active() || gPlayState == NULL ||
        gPlayState->pauseCtx.state != PAUSE_STATE_OFF || CVarGetInteger("gVRMotionControls", 1) == 0) {
        sSwingTicks = 0;
        sShieldHold = false;
        sAimActive = false;
        return;
    }
    Player* player = GET_PLAYER(gPlayState);

    // Aimed items: state read here on the game thread; the stick synth itself runs in the pad
    // merge so it uses the freshest hand sample each input read.
    sAimActive = (player != NULL) && CVarGetInteger("gVRAimedItems", 1) != 0 && func_800B7128(player);

    float pos[3], lin[3];
    // Sword swipe, right hand: linear speed spike -> one B press. The cooldown keeps one physical
    // swipe from machine-gunning B across ticks; 1.7 m/s is brisk-but-not-violent (live-tunable).
    if (CVarGetInteger("gVRSwordSwing", 1) != 0 && !sWheelOpen && !sAimActive &&
        vr_hand_state(1, NULL, NULL, lin, NULL)) {
        const float speed = sqrtf(lin[0] * lin[0] + lin[1] * lin[1] + lin[2] * lin[2]);
        if (speed > CVarGetFloat("gVRSwingSpeed", 1.7f) && sSwingCooldown == 0) {
            sSwingTicks = 2;
            sSwingCooldown = 10;
            vr_controller_rumble(0.5f, 0.06f);
        }
    }
    // Shield, left hand: raised toward head height holds R. Hysteresis band: raise above 30 cm
    // below the head, release only once clearly dropped past 42 cm - no flicker at the boundary.
    if (CVarGetInteger("gVRShieldRaise", 1) != 0 && vr_hand_state(0, pos, NULL, NULL, NULL)) {
        if (!sShieldHold && pos[1] > -0.30f) {
            sShieldHold = true;
            vr_controller_rumble(0.25f, 0.03f);
        } else if (sShieldHold && pos[1] < -0.42f) {
            sShieldHold = false;
        }
    } else {
        sShieldHold = false;
    }
}

} // namespace

extern "C" bool VrGameWheel_Visible(void) {
    return sWheelOpen;
}

extern "C" void VrGameWheel_DrawImGui(void) {
    if (!sWheelOpen) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    const float R = 0.30f * (io.DisplaySize.x < io.DisplaySize.y ? io.DisplaySize.x : io.DisplaySize.y);
    const float ringR = R * 0.78f;
    const float slotR = R * 0.155f;
    const float iconHalf = slotR * 0.82f;

    dl->AddCircleFilled(c, R * 1.18f, IM_COL32(8, 8, 14, 208), 64);
    dl->AddCircle(c, R * 1.18f, IM_COL32(210, 180, 90, 160), 64, 2.5f);

    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (gui == nullptr) {
        return;
    }
    for (int i = 0; i < kWheelSectors; i++) {
        const float ang = (float)i / kWheelSectors * 2.0f * (float)M_PI; // 0 = up, clockwise
        const ImVec2 p(c.x + sinf(ang) * ringR, c.y - cosf(ang) * ringR);
        const bool hovered = (i == sWheelHover);
        const u8 item = WheelSlotItem(sWheelPage, i);
        dl->AddCircleFilled(p, slotR, hovered ? IM_COL32(235, 200, 90, 235) : IM_COL32(30, 30, 44, 220), 32);
        dl->AddCircle(p, slotR, IM_COL32(120, 120, 150, 180), 32, 1.5f);
        if (item != ITEM_NONE && item < 131) {
            // TexturePtr is the O2R resource path under 2s2h; the Gui texture registry resolves it
            // to an ImGui texture exactly the way the Save Editor's inventory grid does.
            ImTextureID tex = gui->GetTextureByName((const char*)gItemIcons[item]);
            if (tex) {
                dl->AddImage(tex, ImVec2(p.x - iconHalf, p.y - iconHalf), ImVec2(p.x + iconHalf, p.y + iconHalf));
            }
        } else if (hovered) {
            dl->AddCircleFilled(p, slotR * 0.25f, IM_COL32(90, 90, 110, 200), 16);
        }
    }

    // Center: hovered item large, page name, and the controls line.
    if (sWheelHover >= 0) {
        const u8 item = WheelSlotItem(sWheelPage, sWheelHover);
        if (item != ITEM_NONE && item < 131) {
            ImTextureID tex = gui->GetTextureByName((const char*)gItemIcons[item]);
            if (tex) {
                const float big = slotR * 1.6f;
                dl->AddImage(tex, ImVec2(c.x - big, c.y - big), ImVec2(c.x + big, c.y + big));
            }
        }
    }
    const char* page = WheelPageName(sWheelPage);
    const ImVec2 pageSz = ImGui::CalcTextSize(page);
    dl->AddText(ImVec2(c.x - pageSz.x * 0.5f, c.y - R * 1.02f), IM_COL32(235, 210, 140, 255), page);
    const char* hint = "Stick: choose   Click / Trigger: equip C-Left   Grip: page";
    const ImVec2 hintSz = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(c.x - hintSz.x * 0.5f, c.y + R * 1.00f), IM_COL32(200, 200, 210, 235), hint);
}

// ---- pad merge --------------------------------------------------------------------------------

extern "C" void VrGame_MergePad(OSContPad* pad) {
    if (pad == NULL || !vr_controllers_active() || CVarGetInteger("gVRMotionControls", 1) == 0) {
        return;
    }
    auto gui = Ship::Context::GetRawInstance()->GetWindow()->GetGui();

    const unsigned vb = vr_controller_buttons();
    static unsigned sPrevPadVb = 0;
    const unsigned edge = vb & ~sPrevPadVb;
    sPrevPadVb = vb;

    // L3 toggles the port's ImGui menu, the donor's shortcut, so settings are reachable from the
    // headset without a keyboard.
    if ((edge & VR_BTN_LSTICK) && gui != nullptr && gui->GetMenu() != nullptr) {
        gui->GetMenu()->ToggleVisibility();
    }
    // While the ImGui menu is open the sticks belong to it, not the game - merging them walked
    // Link around behind the menu in the donor's first build.
    if (gui != nullptr && gui->GetMenuOrMenubarVisible()) {
        return;
    }

    float ls[2], rs[2];
    vr_controller_stick(0, ls);
    vr_controller_stick(1, rs);

    // Aimed items: while first-person aiming a projectile, the RIGHT HAND owns the aim stick.
    // Angular velocity becomes stick deflection (gyro-style rate control tuned near 1:1), driven
    // through the game's own aim integration so clamps, inversion settings and camera modes all
    // keep working - and it covers bow, hookshot, Zora boomerang and Deku bubble uniformly. The
    // flat stick's aim role is REPLACED here (it would fight the hand); movement is parked while
    // aiming anyway.
    const float dead = 0.12f;
    float av[3];
    if (sAimActive && !sWheelOpen && vr_hand_state(1, NULL, NULL, NULL, av)) {
        const float gain = 28.0f * CVarGetFloat("gVRAimGain", 1.0f);
        int ax = (int)lroundf(-av[1] * gain); // hand yaw-left -> stick left (MM: stick left aims left)
        int ay = (int)lroundf(av[0] * gain);  // hand pitch-up -> stick up
        pad->stick_x = (int8_t)(ax < -85 ? -85 : (ax > 85 ? 85 : ax));
        pad->stick_y = (int8_t)(ay < -85 ? -85 : (ay > 85 ? 85 : ay));
    } else {
        // Movement merges ADDITIVELY with any real gamepad, clamped to the N64 range.
        if (fabsf(ls[0]) > dead || fabsf(ls[1]) > dead) {
            int sx = pad->stick_x + (int)lroundf(ls[0] * 85.0f);
            int sy = pad->stick_y + (int)lroundf(ls[1] * 85.0f);
            pad->stick_x = (int8_t)(sx < -128 ? -128 : (sx > 127 ? 127 : sx));
            pad->stick_y = (int8_t)(sy < -128 ? -128 : (sy > 127 ? 127 : sy));
        }
    }

    // Gesture combat, decided on the game tick: a swipe presses B, a raised left hand holds R.
    if (sSwingTicks > 0) {
        pad->button |= BTN_B;
    }
    if (sShieldHold) {
        pad->button |= BTN_R;
    }

    if (sWheelOpen) {
        // The wheel owns the right stick and every action control; movement stays live so you can
        // keep walking while you pick.
        return;
    }

    // Right stick feeds the pad's right-stick fields for the port's own camera consumers (parked
    // while hand-aiming so the camera doesn't fight the aim).
    if (!sAimActive && (fabsf(rs[0]) > dead || fabsf(rs[1]) > dead)) {
        int rx = pad->right_stick_x + (int)lroundf(rs[0] * 85.0f);
        int ry = pad->right_stick_y + (int)lroundf(rs[1] * 85.0f);
        pad->right_stick_x = (int8_t)(rx < -128 ? -128 : (rx > 127 ? 127 : rx));
        pad->right_stick_y = (int8_t)(ry < -128 ? -128 : (ry > 127 ? 127 : ry));
    }

    // MM mapping (SPEC.md §6.1 adapted to MM's B-is-sword layout):
    //   right trigger -> B (sword)      A button      -> A (action/roll)
    //   left trigger  -> C-Left item    B button      -> C-Up (look / Tatl)
    //   X / Y         -> C-Down / C-Right items
    //   left grip     -> Z (target)     right grip    -> R (shield)
    //   menu          -> Start          R3            -> item wheel (handled above)
    u16 btn = 0;
    if (vb & VR_BTN_RTRIGGER) { btn |= BTN_B; }
    if (vb & VR_BTN_A)        { btn |= BTN_A; }
    if (vb & VR_BTN_B)        { btn |= BTN_CUP; }
    if (vb & VR_BTN_LTRIGGER) { btn |= BTN_CLEFT; }
    if (vb & VR_BTN_X)        { btn |= BTN_CDOWN; }
    if (vb & VR_BTN_Y)        { btn |= BTN_CRIGHT; }
    if (vb & VR_BTN_LGRIP)    { btn |= BTN_Z; }
    if (vb & VR_BTN_RGRIP)    { btn |= BTN_R; }
    if (vb & VR_BTN_MENU)     { btn |= BTN_START; }
    pad->button |= btn;
}

// ---- registration -----------------------------------------------------------------------------

static void RegisterVrGame() {
    COND_HOOK(OnGameStateUpdate, true, []() {
        WheelTick();
        GesturesTick();
    });
}

static RegisterShipInitFunc vrGameInit(RegisterVrGame, {});

#else // !ENABLE_VR or non-Windows: flat builds carry no VrGame - callers are all guarded too.

extern "C" bool VrGame_StereoEligible(void) { return false; }
extern "C" bool VrGame_StereoActive(void) { return false; }
extern "C" bool VrGameWheel_Visible(void) { return false; }
extern "C" void VrGameWheel_DrawImGui(void) {}

#endif
