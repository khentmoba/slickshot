; SlickShot per-user installer for Windows.
;
; Built by .github/workflows/Windows-pack.yml with:
;   makensis /DVERSION=<x.y.z> /DSTAGING=<cmake --install prefix> ^
;            /DOUTFILE=<setup.exe> packaging\win-installer\SlickShot.nsi
;
; Installs to %LOCALAPPDATA%\Programs\SlickShot, so no administrator rights
; or UAC prompt is needed. The in-app updater runs this file with /S.

Unicode true
SetCompressor /SOLID lzma

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef STAGING
  !error "Pass /DSTAGING=<install prefix> to makensis"
!endif
!ifndef OUTFILE
  !define OUTFILE "SlickShot-setup.exe"
!endif

!define APP_NAME "SlickShot"
!define APP_EXE "bin\flameshot.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

Name "${APP_NAME}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${APP_NAME}"
RequestExecutionLevel user

!include "MUI2.nsh"

!define MUI_ICON "${__FILEDIR__}\..\..\data\img\app\slickshot.ico"
!define MUI_UNICON "${__FILEDIR__}\..\..\data\img\app\slickshot.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${APP_NAME}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Install"
  ; A running SlickShot keeps bin\flameshot.exe locked. Stop here, before the
  ; old install is removed, so a refused install leaves the old one intact.
  IfFileExists "$INSTDIR\${APP_EXE}" +1 +7
    ClearErrors
    FileOpen $0 "$INSTDIR\${APP_EXE}" a
    IfErrors 0 +3
    MessageBox MB_OK|MB_ICONSTOP "SlickShot is running. Close it, then run this installer again." /SD IDOK
    Abort
    FileClose $0

  ; Remove the previous install first so files from an older build don't linger.
  ; The old uninstaller is not run: _?= makes it run in place, where it cannot
  ; delete its own image and leaves uninstall.exe behind. Only clear a folder
  ; that holds a SlickShot install, so a stray /D= cannot wipe an unrelated one.
  IfFileExists "$INSTDIR\uninstall.exe" +2 0
  IfFileExists "$INSTDIR\${APP_EXE}" 0 +2
  RMDir /r "$INSTDIR"

  SetOutPath "$INSTDIR"
  File /r "${STAGING}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateShortcut "$SMPROGRAMS\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "flameshot-org"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "un.Uninstall"
  Delete "$SMPROGRAMS\${APP_NAME}.lnk"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  RMDir /r "$INSTDIR"
SectionEnd
