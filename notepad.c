#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IDC_EDIT 1001

static HWND hMainWnd = NULL;
static HWND hEdit = NULL;
static int ignoreChange = 0;
static HFONT hEditorFont = NULL;
static HACCEL hAccel = NULL;

#define MAX_TABS 16
static char filenames[MAX_TABS][MAX_PATH];
static char *buffers[MAX_TABS];
static int buffer_lens[MAX_TABS];
static int modifieds[MAX_TABS];
static int tabCount = 0;
static int activeTab = -1;

// helper: save current edit contents into active slot
void SaveCurrentEditToSlot()
{
  if (activeTab < 0)
    return;
  int len = GetWindowTextLengthA(hEdit);
  char *buf = (char *)malloc(len + 1);
  if (!buf)
    return;
  GetWindowTextA(hEdit, buf, len + 1);
  free(buffers[activeTab]);
  buffers[activeTab] = buf;
  buffer_lens[activeTab] = len;
  modifieds[activeTab] = (SendMessage(hEdit, EM_GETMODIFY, 0, 0) != 0);
}

// helper: load slot into edit control
void LoadSlotToEdit(int idx)
{
  if (idx < 0 || idx >= tabCount)
    return;
  ignoreChange = 1;
  SetWindowTextA(hEdit, buffers[idx] ? buffers[idx] : "");
  ignoreChange = 0;
  SendMessage(hEdit, EM_SETMODIFY, modifieds[idx] ? TRUE : FALSE, 0);
}

// create a new empty tab and switch to it
void CreateNewTabAndSwitch()
{
  if (tabCount >= MAX_TABS)
  {
    MessageBoxA(hMainWnd, "Maximum tabs reached.", "Error", MB_OK | MB_ICONERROR);
    return;
  }
  if (activeTab >= 0)
    SaveCurrentEditToSlot();
  buffers[tabCount] = (char *)malloc(1);
  if (buffers[tabCount])
    buffers[tabCount][0] = '\0';
  buffer_lens[tabCount] = 0;
  filenames[tabCount][0] = '\0';
  modifieds[tabCount] = 0;
  activeTab = tabCount;
  tabCount++;
  LoadSlotToEdit(activeTab);
  UpdateTitle();
}

// switch to an existing tab index
void SwitchToTab(int idx)
{
  if (idx < 0 || idx >= tabCount)
    return;
  if (idx == activeTab)
    return;
  if (activeTab >= 0)
    SaveCurrentEditToSlot();
  activeTab = idx;
  LoadSlotToEdit(activeTab);
  UpdateTitle();
}

void UpdateTitle()
{
  char title[MAX_PATH + 64];
  const char *name = "Untitled";
  if (activeTab >= 0 && filenames[activeTab][0] != '\0')
    name = filenames[activeTab];
  if (activeTab >= 0)
  {
    if (modifieds[activeTab])
      snprintf(title, sizeof(title), "%s - Notepad* (%d/%d)", name, activeTab + 1, tabCount);
    else
      snprintf(title, sizeof(title), "%s - Notepad (%d/%d)", name, activeTab + 1, tabCount);
  }
  else
  {
    snprintf(title, sizeof(title), "%s - Notepad", name);
  }
  SetWindowTextA(hMainWnd, title);
}

int PromptSaveIfNeeded()
{
  if (activeTab < 0)
    return IDOK;
  if (!modifieds[activeTab])
    return IDOK;
  int r = MessageBoxA(hMainWnd, "The text has changed. Do you want to save changes?", "Notepad", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (r == IDCANCEL)
    return IDCANCEL;
  if (r == IDNO)
    return IDNO;
  return IDYES;
}

int SaveToFileIndex(int idx, const char *fname)
{
  if (idx < 0 || idx >= tabCount)
    return 0;
  FILE *f = fopen(fname, "wb");
  if (!f)
    return 0;
  // ensure buffer up-to-date
  if (idx == activeTab)
    SaveCurrentEditToSlot();
  if (buffers[idx] && buffer_lens[idx] > 0)
    fwrite(buffers[idx], 1, buffer_lens[idx], f);
  fclose(f);
  strncpy(filenames[idx], fname, MAX_PATH - 1);
  filenames[idx][MAX_PATH - 1] = '\0';
  modifieds[idx] = 0;
  if (idx == activeTab)
    SendMessage(hEdit, EM_SETMODIFY, FALSE, 0);
  UpdateTitle();
  return 1;
}

int SaveToFile(const char *fname)
{
  return SaveToFileIndex(activeTab, fname);
}

int DoSaveAs()
{
  char fname[MAX_PATH] = "";
  OPENFILENAMEA ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hMainWnd;
  ofn.lpstrFile = fname;
  ofn.nMaxFile = sizeof(fname);
  ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  ofn.lpstrDefExt = "txt";
  if (GetSaveFileNameA(&ofn))
  {
    return SaveToFileIndex(activeTab, fname);
  }
  return 0;
}

int DoSave()
{
  if (activeTab < 0)
    return 0;
  if (filenames[activeTab][0] == '\0')
    return DoSaveAs();
  return SaveToFileIndex(activeTab, filenames[activeTab]);
}

int DoOpen()
{
  int res = PromptSaveIfNeeded();
  if (res == IDCANCEL)
    return 0;
  if (res == IDYES)
  {
    if (!DoSave())
      return 0;
  }

  char fname[MAX_PATH] = "";
  OPENFILENAMEA ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hMainWnd;
  ofn.lpstrFile = fname;
  ofn.nMaxFile = sizeof(fname);
  ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (GetOpenFileNameA(&ofn))
  {
    FILE *f = fopen(fname, "rb");
    if (!f)
    {
      MessageBoxA(hMainWnd, "Failed to open file.", "Error", MB_OK | MB_ICONERROR);
      return 0;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(sz + 1);
    if (!buf)
    {
      fclose(f);
      MessageBoxA(hMainWnd, "Out of memory.", "Error", MB_OK | MB_ICONERROR);
      return 0;
    }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    // open in a new tab
    if (tabCount >= MAX_TABS)
    {
      free(buf);
      MessageBoxA(hMainWnd, "Too many tabs open.", "Error", MB_OK | MB_ICONERROR);
      return 0;
    }
    if (activeTab >= 0)
      SaveCurrentEditToSlot();
    buffers[tabCount] = buf;
    buffer_lens[tabCount] = (int)sz;
    strncpy(filenames[tabCount], fname, MAX_PATH - 1);
    filenames[tabCount][MAX_PATH - 1] = '\0';
    modifieds[tabCount] = 0;
    activeTab = tabCount;
    tabCount++;
    LoadSlotToEdit(activeTab);
    UpdateTitle();
    return 1;
  }
  return 0;
}

void DoNew()
{
  int res = PromptSaveIfNeeded();
  if (res == IDCANCEL)
    return;
  if (res == IDYES)
  {
    if (!DoSave())
      return;
  }
  // replace current tab with empty document
  if (activeTab < 0)
    CreateNewTabAndSwitch();
  else
  {
    free(buffers[activeTab]);
    buffers[activeTab] = (char *)malloc(1);
    if (buffers[activeTab])
      buffers[activeTab][0] = '\0';
    buffer_lens[activeTab] = 0;
    filenames[activeTab][0] = '\0';
    modifieds[activeTab] = 0;
    ignoreChange = 1;
    SetWindowTextA(hEdit, "");
    ignoreChange = 0;
    SendMessage(hEdit, EM_SETMODIFY, FALSE, 0);
    UpdateTitle();
  }
}

void DoCut() { SendMessage(hEdit, WM_CUT, 0, 0); }
void DoCopy() { SendMessage(hEdit, WM_COPY, 0, 0); }
void DoPaste() { SendMessage(hEdit, WM_PASTE, 0, 0); }
void DoUndo() { SendMessage(hEdit, EM_UNDO, 0, 0); }
void DoSelectAll() { SendMessage(hEdit, EM_SETSEL, 0, -1); }

void DoChooseFont()
{
  CHOOSEFONTA cf;
  LOGFONTA lf;
  memset(&lf, 0, sizeof(lf));
  if (hEditorFont)
  {
    LOGFONT lftemp;
    GetObjectA(hEditorFont, sizeof(lftemp), &lftemp);
    memcpy(&lf, &lftemp, sizeof(lf));
  }
  else
  {
    strncpy(lf.lfFaceName, "Consolas", LF_FACESIZE - 1);
    lf.lfHeight = -12;
  }
  ZeroMemory(&cf, sizeof(cf));
  cf.lStructSize = sizeof(cf);
  cf.hwndOwner = hMainWnd;
  cf.lpLogFont = &lf;
  cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
  if (ChooseFontA(&cf))
  {
    HFONT hNew = CreateFontIndirectA(&lf);
    if (hNew)
    {
      if (hEditorFont)
        DeleteObject(hEditorFont);
      hEditorFont = hNew;
      SendMessage(hEdit, WM_SETFONT, (WPARAM)hEditorFont, TRUE);
    }
  }
}

void DoAbout()
{
  MessageBoxA(hMainWnd, "Simple Notepad clone\n(c) Minimal Example", "About Notepad", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_CREATE:
  {
    hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
                            0, 0, 0, 0, hwnd, (HMENU)IDC_EDIT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
    if (!hEdit)
    {
      MessageBoxA(NULL, "Failed to create edit control.", "Error", MB_OK | MB_ICONERROR);
      return -1;
    }
    HDC hdc = GetDC(hEdit);
    int lfHeight = -MulDiv(10, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hEdit, hdc);
    LOGFONTA lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = lfHeight;
    lf.lfWeight = FW_NORMAL;
    strncpy(lf.lfFaceName, "Consolas", LF_FACESIZE - 1);
    hEditorFont = CreateFontIndirectA(&lf);
    if (hEditorFont)
      SendMessage(hEdit, WM_SETFONT, (WPARAM)hEditorFont, TRUE);
    // initialize first tab
    for (int i = 0; i < MAX_TABS; ++i)
    {
      buffers[i] = NULL;
      buffer_lens[i] = 0;
      filenames[i][0] = '\0';
      modifieds[i] = 0;
    }
    tabCount = 0;
    activeTab = -1;
    CreateNewTabAndSwitch();
  }
  break;
  case WM_SIZE:
    if (hEdit)
    {
      SetWindowPos(hEdit, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
    }
    break;
  case WM_COMMAND:
  {
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);
    if (id == IDC_EDIT && code == EN_CHANGE)
    {
      if (!ignoreChange && activeTab >= 0)
      {
        modifieds[activeTab] = 1;
        UpdateTitle();
      }
    }
    switch (id)
    {
    case 1:
      DoNew();
      break; // File->New
    case 6:
      CreateNewTabAndSwitch();
      break; // File->New Tab
    case 7:
      // Close tab
      if (activeTab < 0)
        break;
      {
        int r = PromptSaveIfNeeded();
        if (r == IDCANCEL)
          break;
        if (r == IDYES)
        {
          if (!DoSave())
            break;
        }
        // free current
        free(buffers[activeTab]);
        buffers[activeTab] = NULL;
        // shift remaining
        for (int k = activeTab + 1; k < tabCount; ++k)
        {
          buffers[k - 1] = buffers[k];
          buffer_lens[k - 1] = buffer_lens[k];
          modifieds[k - 1] = modifieds[k];
          memcpy(filenames[k - 1], filenames[k], MAX_PATH);
        }
        tabCount--;
        if (tabCount == 0)
        {
          CreateNewTabAndSwitch();
        }
        else
        {
          if (activeTab >= tabCount)
            activeTab = tabCount - 1;
          LoadSlotToEdit(activeTab);
          UpdateTitle();
        }
      }
      break;
    case 2:
      DoOpen();
      break; // File->Open
    case 3:
      DoSave();
      break; // File->Save
    case 4:
      DoSaveAs();
      break; // File->Save As
    case 5:
      PostMessage(hwnd, WM_CLOSE, 0, 0);
      break; // File->Exit
    case 10:
      DoUndo();
      break; // Edit->Undo
    case 11:
      DoCut();
      break;
    case 12:
      DoCopy();
      break;
    case 13:
      DoPaste();
      break;
    case 14:
      DoSelectAll();
      break;
    case 20:
      DoChooseFont();
      break; // Format->Font
    case 30:
      DoAbout();
      break; // Help->About
    }
  }
  break;
  case WM_CLOSE:
  {
    int res = PromptSaveIfNeeded();
    if (res == IDCANCEL)
      return 0;
    if (res == IDYES)
    {
      if (!DoSave())
        return 0;
    }
    DestroyWindow(hwnd);
  }
  break;
  case WM_DESTROY:
    if (hEditorFont)
      DeleteObject(hEditorFont);
    // free buffers
    for (int i = 0; i < tabCount; ++i)
    {
      free(buffers[i]);
      buffers[i] = NULL;
    }
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
  return 0;
}

HMENU CreateMenus()
{
  HMENU hMenubar = CreateMenu();
  HMENU hFile = CreateMenu();
  HMENU hEdit = CreateMenu();
  HMENU hHelp = CreateMenu();
  HMENU hFormat = CreateMenu();

  AppendMenuA(hFile, MF_STRING, 1, "&New\tCtrl+N");
  AppendMenuA(hFile, MF_STRING, 6, "New &Tab\tCtrl+T");
  AppendMenuA(hFile, MF_STRING, 2, "&Open...\tCtrl+O");
  AppendMenuA(hFile, MF_STRING, 3, "&Save\tCtrl+S");
  AppendMenuA(hFile, MF_STRING, 7, "Close Ta&b\tCtrl+W");
  AppendMenuA(hFile, MF_STRING, 4, "Save &As...");
  AppendMenuA(hFile, MF_SEPARATOR, 0, NULL);
  AppendMenuA(hFile, MF_STRING, 5, "E&xit");

  AppendMenuA(hEdit, MF_STRING, 10, "&Undo\tCtrl+Z");
  AppendMenuA(hEdit, MF_SEPARATOR, 0, NULL);
  AppendMenuA(hEdit, MF_STRING, 11, "Cu&t\tCtrl+X");
  AppendMenuA(hEdit, MF_STRING, 12, "&Copy\tCtrl+C");
  AppendMenuA(hEdit, MF_STRING, 13, "&Paste\tCtrl+V");
  AppendMenuA(hEdit, MF_SEPARATOR, 0, NULL);
  AppendMenuA(hEdit, MF_STRING, 14, "Select &All\tCtrl+A");

  AppendMenuA(hFormat, MF_STRING, 20, "&Font...");

  AppendMenuA(hHelp, MF_STRING, 30, "&About\tF1");

  AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hFile, "&File");
  AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hEdit, "&Edit");
  AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hFormat, "&Format");
  AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hHelp, "&Help");

  return hMenubar;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  WNDCLASSA wc = {0};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = "NotepadClass";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  if (!RegisterClassA(&wc))
    return 0;

  hMainWnd = CreateWindowExA(0, wc.lpszClassName, "Untitled - Notepad",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                             NULL, NULL, hInstance, NULL);
  if (!hMainWnd)
    return 0;

  HMENU menus = CreateMenus();
  SetMenu(hMainWnd, menus);

  // Accelerator table
  ACCEL accels[] = {
      {FVIRTKEY | FCONTROL, 'N', 1},  // Ctrl+N -> id 1 (New)
      {FVIRTKEY | FCONTROL, 'T', 6},  // Ctrl+T -> New Tab
      {FVIRTKEY | FCONTROL, 'W', 7},  // Ctrl+W -> Close Tab
      {FVIRTKEY | FCONTROL, 'O', 2},  // Ctrl+O -> Open
      {FVIRTKEY | FCONTROL, 'S', 3},  // Ctrl+S -> Save
      {FVIRTKEY | FCONTROL, 'Z', 10}, // Ctrl+Z -> Undo
      {FVIRTKEY | FCONTROL, 'X', 11}, // Cut
      {FVIRTKEY | FCONTROL, 'C', 12}, // Copy
      {FVIRTKEY | FCONTROL, 'V', 13}, // Paste
      {FVIRTKEY | FCONTROL, 'A', 14}, // Select All
      {FVIRTKEY, VK_F1, 30}           // F1 -> About
  };
  hAccel = CreateAcceleratorTableA(accels, sizeof(accels) / sizeof(accels[0]));

  ShowWindow(hMainWnd, nCmdShow);
  UpdateWindow(hMainWnd);

  // Message loop
  MSG msg;
  while (GetMessageA(&msg, NULL, 0, 0))
  {
    if (!TranslateAcceleratorA(hMainWnd, hAccel, &msg))
    {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
  }

  if (hAccel)
    DestroyAcceleratorTable(hAccel);
  return (int)msg.wParam;
}
