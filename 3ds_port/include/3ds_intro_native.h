#ifndef CTR_INTRO_NATIVE_H
#define CTR_INTRO_NATIVE_H
#include <stdbool.h>
#include <stdint.h>
/* 0 (default): the intro and title draw FireRed's own sprites and tiles 1:1 on
 * the 400x240 canvas (sZoom 1.0, scenery composed around the 240x160 stage).
 * 1: the re-authored native intro assets (assets/intro_native) are used instead. */
#ifndef CTR_REAUTHORED_INTRO
#define CTR_REAUTHORED_INTRO 0
#endif
#if CTR_REAUTHORED_INTRO
#error Re-authored intro assets are excluded from this distribution
#endif
enum { CTR_INTRO_LEGACY, CTR_INTRO_GF, CTR_INTRO_SCENE1, CTR_INTRO_SCENE2,
       CTR_INTRO_SCENE3, CTR_INTRO_TITLE, CTR_INTRO_COPYRIGHT };
typedef struct {
    int16_t x, y, baseX, baseY, matrixA, matrixD;
    uint16_t tag, tileOffset;
    uint8_t priority, palette, anim, mode, subpriority;
    bool flipX, flipY;
} CtrIntroActor;
typedef struct { unsigned grassFrame, bgFrame; int grassY; } CtrIntroScene1Pose;
void CtrIntro_ReadScene1Pose(CtrIntroScene1Pose *pose);
typedef struct { int forestX, plantsX, gengarY, nidorinoY; bool closeup; } CtrIntroScene2Pose;
void CtrIntro_ReadScene2Pose(CtrIntroScene2Pose *pose);
typedef struct { unsigned gengarFrame; int gengarX, gengarY, forestX; } CtrIntroScene3Pose;
void CtrIntro_ReadScene3Pose(CtrIntroScene3Pose *pose);
int CtrIntro_ReadTitleFooterX(void);
void CtrIntro_SetScene(unsigned scene);
unsigned CtrIntro_GetScene(void);
bool CtrIntroNative_Active(void);
unsigned CtrIntro_ReadActors(CtrIntroActor *actors, unsigned capacity);
#endif
