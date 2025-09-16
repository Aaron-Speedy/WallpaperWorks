!include "..\build\app_name.nsh"

Name "${APP_NAME} Installer"
OutFile "../build/${APP_NAME}Installer.exe"
InstallDir $PROGRAMFILES\${APP_NAME}
Icon "../build/favicon.ico"

InstallDirRegKey HKLM "Software\${APP_NAME}" "Install_Dir"

RequestExecutionLevel admin

;--------------------------------
; Pages

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "${APP_NAME} (required)"
    SectionIn RO

    SetOutPath $INSTDIR
    File "..\build\${APP_NAME}.exe"
    CreateDirectory "$INSTDIR\${APP_NAME}"
    SetOutPath "$INSTDIR\${APP_NAME}"

    File /r "..\build\*.*"

    ; Write the installation path into the registry
    WriteRegStr HKLM SOFTWARE\${APP_NAME} "Install_Dir" "$INSTDIR"

    ; Write the uninstall keys for Windows
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoRepair" 1
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts (required)"
    SectionIn RO

    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_NAME}.exe" "" "$INSTDIR\${APP_NAME}.exe" 0
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    DeleteRegKey HKLM SOFTWARE\${APP_NAME}

    RMDir /r "$INSTDIR\${APP_NAME}"
    Delete "$INSTDIR\${APP_NAME}.exe"
    Delete "$INSTDIR\uninstall.exe"

    ; Remove shortcuts, if any
    Delete "$SMPROGRAMS\${APP_NAME}\*.*"

    ; Remove directories used (only deletes empty dirs)
    RMDir "$SMPROGRAMS\${APP_NAME}"
    RMDir "$INSTDIR"
SectionEnd
