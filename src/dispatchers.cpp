#include "dispatchers.hpp"
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/view/Group.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>

static const std::string overviewWorksapceName = "OVERVIEW";
static std::string workspaceNameBackup;
static int workspaceIdBackup;

void recalculateAllMonitor() {
	for (auto &m : g_pCompositor->m_monitors) {
		g_layoutManager->recalculateMonitor(m);
	}
}

bool want_auto_fullscren(PHLWINDOW pWindow) {
	int nodeNumInTargetWorkspace = 1;

	if(!pWindow) {
		return false;
	}

	auto pNode = g_hycov_OvGridLayout->getNodeFromWindow(pWindow);

	if(!pNode) {
		return true;
	}

	// if client is fullscreen before,don't make it fullscreen
	if (pNode->ovbk_windowIsFullscreen) {
		return false;
	}

	// caculate the number of clients that will be in the same workspace with pWindow(don't contain itself)
	for (auto &n : g_hycov_OvGridLayout->m_lOvGridNodesData) {
		if(n.pWindow != pNode->pWindow && n.ovbk_windowWorkspaceId == pNode->ovbk_windowWorkspaceId) {
			nodeNumInTargetWorkspace++;
		}
	}

	// if only one client in workspace(pWindow itself), don't make it fullscreen
	if(nodeNumInTargetWorkspace > 1) {
		return true;
	} else {
		return false;
	}
}

bool isDirectionArg(std::string arg) {
	if (arg == "l" || arg == "r" || arg == "u" || arg == "d" || arg == "left" || arg == "right" || arg == "up" || arg == "down"  || arg == "leftcross" || arg == "rightcross" || arg == "upcross" || arg == "downcross") {
		return true;
	} else {
		return false;
	}
}

bool isCrossMonitor(std::string arg) {
	if (arg == "leftcross" || arg == "rightcross" || arg == "upcross" || arg == "downcross") {
		return true;
	} else {
		return false;
	}
}

std::optional<ShiftDirection> parseShiftArg(std::string arg) {
	if (arg == "l" || arg == "left" || arg == "leftcross") return ShiftDirection::Left;
	else if (arg == "r" || arg == "right" || arg == "rightcross") return ShiftDirection::Right;
	else if (arg == "u" || arg == "up" || arg == "upcross") return ShiftDirection::Up;
	else if (arg == "d" || arg == "down" || arg == "downcross") return ShiftDirection::Down;
	else return {};
}

PHLWINDOW direction_select(std::string arg){
	PHLWINDOW pTempClient = Desktop::focusState()->window();
    auto dataSize = g_pCompositor->m_windows.size();
	auto pTempCWindows = new PHLWINDOW[dataSize + 1];
	PHLWINDOW pTempFocusCWindows = nullptr;
	int last = -1;
	if(!pTempClient){
		delete[] pTempCWindows;
		return nullptr;
	}else if (pTempClient->isFullscreen()){
		delete[] pTempCWindows;
		return nullptr;
	}

    if (!isDirectionArg(arg)) {
        hycov_log(HYCOV_ERR, "Cannot move focus in direction {}, unsupported direction. Supported: l/left/leftcross,r/right/rightcross,u/up/upcross,d/down/downcross", arg);
		delete[] pTempCWindows;
        return nullptr;
    }

    for (auto &w : g_pCompositor->m_windows)
    {
		PHLWINDOW pWindow = w;

        if (pTempClient == pWindow || pWindow->isHidden() || !pWindow->m_isMapped || pWindow->m_fadingOut || pWindow->isFullscreen()) {
			continue;
		}

		auto *pMonitor = g_pCompositor->getMonitorFromID(pWindow->monitorID()).get();

		if (!((isCrossMonitor(arg) && pWindow->m_monitor->m_id != pTempClient->m_monitor->m_id && !pTempClient->m_workspace->m_isSpecialWorkspace && pWindow->m_workspace == pMonitor->m_activeWorkspace) || pTempClient->m_workspace == pWindow->m_workspace)) {
			continue;
		}

		last++;
		pTempCWindows[last] = pWindow;
    }

  	if (last < 0) {
		delete[] pTempCWindows;
  		return nullptr;
	}
  	int sel_x = pTempClient->m_position.x;
  	int sel_y = pTempClient->m_position.y;
  	long long int distance = LLONG_MAX;;

	auto shift = parseShiftArg(arg);
  	switch (shift.value()) {
  	case ShiftDirection::Up:
		// Find the window with the closest coordinates
		// in the top left corner of the window (is limited to same x)
  		for (int _i = 0; _i <= last; _i++) {
  		  if (pTempCWindows[_i]->m_position.y < sel_y && pTempCWindows[_i]->m_position.x == sel_x) {
  		    int dis_x = pTempCWindows[_i]->m_position.x - sel_x;
  		    int dis_y = pTempCWindows[_i]->m_position.y - sel_y;
  		    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y;
  		    if (tmp_distance < distance) {
  		      distance = tmp_distance;
  		      pTempFocusCWindows = pTempCWindows[_i];
  		    }
  		  }
  		}
		// if find nothing above
		// find again(is unlimited to x)
		if(!pTempFocusCWindows){
  			for (int _i = 0; _i <= last; _i++) {
  			  if (pTempCWindows[_i]->m_position.y < sel_y ) {
  			    int dis_x = pTempCWindows[_i]->m_position.x - sel_x;
  			    int dis_y = pTempCWindows[_i]->m_position.y - sel_y;
  			    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y;
  			    if (tmp_distance < distance) {
  			      distance = tmp_distance;
  			      pTempFocusCWindows = pTempCWindows[_i];
  			    }
  			  }
  			}
		}
  		break;
  	case ShiftDirection::Down:
  		for (int _i = 0; _i <= last; _i++) {
  		  if (pTempCWindows[_i]->m_position.y > sel_y && pTempCWindows[_i]->m_position.x == sel_x) {
  		    int dis_x = pTempCWindows[_i]->m_position.x - sel_x;
  		    int dis_y = pTempCWindows[_i]->m_position.y - sel_y;
  		    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y;
  		    if (tmp_distance < distance) {
  		      distance = tmp_distance;
  		      pTempFocusCWindows = pTempCWindows[_i];
  		    }
  		  }
  		}
		if(!pTempFocusCWindows){
  			for (int _i = 0; _i <= last; _i++) {
  			  if (pTempCWindows[_i]->m_position.y > sel_y ) {
  			    int dis_x = pTempCWindows[_i]->m_position.x - sel_x;
  			    int dis_y = pTempCWindows[_i]->m_position.y - sel_y;
  			    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y;
  			    if (tmp_distance < distance) {
  			      distance = tmp_distance;
  			      pTempFocusCWindows = pTempCWindows[_i];
  			    }
  			  }
  			}
		}
  		break;
  	case ShiftDirection::Left:
  		for (int _i = 0; _i <= last; _i++) {
  		  if (pTempCWindows[_i]->m_position.x < sel_x && pTempCWindows[_i]->m_position.y == sel_y) {
  		    int dis_x = pTempCWindows[_i]->m_position.x - sel_x;
  		    int dis_y = pTempCWindows[_i]->m_position.y - sel_y;
  		    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y;
  		    if (tmp_distance < distance) {
  		      distance = tmp_distance;
  		      pTempFocusCWindows = pTempCWindows[_i];
  		    }
  		  }
  		}
		if(!pTempFocusCWindows){
  			for (int _i = 0; _i <= last; _i++) {
  			  if (pTempCWindows[_i]->m_position.x < sel_x) {
  			    int dis_x = pTempCWindows[_i]->m_position.x - sel_x;
  			    int dis_y = pTempCWindows[_i]->m_position.y - sel_y;
  			    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y;
  			    if (tmp_distance < distance) {
  			      distance = tmp_distance;
  			      pTempFocusCWindows = pTempCWindows[_i];
  			    }
  			  }
  			}
		}
  		break;
  	case ShiftDirection::Right:
  		for (int _i = 0; _i <= last; _i++) {
  		  if (pTempCWindows[_i]->m_position.x > sel_x && pTempCWindows[_i]->m_position.y == sel_y) {
  		    int dis_x = pTempCWindows[_i]->m_position.x - sel_x;
  		    int dis_y = pTempCWindows[_i]->m_position.y - sel_y;
  		    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y;
  		    if (tmp_distance < distance) {
  		      distance = tmp_distance;
  		      pTempFocusCWindows = pTempCWindows[_i];
  		    }
  		  }
  		}
		if(!pTempFocusCWindows){
  			for (int _i = 0; _i <= last; _i++) {
  			  if (pTempCWindows[_i]->m_position.x > sel_x) {
  			    int dis_x = pTempCWindows[_i]->m_position.x - sel_x;
  			    int dis_y = pTempCWindows[_i]->m_position.y - sel_y;
  			    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y;
  			    if (tmp_distance < distance) {
  			      distance = tmp_distance;
  			      pTempFocusCWindows = pTempCWindows[_i];
  			    }
  			  }
  			}
		}
  		break;
  	}
	delete[] pTempCWindows;
  	return pTempFocusCWindows;
}

PHLWINDOW get_circle_next_window (std::string arg) {
	bool next_ready = false;
	PHLWINDOW pTempClient = Desktop::focusState()->window();

	if(!pTempClient)
		return nullptr;

    for (auto &w : g_pCompositor->m_windows)
    {
		PHLWINDOW pWindow = w;
        if (pTempClient->m_workspace != pWindow->m_workspace || pWindow->isHidden() || !pWindow->m_isMapped || pWindow->m_fadingOut || pWindow->isFullscreen())
            continue;
		if (next_ready)
			return 	pWindow;
		if (pWindow == pTempClient)
			next_ready = true;
    }

    for (auto &w : g_pCompositor->m_windows)
    {
		PHLWINDOW pWindow = w;
        if (pTempClient->m_workspace != pWindow->m_workspace || pWindow->isHidden() || !pWindow->m_isMapped || pWindow->m_fadingOut || pWindow->isFullscreen())
            continue;
		return pWindow;
    }
	return nullptr;
}

void warpcursor_and_focus_to_window(PHLWINDOW pWindow) {
	Desktop::focusState()->fullWindowFocus(pWindow, Desktop::FOCUS_REASON_OTHER);
	g_pCompositor->warpCursorTo(pWindow->middle());
}

void dispatch_circle(std::string arg)
{
	PHLWINDOW pWindow;
	pWindow = get_circle_next_window(arg);
	if(pWindow){
		warpcursor_and_focus_to_window(pWindow);
	}
}

void dispatch_focusdir(std::string arg)
{
	PHLWINDOW pWindow;
	pWindow = direction_select(arg);
	if(pWindow){
		warpcursor_and_focus_to_window(pWindow);
	}
}

void dispatch_toggleoverview(std::string arg)
{
	if (g_hycov_isOverView && (!g_hycov_enable_alt_release_exit || arg == "internalToggle")) {
		dispatch_leaveoverview("");
		hycov_log(HYCOV_LOG,"leave overview:toggleMethod:{},enable_alt_release_exit:{}",arg,g_hycov_enable_alt_release_exit);
	} else if (g_hycov_isOverView && g_hycov_enable_alt_release_exit && arg != "internalToggle") {
		dispatch_circle("");
		hycov_log(HYCOV_LOG,"toggle overview:switch focus circlely");
	} else if(g_hycov_enable_alt_release_exit && g_hycov_alt_toggle_auto_next && arg != "internalToggle") {
		dispatch_enteroverview(arg);
		dispatch_circle("");
		hycov_log(HYCOV_LOG,"enter overview:alt switch mode auto next");
	} else {
		dispatch_enteroverview(arg);
		hycov_log(HYCOV_LOG,"enter overview:toggleMethod:{}",arg);
	}
}


void dispatch_enteroverview(std::string arg)
{
	if(g_hycov_isOverView) {
		return;
	}

	const auto pMonitor = Desktop::focusState()->monitor();
	if(pMonitor->activeSpecialWorkspaceID() != 0)
		pMonitor->setSpecialWorkspace(nullptr);

	//force display all workspace window,ignore `only_active_worksapce` and `only_active_monitor`
	if (arg == "forceall") {
		g_hycov_forece_display_all = true;
		hycov_log(HYCOV_LOG,"force display all clients");
	} else {
		g_hycov_forece_display_all = false;
	}

	//force display all workspace window in one monitor,ignore `only_active_worksapce` and `only_active_monitor`
	if (arg == "forceallinone") {
		g_hycov_forece_display_all_in_one_monitor = true;
		hycov_log(HYCOV_LOG,"force display all clients in one monitor");
	} else {
		g_hycov_forece_display_all_in_one_monitor = false;
	}

	//force only display current workspace,ignore `only_active_worksapce` and `only_active_monitor`
	if (arg == "onlycurrentworkspace") {
		g_hycov_force_display_only_current_workspace = true;
		hycov_log(HYCOV_LOG,"force display only current workspace");
	} else {
		g_hycov_force_display_only_current_workspace = false;
	}

	//ali clients exit fullscreen status before enter overview
	PHLWINDOW pFullscreenWindow;
	PHLWINDOW pActiveWindow = Desktop::focusState()->window();
	PHLWORKSPACE pActiveWorkspace;
	CMonitor *pActiveMonitor;

	bool isNoShouldTileWindow = true;

    for (auto &w : g_pCompositor->m_windows)
    {
		PHLWINDOW pWindow = w;
        if (pWindow->isHidden() || !pWindow->m_isMapped || pWindow->m_fadingOut || pWindow->m_workspace->m_isSpecialWorkspace)
            continue;
		isNoShouldTileWindow = false;
	}

	//if no clients, forbit enter overview
	if(isNoShouldTileWindow){
		return;
	}

	hycov_log(HYCOV_LOG,"enter overview");
	g_hycov_isOverView = true;

	// Suppress layout recalculation during overview
	g_hycov_pLayoutManager_recalculateMonitorHook->hook();
	if (g_hycov_pHyprDwindleLayout_recalculateHook)
		g_hycov_pHyprDwindleLayout_recalculateHook->hook();
	if (g_hycov_pHyprMasterLayout_recalculateHook)
		g_hycov_pHyprMasterLayout_recalculateHook->hook();

	//make all fullscreen window exit fullscreen state
	for (auto pWorkspace : g_pCompositor->getWorkspacesCopy())
	{
		if (!pWorkspace || pWorkspace->inert())
			continue;
		if (pWorkspace->m_hasFullscreenWindow)
		{
			pFullscreenWindow = pWorkspace->getFullscreenWindow();
			g_pCompositor->setWindowFullscreenState(pFullscreenWindow, { .internal = FSMODE_NONE, .client = FSMODE_FULLSCREEN });

			//let overview know the client is a fullscreen before
            pFullscreenWindow->m_fullscreenState = { .internal = FSMODE_NONE, .client = FSMODE_FULLSCREEN };
		}
	}

	//change workspace name to OVERVIEW
	pActiveMonitor	= Desktop::focusState()->monitor().get();
	pActiveWorkspace = g_pCompositor->getWorkspaceByID(pActiveMonitor->m_activeWorkspace->m_id);
	workspaceNameBackup = pActiveWorkspace->m_name;
	workspaceIdBackup = pActiveWorkspace->m_id;
    g_pCompositor->getWorkspaceByID(workspaceIdBackup)->m_name = std::move(overviewWorksapceName);

	// Initialize overview layout: record window states and move to active workspace
	for (auto &w : g_pCompositor->m_windows) {
		PHLWINDOW pWindow = w;
		if (pWindow->isHidden() || !pWindow->m_isMapped || pWindow->m_fadingOut)
			continue;
		if (pWindow->m_workspace->m_isSpecialWorkspace && !g_hycov_show_special)
			continue;

		if (pWindow->monitorID() != pActiveMonitor->m_id &&
			g_hycov_only_active_monitor && !g_hycov_forece_display_all &&
			!g_hycov_forece_display_all_in_one_monitor)
			continue;

		if (pWindow->m_workspace != pActiveWorkspace &&
			(g_hycov_only_active_workspace || g_hycov_force_display_only_current_workspace) &&
			!g_hycov_forece_display_all && !g_hycov_forece_display_all_in_one_monitor)
			continue;

		// Record each window in the old layout
		const auto pOldNode = &g_hycov_OvGridLayout->m_lSOldLayoutRecordNodeData.emplace_back();
		pOldNode->pWindow = pWindow;

		const auto pNode = &g_hycov_OvGridLayout->m_lOvGridNodesData.emplace_back();

		auto pWindowOriWorkspace = pWindow->m_workspace;

		auto oldLayoutRecordNode = g_hycov_OvGridLayout->getOldLayoutRecordNodeFromWindow(pWindow);
		if (oldLayoutRecordNode) {
			pNode->isInOldLayout = true;
			g_hycov_OvGridLayout->m_lSOldLayoutRecordNodeData.remove(*oldLayoutRecordNode);
		}

		// record the previous window in group
		if (pWindow->m_group && pWindow->m_group->current() == pWindow) {
			pNode->isGroupActive = true;
		}

		pNode->workspaceID = pWindow->workspaceID();
		pNode->pWindow = pWindow;
		pNode->workspaceName = pWindowOriWorkspace->m_name;

		// record the window stats which are used by restore
		pNode->ovbk_windowMonitorId = pWindow->monitorID();
		pNode->ovbk_windowWorkspaceId = pWindow->workspaceID();
		pNode->ovbk_windowFullscreenMode = pWindowOriWorkspace->m_fullscreenMode;
		pNode->ovbk_position = pWindow->m_realPosition->goal();
		pNode->ovbk_size = pWindow->m_realSize->goal();
		pNode->ovbk_windowIsFloating = pWindow->m_isFloating;
		pNode->ovbk_windowIsFullscreen = pWindow->isFullscreen();
		pNode->ovbk_windowWorkspaceName = pWindowOriWorkspace->m_name;

		// change all client(exclude special workspace) to active workspace
		if ((!g_pCompositor->isWorkspaceSpecial(pNode->workspaceID) ||
			 g_hycov_show_special) &&
			pNode->isInOldLayout &&
			(pWindowOriWorkspace->m_id != pActiveWorkspace->m_id ||
			 pWindowOriWorkspace->m_name != pActiveWorkspace->m_name) &&
			(!(g_hycov_only_active_workspace ||
			   g_hycov_force_display_only_current_workspace) ||
			 g_hycov_forece_display_all ||
			 g_hycov_forece_display_all_in_one_monitor)) {
			// Use direct assignment to avoid triggering dwindle/master layout
			// callbacks that would reposition windows after calculateWorkspace
			pWindow->m_workspace = pActiveWorkspace;
			// Always update the monitor when moving cross-monitor windows,
			// otherwise windows render on the wrong monitor in overview
			if (pWindow->monitorID() != pActiveMonitor->m_id) {
				pWindow->m_monitor = g_pCompositor->getMonitorFromID(pActiveMonitor->m_id);
			}
			pNode->workspaceID = pWindow->workspaceID();
			pNode->workspaceName = pActiveWorkspace->m_name;
		}

		// clean fullscreen status
		if (pWindow->isFullscreen()) {
			pWindow->m_fullscreenState = {.internal = FSMODE_NONE,
										  .client = FSMODE_FULLSCREEN};
		}

		// clean floating status(only apply to old layout window)
		if (pWindow->m_isFloating && pNode->isInOldLayout) {
			pWindow->m_isFloating = false;
			pWindow->updateWindowData();
		}
	}

	// Remove the duplicate old layout records (we added them before node creation)
	g_hycov_OvGridLayout->m_lSOldLayoutRecordNodeData.clear();

	// Recalculate grid layout for the active workspace
	g_hycov_OvGridLayout->calculateWorkspace(pActiveWorkspace->m_id);

	//Preserve window focus and warp cursor to focused window's grid position
	if(pActiveWindow){
		warpcursor_and_focus_to_window(pActiveWindow);
	} else {
    	for (auto &w : g_pCompositor->m_windows) {
			PHLWINDOW pWindow = w;
			auto node = g_hycov_OvGridLayout->getNodeFromWindow(pWindow);
    	    if ( !node || g_pCompositor->isWorkspaceSpecial(node->workspaceID) || pWindow->isHidden() || !pWindow->m_isMapped || pWindow->m_fadingOut || pWindow->isFullscreen())
    	        continue;
			warpcursor_and_focus_to_window(pWindow);
			break;
    	}
	}

	// enable hook fullscreenActive function
  	g_hycov_pFullscreenActiveHook->hook();

	//disable changeworkspace
	if(g_hycov_disable_workspace_change) {
  		g_hycov_pChangeworkspaceHook->hook();
		g_hycov_pMoveActiveToWorkspaceHook->hook();
	}

	//disable spawn
	if(g_hycov_disable_spawn) {
		g_hycov_pSpawnHook->hook();
	}

	g_hycov_pCKeybindManager_changeGroupActiveHook->hook();
	g_hycov_pCKeybindManager_toggleGroupHook->hook();
	g_hycov_pCKeybindManager_moveOutOfGroupHook->hook();

	return;
}

void dispatch_leaveoverview(std::string arg)
{
	if(!g_hycov_isOverView) {
		return;
	}

	const auto pMonitor = Desktop::focusState()->monitor();
	if(pMonitor->activeSpecialWorkspaceID() != 0)
		pMonitor->setSpecialWorkspace(nullptr);

	hycov_log(HYCOV_LOG,"leave overview");
	g_hycov_isOverView = false;
	g_hycov_isOverViewExiting = true;

	//restore workspace name
    g_pCompositor->getWorkspaceByID(workspaceIdBackup)->m_name = std::move(workspaceNameBackup);

	//enable changeworkspace
	if(g_hycov_disable_workspace_change) {
  		g_hycov_pChangeworkspaceHook->unhook();
		g_hycov_pMoveActiveToWorkspaceHook->unhook();
	}

	//enable spawn
	if(g_hycov_disable_spawn) {
		g_hycov_pSpawnHook->unhook();
	}

	g_hycov_pCKeybindManager_changeGroupActiveHook->unhook();
	g_hycov_pCKeybindManager_toggleGroupHook->unhook();
	g_hycov_pCKeybindManager_moveOutOfGroupHook->unhook();

	// if no clients, just exit overview, don't restore client's state
	if (g_hycov_OvGridLayout->m_lOvGridNodesData.empty())
	{
		// Re-enable layout recalculation
		g_hycov_pLayoutManager_recalculateMonitorHook->unhook();
		recalculateAllMonitor();
		g_hycov_isOverViewExiting = false;
		return;
	}

	//move clients to it's original workspace
	g_hycov_OvGridLayout->moveWindowToSourceWorkspace();
	// go to the workspace where the active client was before
	g_hycov_OvGridLayout->changeToActivceSourceWorkspace();

	for (auto &n : g_hycov_OvGridLayout->m_lOvGridNodesData)
	{
		if (n.ovbk_windowIsFloating)
		{
			//make floating client restore it's floating status
			n.pWindow->m_isFloating = true;

			// make floating client restore it's position and size
			n.pWindow->m_realSize->setValueAndWarp(n.ovbk_size);
			n.pWindow->m_realPosition->setValueAndWarp(n.ovbk_position);

		} else if(!n.ovbk_windowIsFloating && !n.ovbk_windowIsFullscreen) {
			// make nofloating client restore it's position and size
			n.pWindow->m_realSize->setValueAndWarp(n.ovbk_size);
			n.pWindow->m_realPosition->setValueAndWarp(n.ovbk_position);

			// restore active window in group
			if(n.isGroupActive && n.pWindow->m_group) {
				n.pWindow->m_group->setCurrent(n.pWindow);
			}
		}
	}

	//Preserve window focus
	PHLWINDOW pActiveWindow = Desktop::focusState()->window();
	Desktop::focusState()->fullWindowFocus(nullptr, Desktop::FOCUS_REASON_OTHER);

	if(pActiveWindow){
		if(g_hycov_forece_display_all_in_one_monitor && pActiveWindow->monitorID() != Desktop::focusState()->monitor()->m_id) {
			warpcursor_and_focus_to_window(pActiveWindow);
		} else {
			Desktop::focusState()->fullWindowFocus(pActiveWindow, Desktop::FOCUS_REASON_OTHER);
		}

		if(pActiveWindow->m_isFloating && g_hycov_raise_float_to_top) {
			g_pCompositor->changeWindowZOrder(pActiveWindow, true);
		} else if(g_hycov_auto_fullscreen && want_auto_fullscren(pActiveWindow)) {
			g_pCompositor->setWindowFullscreenState(pActiveWindow, { .internal = FSMODE_NONE, .client = FSMODE_MAXIMIZED });
		}
	}

	for (auto &n : g_hycov_OvGridLayout->m_lOvGridNodesData)
	{
		//make all fullscreen window restore it's status
		if (n.ovbk_windowIsFullscreen)
		{
			if (!Desktop::focusState()->window()) {
				continue;
			}

			if (n.pWindow != Desktop::focusState()->window() && n.pWindow->m_workspace == Desktop::focusState()->window()->m_workspace)
			{
				continue;
			}
			g_pCompositor->setWindowFullscreenState(n.pWindow, { .internal = FSMODE_NONE, .client = FSMODE_MAXIMIZED });
		}
	}

	//clean overview layout node data
	g_hycov_OvGridLayout->m_lOvGridNodesData.clear();

	//mark has exited overview mode
	g_hycov_isOverViewExiting = false;

	// Re-enable layout recalculation and recalculate
	if (g_hycov_pHyprDwindleLayout_recalculateHook)
		g_hycov_pHyprDwindleLayout_recalculateHook->unhook();
	if (g_hycov_pHyprMasterLayout_recalculateHook)
		g_hycov_pHyprMasterLayout_recalculateHook->unhook();
	g_hycov_pLayoutManager_recalculateMonitorHook->unhook();
	recalculateAllMonitor();

	// disable hook fullscreenActive function
  	g_hycov_pFullscreenActiveHook->unhook();

	return;
}

SDispatchResult dispatch_circle_v2(std::string arg)
{
	dispatch_circle(arg);
	return {};
}

SDispatchResult dispatch_focusdir_v2(std::string arg)
{
	dispatch_focusdir(arg);
	return {};
}

SDispatchResult dispatch_toggleoverview_v2(std::string arg)
{
	dispatch_toggleoverview(arg);
	return {};
}

SDispatchResult dispatch_enteroverview_v2(std::string arg)
{
	dispatch_enteroverview(arg);
	return {};
}

SDispatchResult dispatch_leaveoverview_v2(std::string arg)
{
	dispatch_leaveoverview(arg);
	return {};
}

void registerDispatchers()
{
	HyprlandAPI::addDispatcherV2(PHANDLE, "hycov:enteroverview", dispatch_enteroverview_v2);
	HyprlandAPI::addDispatcherV2(PHANDLE, "hycov:leaveoverview", dispatch_leaveoverview_v2);
	HyprlandAPI::addDispatcherV2(PHANDLE, "hycov:toggleoverview", dispatch_toggleoverview_v2);
	HyprlandAPI::addDispatcherV2(PHANDLE, "hycov:movefocus", dispatch_focusdir_v2);
}
