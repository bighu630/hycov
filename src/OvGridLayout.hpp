#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <list>


struct SOvGridNodeData
{
  PHLWINDOW pWindow = nullptr;
  int ovbk_windowWorkspaceId = -1;
  std::string ovbk_windowWorkspaceName;
  int ovbk_windowMonitorId = -1;
  std::string workspaceName;
  bool ovbk_windowIsFloating = false;
  bool ovbk_windowIsFullscreen = false;
  eFullscreenMode ovbk_windowFullscreenMode;
  Vector2D ovbk_position;
  Vector2D ovbk_size;
  Vector2D position;
  Vector2D size;
  bool isInOldLayout = false;
  bool isGroupActive = false;

  int workspaceID = -1;

  bool operator==(const SOvGridNodeData &rhs) const
  {
    return pWindow == rhs.pWindow;
  }
};


struct SOldLayoutRecordNodeData
{
  PHLWINDOW pWindow = nullptr;
  bool operator==(const SOldLayoutRecordNodeData &rhs) const
  {
    return pWindow == rhs.pWindow;
  }
};


class OvGridLayout
{
public:
  void applyNodeDataToWindow(SOvGridNodeData *);
  void calculateWorkspace(const int &);
  int getNodesNumOnWorkspace(const int &);
  SOvGridNodeData *getNodeFromWindow(PHLWINDOW);
  SOldLayoutRecordNodeData *getOldLayoutRecordNodeFromWindow(PHLWINDOW);
  void resizeNodeSizePos(SOvGridNodeData *, int, int, int, int);
  void moveWindowToSourceWorkspace();
  void changeToActivceSourceWorkspace();
  void removeOldLayoutData(PHLWINDOW pWindow);
  std::list<SOvGridNodeData> m_lOvGridNodesData;
  std::list<SOldLayoutRecordNodeData> m_lSOldLayoutRecordNodeData;
};
