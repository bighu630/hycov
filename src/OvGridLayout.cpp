
#include "OvGridLayout.hpp"
#include "dispatchers.hpp"
#include "globals.hpp"
#include "src/SharedDefs.hpp"
#include "src/desktop/Workspace.hpp"
#include "src/helpers/Monitor.hpp"
#include "src/managers/EventManager.hpp"
#include "src/render/Renderer.hpp"
#include <cmath>

SOvGridNodeData *OvGridLayout::getNodeFromWindow(PHLWINDOW pWindow) {
  for (auto &nd : m_lOvGridNodesData) {
    if (nd.pWindow == pWindow)
      return &nd;
  }

  return nullptr;
}

SOldLayoutRecordNodeData *
OvGridLayout::getOldLayoutRecordNodeFromWindow(PHLWINDOW pWindow) {
  for (auto &nd : m_lSOldLayoutRecordNodeData) {
    if (nd.pWindow == pWindow)
      return &nd;
  }

  return nullptr;
}

int OvGridLayout::getNodesNumOnWorkspace(const int &ws) {
  int no = 0;
  for (auto &n : m_lOvGridNodesData) {
    if (n.workspaceID == ws)
      no++;
  }

  return no;
}

void OvGridLayout::resizeNodeSizePos(SOvGridNodeData *node, int x, int y,
                                     int width, int height) {

  node->size = Vector2D(width, height - g_hycov_height_of_titlebar);
  node->position =
      Vector2D(x, y + g_hycov_height_of_titlebar);
  applyNodeDataToWindow(node);
}

void OvGridLayout::removeOldLayoutData(PHLWINDOW pWindow) {
  // In the new layout system we don't need to do special handling
  // just iterate and remove from lSOldLayoutRecordNodeData
  auto oldNode = getOldLayoutRecordNodeFromWindow(pWindow);
  if (oldNode) {
    m_lSOldLayoutRecordNodeData.remove(*oldNode);
  }
}

void OvGridLayout::calculateWorkspace(const int &ws) {
  const auto pWorksapce = g_pCompositor->getWorkspaceByID(ws);
  if (!pWorksapce)
    return;

  const int NODECOUNT = getNodesNumOnWorkspace(pWorksapce->m_id);
  if (NODECOUNT == 0)
    return;

  const auto pMonitor = g_pCompositor->getMonitorFromID(pWorksapce->monitorID());

  static const auto *PBORDERSIZE = &g_hycov_bordersize;
  static const auto *GAPPO = &g_hycov_overview_gappo;
  static const auto *GAPPI = &g_hycov_overview_gappi;

  // w is the usable area (monitor minus reserved bar space)
  int w_x = pMonitor->m_position.x + (int)pMonitor->m_reservedArea.left();
  int w_y = pMonitor->m_position.y + (int)pMonitor->m_reservedArea.top();
  int w_width = pMonitor->m_size.x - (int)pMonitor->m_reservedArea.left() - (int)pMonitor->m_reservedArea.right();
  int w_height = pMonitor->m_size.y - (int)pMonitor->m_reservedArea.top() - (int)pMonitor->m_reservedArea.bottom();

  // Collect nodes for this workspace
  auto dataSize = m_lOvGridNodesData.size();
  auto pTempNodes = new SOvGridNodeData *[dataSize + 1];
  int n = 0;
  for (auto &node : m_lOvGridNodesData) {
    if (node.workspaceID == ws)
      pTempNodes[n++] = &node;
  }
  pTempNodes[n] = nullptr;

  // Grid dimensions: cols = ceil(sqrt(N)), rows = ceil(N / cols)
  int cols = std::max(1, (int)std::ceil(std::sqrt((double)NODECOUNT)));
  int rows = (NODECOUNT + cols - 1) / cols;

  // Uniform cell dimensions
  int cw = (w_width - 2 * (*GAPPO) - (cols - 1) * (*GAPPI)) / cols;
  int ch = (w_height - 2 * (*GAPPO) - (rows - 1) * (*GAPPI)) / rows;

  // Number of windows in the last (potentially partial) row
  int last_row_count = NODECOUNT % cols;

  for (int i = 0; pTempNodes[i]; i++) {
    int row = i / cols;
    int col = i % cols;

    // Center windows in the last partial row
    int x_offset = 0;
    if (last_row_count != 0 && row == rows - 1) {
      int used_width = last_row_count * cw + (last_row_count - 1) * (*GAPPI);
      x_offset = (w_width - 2 * (*GAPPO) - used_width) / 2;
    }

    int x = w_x + (*GAPPO) + x_offset + col * (cw + (*GAPPI));
    int y = w_y + (*GAPPO) + row * (ch + (*GAPPI));

    resizeNodeSizePos(pTempNodes[i], x, y, cw - 2 * (*PBORDERSIZE), ch - 2 * (*PBORDERSIZE));
  }

  delete[] pTempNodes;
}

void OvGridLayout::applyNodeDataToWindow(SOvGridNodeData *pNode) {

  const auto pWindow = pNode->pWindow;

  pWindow->m_size = pNode->size;
  pWindow->m_position = pNode->position;

  auto calcPos = pWindow->m_position;
  auto calcSize = pWindow->m_size;

  // Use setValueAndWarp to set both value and goal, stopping any in-progress
  // animation. This is critical for windows moved from other workspaces:
  // their goal still points to the old workspace position and without this
  // the animation system would animate them back there.
  pWindow->m_realSize->setValueAndWarp(calcSize);
  pWindow->m_realPosition->setValueAndWarp(calcPos);

  pWindow->updateWindowDecos();
}

void OvGridLayout::changeToActivceSourceWorkspace() {
  PHLWINDOW pWindow = nullptr;
  SOvGridNodeData *pNode;
  PHLWORKSPACE pWorksapce;
  hycov_log(HYCOV_LOG, "changeToActivceSourceWorkspace");
  pWindow = Desktop::focusState()->window();
  pNode = getNodeFromWindow(pWindow);
  if (pNode) {
    pWorksapce = g_pCompositor->getWorkspaceByID(pNode->ovbk_windowWorkspaceId);
  } else if (pWindow) {
    pWorksapce = pWindow->m_workspace;
  } else {
    pWorksapce = Desktop::focusState()->monitor()->m_activeWorkspace;
  }
  hycov_log(HYCOV_LOG, "changeToWorkspace:{}", pWorksapce->m_id);
  g_pEventManager->postEvent(SHyprIPCEvent{"workspace", pWorksapce->m_name});
}

void OvGridLayout::moveWindowToSourceWorkspace() {
  PHLWORKSPACE pWorkspace;

  hycov_log(HYCOV_LOG, "moveWindowToSourceWorkspace");

  for (auto &nd : m_lOvGridNodesData) {
    if (nd.pWindow && nd.pWindow->m_workspace &&
        (nd.pWindow->m_workspace->m_id != nd.ovbk_windowWorkspaceId ||
         nd.workspaceName != nd.ovbk_windowWorkspaceName)) {
      pWorkspace = g_pCompositor->getWorkspaceByID(nd.ovbk_windowWorkspaceId);
      if (!pWorkspace) {
        hycov_log(HYCOV_LOG, "source workspace no exist");
        // Validate the original monitor still exists; fall back to active monitor
        auto pOrigMonitor = g_pCompositor->getMonitorFromID(nd.ovbk_windowMonitorId);
        MONITORID monitorId = pOrigMonitor ? nd.ovbk_windowMonitorId
                                           : Desktop::focusState()->monitor()->m_id;
        g_hycov_pSpawnHook->hook(); // disable on-empty-create workspace rule
        pWorkspace = g_pCompositor->createNewWorkspace(
            nd.ovbk_windowWorkspaceId, monitorId, nd.ovbk_windowWorkspaceName);
        g_hycov_pSpawnHook->unhook();
        if (!pWorkspace) {
          hycov_log(HYCOV_LOG, "failed to create workspace, skipping window");
          continue;
        }
        // The source workspace was destroyed while in overview, which also destroyed
        // its layout space. The window's ITarget::currentSpace is now a dangling
        // pointer, so calling moveWindowToWorkspaceSafe would crash in the layout's
        // removeTarget. Use direct assignment instead (matching how enteroverview
        // moved the window in), then let recalculateAllMonitor() rebuild layouts.
        nd.pWindow->m_workspace = pWorkspace;
        if (pOrigMonitor)
          nd.pWindow->m_monitor = pOrigMonitor;
        nd.workspaceID = nd.ovbk_windowWorkspaceId;
        nd.workspaceName = nd.ovbk_windowWorkspaceName;
        nd.pWindow->m_position = nd.ovbk_position;
        nd.pWindow->m_size = nd.ovbk_size;
        g_pHyprRenderer->damageWindow(nd.pWindow);
        continue;
      }
      // Move window back to its source workspace
      g_pCompositor->moveWindowToWorkspaceSafe(nd.pWindow, pWorkspace);
      nd.workspaceID = nd.ovbk_windowWorkspaceId;
      nd.workspaceName = nd.ovbk_windowWorkspaceName;
      nd.pWindow->m_position = nd.ovbk_position;
      nd.pWindow->m_size = nd.ovbk_size;
      g_pHyprRenderer->damageWindow(nd.pWindow);
    }
  }
}
