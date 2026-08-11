// MM game-side VR glue: pad merge, stereo routing gates, and the VR item wheel.
// C-linkage so BenPort (C++) and game C code can both consume it. The OSContPad-taking
// entry point (VrGame_MergePad) is declared at its call site in padmgr.c instead of here,
// because OSContPad's anonymous-struct typedef cannot be forward-declared.
#ifndef MM_VR_GAME_H
#define MM_VR_GAME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stereo routing: Active = render per-eye this frame (VR live, not Theater, real gameplay -
// title/file select ride the flat panel like the donor ports). Eligible = a play state exists.
bool VrGame_StereoActive(void);
bool VrGame_StereoEligible(void);

// The stick-click item wheel (right-stick click opens; stick highlights; click/trigger equips
// the highlighted item or mask to C-Left; grips page through the four inventory grids).
bool VrGameWheel_Visible(void);
void VrGameWheel_DrawImGui(void);

#ifdef __cplusplus
}
#endif

#endif // MM_VR_GAME_H
