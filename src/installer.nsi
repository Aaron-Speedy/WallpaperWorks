!define NAME "Wallpaper"
!define EXE_NAME "wallpaper"

Name "${NAME} Installer"
OutFile "../build/${EXE_NAME}_installer.exe"
InstallDir $PROGRAMFILES\${NAME}
; Icon "../build/icon.ico"

InstallDirRegKey HKLM "Software\${NAME}" "Install_Dir"

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
Section "${NAME} (required)"
    SectionIn RO

    SetOutPath $INSTDIR
    File "..\build\${EXE_NAME}.exe"
    CreateDirectory "$INSTDIR\${NAME}"
    SetOutPath "$INSTDIR\${NAME}"

    File /r "..\build\*.*"

    ; Write the installation path into the registry
    WriteRegStr HKLM SOFTWARE\${NAME} "Install_Dir" "$INSTDIR"

    ; Write the uninstall keys for Windows
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "DisplayName" "${NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "NoRepair" 1
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts (required)"
    SectionIn RO

    CreateDirectory "$SMPROGRAMS\${NAME}"
    CreateShortcut "$SMPROGRAMS\${NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
    CreateShortcut "$SMPROGRAMS\${NAME}\${NAME}.lnk" "$INSTDIR\${EXE_NAME}.exe" "" "$INSTDIR\${NAME}.exe" 0
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"
    DeleteRegKey HKLM SOFTWARE\${NAME}

    RMDir /r "$INSTDIR\${NAME}"
    Delete "$INSTDIR\${EXE_NAME}.exe"
    Delete "$INSTDIR\uninstall.exe"

    ; Remove shortcuts, if any
    Delete "$SMPROGRAMS\${NAME}\*.*"

    ; Remove directories used (only deletes empty dirs)
    RMDir "$SMPROGRAMS\${NAME}"
    RMDir "$INSTDIR"
SectionEnd
