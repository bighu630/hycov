
#include "globaleventhook.hpp"
#include "dispatchers.hpp"
#include <regex>
#include <set>
#include <hyprland/src/SharedDefs.hpp>
#include "OvGridLayout.hpp"
#include "src/desktop/Workspace.hpp"
#include "src/helpers/Monitor.hpp"
#include "src/layout/DwindleLayout.hpp"
#include "src/layout/MasterLayout.hpp"
#include "src/managers/input/InputManager.hpp"

// std::unique_ptr<HOOK_CALLBACK_FN> mouseMoveHookPtr = std::make_unique<HOOK_CALLBACK_FN>(mouseMoveHook);
// std::unique_ptr<HOOK_CALLBACK_FN> mouseButtonHookPtr = std::make_unique<HOOK_CALLBACK_FN>(mouseButtonHook);
typedef void (*origCWindow_onUnmap)(void*);
typedef void (*origStartAnim)(void*, bool in, bool left, bool instant);
typedef void (*origFullscreenActive)(std::string args);
typedef void (*origOnKeyboardKey)(void*, std::any e, SP<IKeyboard> pKeyboard);
typedef void (*origCInputManager_onMouseButton)(void*, IPointer::SButtonEvent e);
typedef void (*origCInputManager_mouseMoveUnified)(void* , uint32_t time, bool refocus);

static double gesture_dx,gesture_previous_dx;
static double gesture_dy,gesture_previous_dy;

std::string getKeynameFromKeycode(IKeyboard::SKeyEvent e, SP<IKeyboard> pKeyboard) {
  auto keyboard = pKeyboard.get();
  xkb_keycode_t keycode = e.keycode + 8;
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(keyboard->m_xkbState, keycode);
  char *tmp_keyname = new char[64];
  xkb_keysym_get_name(keysym, tmp_keyname, 64);
  std::string keyname = tmp_keyname;
  delete[] tmp_keyname;
  return keyname;
}

bool isKeyReleaseToggleExitOverviewHit(IKeyboard::SKeyEvent e, SP<IKeyboard> pKeyboard) {
  if (g_hycov_alt_replace_key == "")
    return false;

  if (isNumber(g_hycov_alt_replace_key) && std::stoi(g_hycov_alt_replace_key) > 9 && std::stoi(g_hycov_alt_replace_key) == (e.keycode + 8)) {
    return true;
  } else if (g_hycov_alt_replace_key.find("code:") == 0 && isNumber(g_hycov_alt_replace_key.substr(5)) && std::stoi(g_hycov_alt_replace_key.substr(5)) == (e.keycode + 8)) {
    return true;
  } else {
    std::string keyname = getKeynameFromKeycode(e,pKeyboard);
    if (keyname == g_hycov_alt_replace_key) {
      return true;
    }
  }

  return false;
}

static void toggle_hotarea(int x_root, int y_root)
{
  CMonitor *pMonitor = g_pCompositor->m_lastMonitor.get();

  if (g_hycov_hotarea_monitor != "all" && pMonitor->m_name!= g_hycov_hotarea_monitor)
    return;

  auto m_x = pMonitor->m_position.x;
  auto m_y = pMonitor->m_position.y;
  auto m_width = pMonitor->m_position.x;
  auto m_height = pMonitor->m_position.y;

  if (!g_hycov_isInHotArea &&
    ((g_hycov_hotarea_pos == 1 && x_root < (m_x + g_hycov_hotarea_size) && y_root > (m_y + m_height - g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 2 && x_root > (m_x + m_width - g_hycov_hotarea_size) && y_root > (m_y + m_height - g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 3 && x_root < (m_x + g_hycov_hotarea_size) && y_root < (m_y + g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 4 && x_root > (m_x + m_width - g_hycov_hotarea_size) && y_root < (m_y + g_hycov_hotarea_size))))
  {
    g_hycov_isInHotArea = true;
    hycov_log(LOG,"cursor enter hotarea");
    dispatch_toggleoverview("internalToggle");
  }
  else if (g_hycov_isInHotArea &&
    !((g_hycov_hotarea_pos == 1 && x_root < (m_x + g_hycov_hotarea_size) && y_root > (m_y + m_height - g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 2 && x_root > (m_x + m_width - g_hycov_hotarea_size) && y_root > (m_y + m_height - g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 3 && x_root < (m_x + g_hycov_hotarea_size) && y_root < (m_y + g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 4 && x_root > (m_x + m_width - g_hycov_hotarea_size) && y_root < (m_y + g_hycov_hotarea_size))))
  {
      g_hycov_isInHotArea = false;
  }
}

static void hkCInputManager_mouseMoveUnified(void* thisptr, uint32_t time, bool refocus)
{
  (*(origCInputManager_mouseMoveUnified)g_hycov_pCInputManager_mouseMoveUnifiedHook->m_original)(thisptr, time, refocus);

  Vector2D   mouseCoords        = g_pInputManager->getMouseCoordsInternal();
  const auto MOUSECOORDSFLOORED = mouseCoords.floor();

  toggle_hotarea(MOUSECOORDSFLOORED.x, MOUSECOORDSFLOORED.y);
}

static void hkCInputManager_onMouseButton(void* thisptr, IPointer::SButtonEvent e)
{
  if(g_hycov_isOverView && (e.button == BTN_LEFT || e.button == BTN_RIGHT) ) {

    if (g_hycov_click_in_cursor) {
        g_pInputManager->refocus();
    }

    if (!g_pCompositor->m_lastWindow.lock()) {
      return;
    }

    switch (e.button)
    {
    case BTN_LEFT:
      if (g_hycov_isOverView && e.state == WL_POINTER_BUTTON_STATE_PRESSED)
      {
        dispatch_toggleoverview("internalToggle");
        return;
      }
      break;
    case BTN_RIGHT:
      if (g_hycov_isOverView && e.state == WL_POINTER_BUTTON_STATE_PRESSED)
      {
        g_pCompositor->closeWindow(g_pCompositor->m_lastWindow.lock());
        return;
      }
      break;
    }
  } else {
    (*(origCInputManager_onMouseButton)g_hycov_pCInputManager_onMouseButtonHook->m_original)(thisptr, e);
  }
}


static void hkCWindow_onUnmap(void* thisptr) {
  // call the original function,Let it do what it should do
  (*(origCWindow_onUnmap)g_hycov_pCWindow_onUnmap->m_original)(thisptr);

  // after done original thing,The workspace automatically exit overview if no client exists
  auto nodeNumInSameMonitor = 0;
  auto nodeNumInSameWorkspace = 0;
	for (auto &n : g_hycov_OvGridLayout->m_lOvGridNodesData) {
		if(n.pWindow->monitorID() == g_pCompositor->m_lastMonitor->m_id && !g_pCompositor->isWorkspaceSpecial(n.workspaceID)) {
			nodeNumInSameMonitor++;
		}
		if(n.pWindow->m_workspace == g_pCompositor->m_lastMonitor->m_activeWorkspace) {
			nodeNumInSameWorkspace++;
		}
	}

  if (g_hycov_isOverView && nodeNumInSameMonitor == 0) {
    hycov_log(LOG,"no tiling window in same monitor,auto exit overview");
    dispatch_leaveoverview("");
    return;
  }

  if (g_hycov_isOverView && nodeNumInSameWorkspace == 0 && (g_hycov_only_active_workspace || g_hycov_force_display_only_current_workspace)) {
    hycov_log(LOG,"no tiling windwo in same workspace,auto exit overview");
    dispatch_leaveoverview("");
    return;
  }

}

static void hkChangeworkspace(std::string args) {
  // just log a message and do nothing, mean the original function is disabled
  hycov_log(LOG,"ChangeworkspaceHook hook toggle");
}

static void hkMoveActiveToWorkspace(std::string args) {
  // just log a message and do nothing, mean the original function is disabled
  hycov_log(LOG,"MoveActiveToWorkspace hook toggle");
}

static void hkSpawn(std::string args) {
  // just log a message and do nothing, mean the original function is disabled
  hycov_log(LOG,"Spawn hook toggle");
}

static void hkStartAnim(void* thisptr,bool in, bool left, bool instant = false) {
  // if is exiting overview, omit the animation of workspace change (instant = true)
  if (g_hycov_isOverViewExiting) {
    (*(origStartAnim)g_hycov_pStartAnimHook->m_original)(thisptr, in, left, true);
    hycov_log(LOG,"hook startAnim,disable workspace change anim,in:{},isOverview:{}",in,g_hycov_isOverView);
  } else {
    (*(origStartAnim)g_hycov_pStartAnimHook->m_original)(thisptr, in, left, instant);
    // hycov_log(LOG,"hook startAnim,enable workspace change anim,in:{},isOverview:{}",in,g_hycov_isOverView);
  }
}

static void hkOnKeyboardKey(void* thisptr,std::any event, SP<IKeyboard> pKeyboard) {

  (*(origOnKeyboardKey)g_hycov_pOnKeyboardKeyHook->m_original)(thisptr, event, pKeyboard);

  auto e = std::any_cast<IKeyboard::SKeyEvent>(event);
  // hycov_log(LOG,"alt key,keycode:{}",e.keycode);
  if(g_hycov_enable_alt_release_exit && g_hycov_isOverView && e.state == WL_KEYBOARD_KEY_STATE_RELEASED) {
    if (!isKeyReleaseToggleExitOverviewHit(e,pKeyboard))
      return;
    dispatch_leaveoverview("");
    hycov_log(LOG,"alt key release toggle leave overview");
  }

}

static void hkFullscreenActive(std::string args) {
  // auto exit overview and fullscreen window when toggle fullscreen in overview mode
  hycov_log(LOG,"FullscreenActive hook toggle");

  // (*(origFullscreenActive)g_hycov_pFullscreenActiveHook->m_original)(args);
  const auto pWindow = g_pCompositor->m_lastWindow.lock();

  if (!pWindow)
        return;

  if (pWindow->m_workspace->m_isSpecialWorkspace)
        return;

  if (g_hycov_isOverView && want_auto_fullscren(pWindow) && !g_hycov_auto_fullscreen) {
    hycov_log(LOG,"FullscreenActive toggle leave overview with fullscreen");
    dispatch_toggleoverview("internalToggle");
  } else if (g_hycov_isOverView && (!want_auto_fullscren(pWindow) || g_hycov_auto_fullscreen)) {
    hycov_log(LOG,"FullscreenActive toggle leave overview without fullscreen");
    dispatch_toggleoverview("internalToggle");
  } else {
    hycov_log(LOG,"FullscreenActive set fullscreen");
    g_pCompositor->setWindowFullscreenState(pWindow, {.internal =!pWindow->isFullscreen() ? FSMODE_MAXIMIZED: FSMODE_NONE,.client = args == "1" ? FSMODE_MAXIMIZED: FSMODE_FULLSCREEN});
  }
}

void hkHyprDwindleLayout_recalculateMonitor(void* thisptr,const int& ID) {
  ;
}

void hkHyprMasterLayout_recalculateMonitor(void* thisptr,const int& ID) {
  ;
}

void hkHyprDwindleLayout_recalculateWindow(void* thisptr,CWindow* pWindow) {
  ;
}

void hkSDwindleNodeData_recalcSizePosRecursive(void* thisptr,bool force, bool horizontalOverride, bool verticalOverride) {
  ;
}

void hkCKeybindManager_toggleGroup(std::string args) {
  ;
}

void hkCKeybindManager_moveOutOfGroup(std::string args) {
  ;
}

void hkCKeybindManager_changeGroupActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();
    PHLWINDOW pTargetWindow;
    if (!PWINDOW)
        return;

    if (!PWINDOW->m_groupData.pNextWindow.lock())
        return;

    if (PWINDOW->m_groupData.pNextWindow.lock() == PWINDOW)
        return;

    auto pNode =  g_hycov_OvGridLayout->getNodeFromWindow(PWINDOW);
    if (!pNode)
      return;

    if (args != "b" && args != "prev") {
        pTargetWindow = PWINDOW->m_groupData.pNextWindow.lock();
    } else {
        pTargetWindow = PWINDOW->getGroupPrevious();
    }

    hycov_log(LOG,"changeGroupActive,pTargetWindow:{}",pTargetWindow);

    if(pNode->isInOldLayout) { // if client is taken from the old layout
        g_hycov_OvGridLayout->removeOldLayoutData(PWINDOW);
        pNode->isInOldLayout = false;
    }

    pNode->pWindow = pTargetWindow;
    pNode->pWindow->m_workspace = g_pCompositor->getWorkspaceByID(pNode->workspaceID);

    PWINDOW->setGroupCurrent(pTargetWindow);
    g_hycov_OvGridLayout->applyNodeDataToWindow(pNode);
}


void registerGlobalEventHook()
{
  g_hycov_isInHotArea = false;
  g_hycov_isGestureBegin = false;
  g_hycov_isOverView = false;
  g_hycov_isOverViewExiting = false;
  gesture_dx = 0;
  gesture_dy = 0;
  gesture_previous_dx = 0;
  gesture_previous_dy = 0;

  // HyprlandAPI::registerCallbackStatic(PHANDLE, "mouseMove", mouseMoveHookPtr.get());
  // HyprlandAPI::registerCallbackStatic(PHANDLE, "mouseButton", mouseButtonHookPtr.get());

  //create public function hook

  // hook function of Swipe gesture event handle
  // hook function of Gridlayout Remove a node from tiled list
  g_hycov_pCWindow_onUnmap = HyprlandAPI::createFunctionHook(PHANDLE, (void *)&CWindow::onUnmap, (void*)&hkCWindow_onUnmap);

  // hook function of workspace change animation start
  g_hycov_pStartAnimHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CWorkspace::startAnim, (void*)&hkStartAnim);
  g_hycov_pStartAnimHook->hook();

  //  hook function of keypress
  g_hycov_pOnKeyboardKeyHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CInputManager::onKeyboardKey, (void*)&hkOnKeyboardKey);

  // layotu reculate
  g_hycov_pHyprDwindleLayout_recalculateMonitorHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CHyprDwindleLayout::recalculateMonitor, (void*)&hkHyprDwindleLayout_recalculateMonitor);
  g_hycov_pHyprMasterLayout_recalculateMonitorHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CHyprMasterLayout::recalculateMonitor, (void*)&hkHyprMasterLayout_recalculateMonitor);
  g_hycov_pHyprDwindleLayout_recalculateWindowHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CHyprDwindleLayout::recalculateWindow, (void*)&hkHyprDwindleLayout_recalculateWindow);
  g_hycov_pSDwindleNodeData_recalcSizePosRecursiveHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&SDwindleNodeData::recalcSizePosRecursive, (void*)&hkSDwindleNodeData_recalcSizePosRecursive);


  //mousebutto
  g_hycov_pCInputManager_onMouseButtonHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CInputManager::onMouseButton, (void*)&hkCInputManager_onMouseButton);


  //changeGroupActive
  g_hycov_pCKeybindManager_changeGroupActiveHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CKeybindManager::changeGroupActive, (void*)&hkCKeybindManager_changeGroupActive);

  //toggleGroup
  g_hycov_pCKeybindManager_toggleGroupHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CKeybindManager::toggleGroup, (void*)&hkCKeybindManager_toggleGroup);
  //moveOutOfGroup
  g_hycov_pCKeybindManager_moveOutOfGroupHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CKeybindManager::moveOutOfGroup, (void*)&hkCKeybindManager_moveOutOfGroup);
  //mouse
  // g_hycov_pCInputManager_mouseMoveUnifiedHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CInputManager::mouseMoveUnified, (void*)&hkCInputManager_mouseMoveUnified);


  //create private function hook

  // hook function of changeworkspace
  static const auto ChangeworkspaceMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "changeworkspace");
  g_hycov_pChangeworkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, ChangeworkspaceMethods[0].address, (void*)&hkChangeworkspace);

  // hook function of moveActiveToWorkspace
  static const auto MoveActiveToWorkspaceMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "moveActiveToWorkspace");
  g_hycov_pMoveActiveToWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, MoveActiveToWorkspaceMethods[0].address, (void*)&hkMoveActiveToWorkspace);

  // hook function of spawn (bindkey will call spawn to excute a command or a dispatch)
  static const auto SpawnMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "spawn");
  g_hycov_pSpawnHook = HyprlandAPI::createFunctionHook(PHANDLE, SpawnMethods[0].address, (void*)&hkSpawn);

  //hook function of fullscreenActive
  static const auto FullscreenActiveMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "fullscreenActive");
  g_hycov_pFullscreenActiveHook = HyprlandAPI::createFunctionHook(PHANDLE, FullscreenActiveMethods[0].address, (void*)&hkFullscreenActive);

  //register pEvent hook
  if(g_hycov_enable_hotarea){
    // g_hycov_pCInputManager_mouseMoveUnifiedHook->hook();
    // static auto mouseMoveHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "mouseMove",[&](void* self, SCallbackInfo& info, std::any data) { hkmouseMove(self, info, data); });
  }

  if(g_hycov_enable_click_action) {
    g_hycov_pCInputManager_onMouseButtonHook->hook();
  }

  //if enable gesture, apply hook Swipe function
  if(g_hycov_enable_gesture){
    g_hycov_pOnSwipeBeginHook->hook();
    g_hycov_pOnSwipeEndHook->hook();
    g_hycov_pOnSwipeUpdateHook->hook();
  }

  //if enable auto_exit, apply hook RemovedTiling function
  if(g_hycov_auto_exit){
    g_hycov_pCWindow_onUnmap->hook();
  }

  //apply hook OnKeyboardKey function
  if (g_hycov_enable_alt_release_exit) {
      g_hycov_pOnKeyboardKeyHook->hook();
  }

}
