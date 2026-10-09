/* Asset-free fallback: draw the original intro from the user's pack.
 * This replaces the reference's re-authored bitmap renderer only. */
static CtrIntroActor sNativeIntroActors[64];
static unsigned sNativeIntroCount, sNativeIntroScene;
void CtrIntro_SetScene(unsigned scene) { sNativeIntroScene = scene; }
unsigned CtrIntro_GetScene(void) { return sNativeIntroScene; }
bool CtrIntroNative_Active(void) { return false; }
static bool NativeIntroActive(void) { return false; }
static bool NativeIntroCinematic(void) { return false; }
static bool NativeIntroTitle(void) { return false; }
static void NativeIntroRelease(void) { }
static void NativeIntroPrepare(void) { sNativeIntroCount = 0; }
static bool DrawNativeIntroBg(unsigned bg) { (void)bg; return false; }
static void DrawNativeIntroObjects(unsigned priority, bool effects) { (void)priority; (void)effects; }
static void NativeIntroObjWindow(void) { }
