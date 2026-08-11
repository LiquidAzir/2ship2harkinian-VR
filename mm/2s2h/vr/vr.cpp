// OpenXR VR layer for 2 Ship 2 Harkinian (Majora's Mask). Ported from RaYRoD-TV/BanjoKazooie-VR.
//
// The real OpenXR implementation (session, swapchains, per-eye matrices, submit) lands with Phase 2
// of SPEC.md. Until then ENABLE_VR must stay off and this file provides the stub block only, so the
// flat game links unchanged against the mm-vr libultraship (whose Fast3D interpreter calls the
// vr_* seam unconditionally and gates everything on vr_is_active() at runtime).
//
// Unlike the donor, the stub block below covers the ENTIRE vr.h surface - including vr_fog_mode /
// vr_fog_linear_coeffs / vr_sky_persp_match, which the donor only defines inside its ENABLE_VR
// section even though libultraship links against them unconditionally (its ENABLE_VR=OFF build does
// not link; ours does).
#include "vr.h"

#if defined(ENABLE_VR) && defined(_WIN32)

#error "The MM OpenXR layer is not yet ported (SPEC.md Phase 2). Build with ENABLE_VR=OFF."

#else // no VR layer (non-Windows, or ENABLE_VR off) - stubs so the flat build links unchanged.

extern "C" void vr_request_enable(void) {}
extern "C" bool vr_is_requested(void) { return false; }
extern "C" bool vr_headset_present(void) { return false; }
extern "C" bool vr_is_active(void) { return false; }
extern "C" bool vr_passthrough_supported(void) { return false; }
extern "C" bool vr_passthrough_active(void) { return false; }
extern "C" int  vr_display_refresh_hz(void) { return 0; }

extern "C" void vr_begin_frame(void) {}
extern "C" int  vr_eye_count(void) { return 0; }
extern "C" int  vr_eye_width(int e)  { (void)e; return 0; }
extern "C" int  vr_eye_height(int e) { (void)e; return 0; }
extern "C" const float* vr_eye_viewproj(int e) { (void)e; return 0; }
extern "C" const float* vr_sky_viewproj(int e) { (void)e; return 0; }
extern "C" bool vr_sky_persp_match(const void* mtxAddr) { (void)mtxAddr; return false; }
extern "C" void vr_set_sky_fov(float a, float b) { (void)a; (void)b; }
extern "C" float vr_sky_fov_h(void) { return 0.0f; }
extern "C" float vr_sky_fov_v(void) { return 0.0f; }
extern "C" float vr_sky_decouple_rad(void) { return 0.0f; }
extern "C" bool vr_sky_dome_active(void) { return false; }
extern "C" bool vr_sky_remap_active(void) { return false; }
extern "C" void vr_set_sky_camera(const float e[3], const float a[3], const float u[3]) { (void)e; (void)a; (void)u; }
extern "C" void vr_set_focus_distance(float gameUnits) { (void)gameUnits; }

extern "C" bool vr_submit_eye_texture(int e, unsigned int t, int w, int h) { (void)e; (void)t; (void)w; (void)h; return false; }
extern "C" bool vr_submit_panel_texture(unsigned int t, int w, int h) { (void)t; (void)w; (void)h; return false; }
extern "C" bool vr_present_desktop_panel(int w, int h) { (void)w; (void)h; return false; }
extern "C" void vr_menu_render_begin(int w, int h) { (void)w; (void)h; }
extern "C" void vr_menu_render_present(int w, int h) { (void)w; (void)h; }
extern "C" void vr_menu_mirror_desktop(int w, int h) { (void)w; (void)h; }
extern "C" void vr_mirror_game_desktop(unsigned int t, int sw, int sh, int dw, int dh, int c) { (void)t; (void)sw; (void)sh; (void)dw; (void)dh; (void)c; }
extern "C" void vr_menu_apply_opacity(void) {}

extern "C" int  vr_overlay_width(void) { return 0; }
extern "C" int  vr_overlay_height(void) { return 0; }
extern "C" bool vr_begin_overlay(bool s) { (void)s; return false; }
extern "C" void vr_end_overlay(bool s) { (void)s; }
extern "C" void vr_set_panel_mode(bool on) { (void)on; }
extern "C" bool vr_begin_panel(void) { return false; }
extern "C" void vr_end_panel(void) {}
extern "C" void vr_submit(void) {}

extern "C" int   vr_get_view_mode(void) { return 0; }   extern "C" void vr_set_view_mode(int m) { (void)m; }
extern "C" float vr_fp_forward_game_units(void) { return 0.0f; }
// Stock fog untouched: 2 is the contract's "outside VR" answer. Returning 0 here would strip the
// flat game's fog - the one stub whose value is load-bearing.
extern "C" int  vr_fog_mode(void) { return 2; }
extern "C" void vr_fog_linear_coeffs(float* mul, float* off) { if (mul) *mul = 0.0f; if (off) *off = 0.0f; }
extern "C" float vr_get_world_scale(void) { return 0; } extern "C" void vr_set_world_scale(float v) { (void)v; }
extern "C" float vr_get_stereo(void) { return 0; }      extern "C" void vr_set_stereo(float v) { (void)v; }
extern "C" float vr_get_head_scale(void) { return 0; }  extern "C" void vr_set_head_scale(float v) { (void)v; }
extern "C" float vr_get_eye_height(void) { return 0; }  extern "C" void vr_set_eye_height(float v) { (void)v; }
extern "C" float vr_get_menu_dist(void) { return 0; }   extern "C" void vr_set_menu_dist(float v) { (void)v; }
extern "C" float vr_get_menu_size(void) { return 0; }   extern "C" void vr_set_menu_size(float v) { (void)v; }
extern "C" float vr_get_hud_scale(void) { return 0; }   extern "C" void vr_set_hud_scale(float v) { (void)v; }
extern "C" float vr_get_hud_dist(void)  { return 0; }   extern "C" void vr_set_hud_dist(float v) { (void)v; }
extern "C" void  vr_set_hud_menu_mode(int on) { (void)on; }
extern "C" void  vr_get_head_offset_m(float out[3]) { out[0] = out[1] = out[2] = 0.0f; }
extern "C" const float* vr_hud_viewproj(int e) { (void)e; return 0; }
extern "C" const float* vr_hud_viewproj_flat(int e) { (void)e; return 0; }
extern "C" const float* vr_full2d_viewproj(int e) { (void)e; return 0; }

extern "C" void  vr_reset_defaults(void) {}
extern "C" float vr_head_yaw_rad(void) { return 0; }
extern "C" float vr_head_pitch_rad(void) { return 0; }

extern "C" bool  vr_controllers_active(void) { return false; }
extern "C" unsigned vr_controller_buttons(void) { return 0; }
extern "C" void  vr_controller_stick(int hand, float out[2]) { (void)hand; out[0] = out[1] = 0.0f; }
extern "C" void  vr_controller_rumble(float strength, float seconds) { (void)strength; (void)seconds; }
extern "C" void  vr_controller_rumble_stop(void) {}

extern "C" void  vr_recenter(void) {}
extern "C" void  vr_set_flip_angle(float radians) { (void)radians; }
extern "C" void  vr_set_fp_framing(int on) { (void)on; }
extern "C" void  vr_shutdown(void) {}

extern "C" void vr_debug_synth_matrices(int eye, float e[16], float s[16], float h[16], float hf[16], float f[16]) {
    (void)eye; (void)e; (void)s; (void)h; (void)hf; (void)f;
}
extern "C" int  vr_debug_dump_texture(unsigned int t, int w, int h, const char* p) { (void)t; (void)w; (void)h; (void)p; return 0; }

#endif
