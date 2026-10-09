#ifndef CTR_STORAGE_BRIDGE_H
#define CTR_STORAGE_BRIDGE_H
/* Read-only FireRed controller snapshots. No Pokemon/save layouts cross here. */
int CtrBottomStorage_PCMenu(void);
int CtrBottomStorage_PCMenuReady(void);
int CtrBottomStorage_PCMenuCursor(void);
int CtrBottomStorage_PCMenuCount(void);
int CtrBottomStorage_BoxOpen(void);
int CtrBottomStorage_BoxReady(void);
int CtrBottomStorage_CursorArea(void);
int CtrBottomStorage_CursorPos(void);
int CtrBottomStorage_MultiMove(void);
int CtrBottomStorage_ContextMenu(void);
int CtrBottomStorage_ContextCount(void);
int CtrBottomStorage_ContextCursor(void);
int CtrBottomStorage_ContextX(void);
int CtrBottomStorage_ContextY(void);
int CtrBottomStorage_ContextW(void);
int CtrBottomStorage_BoxChoose(void);
int CtrBottomStorage_YesNo(void);
int CtrBottomStorage_InputContext(void);
int CtrBottomStorage_PCInputContext(void);
int CtrBottomStorage_BoxPresented(void);
int CtrBottomStorage_NamingPresented(void);
#endif
