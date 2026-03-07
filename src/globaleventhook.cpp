
#include "globaleventhook.hpp"
#include "dispatchers.hpp"
#include <set>
#include <hyprland/src/SharedDefs.hpp>
#include "OvGridLayout.hpp"
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/view/Group.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>

typedef void (*origCWindow_onUnmap)(void*);
typedef void (*origFullscreenActive)(std::string args);
typedef void (*origOnKeyboardKey)(void*, const IKeyboard::SKeyEvent&, SP<IKeyboard> pKeyboard);
typedef void (*origCInputManager_onMouseButton)(void*, IPointer::SButtonEvent e);
typedef void (*origCInputManager_mouseMoveUnified)(void* , uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos);
typedef void (*origLayoutManagerRecalculateMonitor)(void*, PHLMONITOR);

static double gesture_dx,gesture_previous_dx;
static double gesture_dy,gesture_previous_dy;

std::string getKeynameFromKeycode(const IKeyboard::SKeyEvent& e, SP<IKeyboard> pKeyboard) {
  auto keyboard = pKeyboard.get();
  xkb_keycode_t keycode = e.keycode + 8;
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(keyboard->m_xkbState, keycode);
  char *tmp_keyname = new char[64];
  xkb_keysym_get_name(keysym, tmp_keyname, 64);
  std::string keyname = tmp_keyname;
  delete[] tmp_keyname;
  return keyname;
}

bool isKeyReleaseToggleExitOverviewHit(const IKeyboard::SKeyEvent& e, SP<IKeyboard> pKeyboard) {
  if (g_hycov_alt_replace_key == "")
    return false;

  if (Hyprutils::String::isNumber(g_hycov_alt_replace_key) && std::stoi(g_hycov_alt_replace_key) > 9 && std::stoi(g_hycov_alt_replace_key) == (int)(e.keycode + 8)) {
    return true;
  } else if (g_hycov_alt_replace_key.find("code:") == 0 && Hyprutils::String::isNumber(g_hycov_alt_replace_key.substr(5)) && std::stoi(g_hycov_alt_replace_key.substr(5)) == (int)(e.keycode + 8)) {
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
  PHLMONITOR pMonitor = Desktop::focusState()->monitor();

  if (!pMonitor)
    return;

  if (g_hycov_hotarea_monitor != "all" && pMonitor->m_name != g_hycov_hotarea_monitor)
    return;

  auto m_x = pMonitor->m_position.x;
  auto m_y = pMonitor->m_position.y;
  auto m_width = pMonitor->m_size.x;
  auto m_height = pMonitor->m_size.y;

  if (!g_hycov_isInHotArea &&
    ((g_hycov_hotarea_pos == 1 && x_root < (m_x + g_hycov_hotarea_size) && y_root > (m_y + m_height - g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 2 && x_root > (m_x + m_width - g_hycov_hotarea_size) && y_root > (m_y + m_height - g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 3 && x_root < (m_x + g_hycov_hotarea_size) && y_root < (m_y + g_hycov_hotarea_size)) ||
    (g_hycov_hotarea_pos == 4 && x_root > (m_x + m_width - g_hycov_hotarea_size) && y_root < (m_y + g_hycov_hotarea_size))))
  {
    g_hycov_isInHotArea = true;
    hycov_log(HYCOV_LOG,"cursor enter hotarea");
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

static void hkCInputManager_mouseMoveUnified(void* thisptr, uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos)
{
  (*(origCInputManager_mouseMoveUnified)g_hycov_pCInputManager_mouseMoveUnifiedHook->m_original)(thisptr, time, refocus, mouse, overridePos);

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

    if (!Desktop::focusState()->window()) {
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
        g_pCompositor->closeWindow(Desktop::focusState()->window());
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
  auto pLastMonitor = Desktop::focusState()->monitor();
	for (auto &n : g_hycov_OvGridLayout->m_lOvGridNodesData) {
		if(pLastMonitor && n.pWindow->monitorID() == pLastMonitor->m_id && !g_pCompositor->isWorkspaceSpecial(n.workspaceID)) {
			nodeNumInSameMonitor++;
		}
		if(pLastMonitor && n.pWindow->m_workspace == pLastMonitor->m_activeWorkspace) {
			nodeNumInSameWorkspace++;
		}
	}

  if (g_hycov_isOverView && nodeNumInSameMonitor == 0) {
    hycov_log(HYCOV_LOG,"no tiling window in same monitor,auto exit overview");
    dispatch_leaveoverview("");
    return;
  }

  if (g_hycov_isOverView && nodeNumInSameWorkspace == 0 && (g_hycov_only_active_workspace || g_hycov_force_display_only_current_workspace)) {
    hycov_log(HYCOV_LOG,"no tiling window in same workspace,auto exit overview");
    dispatch_leaveoverview("");
    return;
  }

}

static void hkChangeworkspace(std::string args) {
  // just log a message and do nothing, mean the original function is disabled
  hycov_log(HYCOV_LOG,"ChangeworkspaceHook hook toggle");
}

static void hkMoveActiveToWorkspace(std::string args) {
  // just log a message and do nothing, mean the original function is disabled
  hycov_log(HYCOV_LOG,"MoveActiveToWorkspace hook toggle");
}

static void hkSpawn(std::string args) {
  // just log a message and do nothing, mean the original function is disabled
  hycov_log(HYCOV_LOG,"Spawn hook toggle");
}

static void hkOnKeyboardKey(void* thisptr, const IKeyboard::SKeyEvent& event, SP<IKeyboard> pKeyboard) {

  (*(origOnKeyboardKey)g_hycov_pOnKeyboardKeyHook->m_original)(thisptr, event, pKeyboard);

  if(g_hycov_enable_alt_release_exit && g_hycov_isOverView && event.state == WL_KEYBOARD_KEY_STATE_RELEASED) {
    if (!isKeyReleaseToggleExitOverviewHit(event, pKeyboard))
      return;
    dispatch_leaveoverview("");
    hycov_log(HYCOV_LOG,"alt key release toggle leave overview");
  }

}

static void hkFullscreenActive(std::string args) {
  // auto exit overview and fullscreen window when toggle fullscreen in overview mode
  hycov_log(HYCOV_LOG,"FullscreenActive hook toggle");

  const auto pWindow = Desktop::focusState()->window();

  if (!pWindow)
        return;

  if (pWindow->m_workspace->m_isSpecialWorkspace)
        return;

  if (g_hycov_isOverView && want_auto_fullscren(pWindow) && !g_hycov_auto_fullscreen) {
    hycov_log(HYCOV_LOG,"FullscreenActive toggle leave overview with fullscreen");
    dispatch_toggleoverview("internalToggle");
  } else if (g_hycov_isOverView && (!want_auto_fullscren(pWindow) || g_hycov_auto_fullscreen)) {
    hycov_log(HYCOV_LOG,"FullscreenActive toggle leave overview without fullscreen");
    dispatch_toggleoverview("internalToggle");
  } else {
    hycov_log(HYCOV_LOG,"FullscreenActive set fullscreen");
    g_pCompositor->setWindowFullscreenState(pWindow, {.internal =!pWindow->isFullscreen() ? FSMODE_MAXIMIZED: FSMODE_NONE,.client = args == "1" ? FSMODE_MAXIMIZED: FSMODE_FULLSCREEN});
  }
}

void hkHyprDwindleLayout_recalculate(void* thisptr) {
  ;
}

void hkHyprMasterLayout_recalculate(void* thisptr) {
  ;
}

void hkLayoutManagerRecalculateMonitor(void* thisptr, PHLMONITOR monitor) {
  // suppress layout recalculation during overview mode
  if (g_hycov_isOverView) {
    return;
  }
  (*(origLayoutManagerRecalculateMonitor)g_hycov_pLayoutManager_recalculateMonitorHook->m_original)(thisptr, monitor);
}

void hkCKeybindManager_toggleGroup(std::string args) {
  ;
}

void hkCKeybindManager_moveOutOfGroup(std::string args) {
  ;
}

void hkCKeybindManager_changeGroupActive(std::string args) {
    const auto PWINDOW = Desktop::focusState()->window();
    PHLWINDOW pTargetWindow;
    if (!PWINDOW)
        return;

    if (!PWINDOW->m_group)
        return;

    if (PWINDOW->m_group->size() <= 1)
        return;

    auto pNode = g_hycov_OvGridLayout->getNodeFromWindow(PWINDOW);
    if (!pNode)
      return;

    if (args != "b" && args != "prev") {
        pTargetWindow = PWINDOW->m_group->next();
    } else {
        // previous is current - 1 (wrap around)
        size_t curIdx = PWINDOW->m_group->getCurrentIdx();
        size_t prevIdx = (curIdx == 0) ? PWINDOW->m_group->size() - 1 : curIdx - 1;
        pTargetWindow = PWINDOW->m_group->fromIndex(prevIdx);
    }

    hycov_log(HYCOV_LOG,"changeGroupActive,pTargetWindow:{}",pTargetWindow);

    pNode->pWindow = pTargetWindow;
    pNode->pWindow->m_workspace = g_pCompositor->getWorkspaceByID(pNode->workspaceID);

    PWINDOW->m_group->setCurrent(pTargetWindow);
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

  //create public function hook

  // hook function of Gridlayout Remove a node from tiled list
  g_hycov_pCWindow_onUnmap = HyprlandAPI::createFunctionHook(PHANDLE, (void *)&Desktop::View::CWindow::onUnmap, (void*)&hkCWindow_onUnmap);

  //  hook function of keypress
  g_hycov_pOnKeyboardKeyHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CInputManager::onKeyboardKey, (void*)&hkOnKeyboardKey);

  // layout recalculate hooks (to suppress during overview)
  static const auto DwindleRecalculateMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "recalculate");
  for (auto &m : DwindleRecalculateMethods) {
    if (m.demangled.find("CDwindleAlgorithm") != std::string::npos) {
      g_hycov_pHyprDwindleLayout_recalculateHook = HyprlandAPI::createFunctionHook(PHANDLE, m.address, (void*)&hkHyprDwindleLayout_recalculate);
      break;
    }
  }
  for (auto &m : DwindleRecalculateMethods) {
    if (m.demangled.find("CMasterAlgorithm") != std::string::npos) {
      g_hycov_pHyprMasterLayout_recalculateHook = HyprlandAPI::createFunctionHook(PHANDLE, m.address, (void*)&hkHyprMasterLayout_recalculate);
      break;
    }
  }

  // hook LayoutManager::recalculateMonitor to suppress during overview
  g_hycov_pLayoutManager_recalculateMonitorHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&Layout::CLayoutManager::recalculateMonitor, (void*)&hkLayoutManagerRecalculateMonitor);

  //mousebutto
  g_hycov_pCInputManager_onMouseButtonHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CInputManager::onMouseButton, (void*)&hkCInputManager_onMouseButton);


  //changeGroupActive
  g_hycov_pCKeybindManager_changeGroupActiveHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CKeybindManager::changeGroupActive, (void*)&hkCKeybindManager_changeGroupActive);

  //toggleGroup
  g_hycov_pCKeybindManager_toggleGroupHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CKeybindManager::toggleGroup, (void*)&hkCKeybindManager_toggleGroup);
  //moveOutOfGroup
  g_hycov_pCKeybindManager_moveOutOfGroupHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&CKeybindManager::moveOutOfGroup, (void*)&hkCKeybindManager_moveOutOfGroup);

  //create private function hook

  // hook function of changeworkspace
  static const auto ChangeworkspaceMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "changeworkspace");
  g_hycov_pChangeworkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, ChangeworkspaceMethods[0].address, (void*)&hkChangeworkspace);

  // hook function of moveActiveToWorkspace
  static const auto MoveActiveToWorkspaceMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "moveActiveToWorkspace");
  g_hycov_pMoveActiveToWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, MoveActiveToWorkspaceMethods[0].address, (void*)&hkMoveActiveToWorkspace);

  // hook function of spawn (bindkey will call spawn to execute a command or a dispatch)
  static const auto SpawnMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "spawn");
  g_hycov_pSpawnHook = HyprlandAPI::createFunctionHook(PHANDLE, SpawnMethods[0].address, (void*)&hkSpawn);

  //hook function of fullscreenActive
  static const auto FullscreenActiveMethods = HyprlandAPI::findFunctionsByName(PHANDLE, "fullscreenActive");
  g_hycov_pFullscreenActiveHook = HyprlandAPI::createFunctionHook(PHANDLE, FullscreenActiveMethods[0].address, (void*)&hkFullscreenActive);

  //register pEvent hook
  if(g_hycov_enable_hotarea){
    // mouseMoveUnified hook can be enabled if needed
    // g_hycov_pCInputManager_mouseMoveUnifiedHook->hook();
  }

  if(g_hycov_enable_click_action) {
    g_hycov_pCInputManager_onMouseButtonHook->hook();
  }

  //if enable gesture, apply hook Swipe function
  if(g_hycov_enable_gesture){
    if (g_hycov_pOnSwipeBeginHook) g_hycov_pOnSwipeBeginHook->hook();
    if (g_hycov_pOnSwipeEndHook) g_hycov_pOnSwipeEndHook->hook();
    if (g_hycov_pOnSwipeUpdateHook) g_hycov_pOnSwipeUpdateHook->hook();
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
