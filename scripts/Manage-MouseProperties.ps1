param(
 [ValidateSet('Plan','Install','Remove')][string]$Action='Plan',
 [string]$TargetUserSid,
 [switch]$Elevated
)
$ErrorActionPreference='Stop'
if(-not [Environment]::Is64BitProcess){throw 'Use 64-bit Windows PowerShell or PowerShell.'}
$launcherOutput=Split-Path $PSScriptRoot -Parent
$launcherGuid='{DDBD1A52-717F-43AC-AC2A-73262E8A31B8}'
$launcherClass='Software\Classes\CLSID\'+$launcherGuid
$launcherHandler='SOFTWARE\Microsoft\Windows\CurrentVersion\Controls Folder\Mouse\shellex\PropertySheetHandlers\TouchpadStudio'
$launcherDll=Join-Path $launcherOutput 'TouchpadStudio.MouseProperties.dll'
$launcherExe=Join-Path $launcherOutput 'TouchpadStudio.exe'
$launcherBackup=Join-Path $launcherOutput 'TouchpadStudio-registration-backup.json'
$launcherResult=Join-Path $launcherOutput 'TouchpadStudio-registration-result.json'
if(-not $TargetUserSid){$TargetUserSid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value}
$null=New-Object Security.Principal.SecurityIdentifier($TargetUserSid)
if($Action -eq 'Plan'){
 [pscustomobject]@{
  MachineKey='HKLM\'+$launcherHandler
  UserCOMKey='HKEY_USERS\'+$TargetUserSid+'\'+$launcherClass
  DLL=$launcherDll
  Application=$launcherExe
  Backup=$launcherBackup
  AdministratorPrompt=$true
  ChangesDrivers=$false
  ChangesGestureSettings=$false
 }
 return
}
$launcherAdmin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if(-not $launcherAdmin){
 if($Elevated){throw 'Administrator approval was not granted.'}
 # The SID is passed explicitly so alternate administrator credentials cannot
 # accidentally register COM for the wrong Windows account.
 $launcherArguments='-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -Action '+$Action+' -TargetUserSid '+$TargetUserSid+' -Elevated'
 $launcherProcess=Start-Process -FilePath "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $launcherArguments -Verb RunAs -WindowStyle Hidden -PassThru -Wait
 if(Test-Path -LiteralPath $launcherResult){Get-Content -LiteralPath $launcherResult}
 if($launcherProcess.ExitCode -ne 0){throw 'The launcher registration action did not complete. See the result file, or approve the Windows administrator prompt.'}
 return
}
$launcherStatus=[ordered]@{Action=$Action;Success=$false;UserSID=$TargetUserSid;Time=(Get-Date).ToString('o');Message=$null}
$launcherUser=$null
try{
 $launcherUser=[Microsoft.Win32.Registry]::Users.OpenSubKey($TargetUserSid,$true)
 if(-not $launcherUser){throw 'The intended Windows user hive is unavailable. No registration changed.'}
 $launcherMachine=[Microsoft.Win32.Registry]::LocalMachine
 if($Action -eq 'Install'){
  foreach($launcherFile in @($launcherDll,$launcherExe)){if(-not(Test-Path -LiteralPath $launcherFile -PathType Leaf)){throw "Required file missing: $launcherFile"}}
  $launcherExistingUser=$launcherUser.OpenSubKey($launcherClass)
  $launcherExistingMachine=$launcherMachine.OpenSubKey($launcherHandler)
  $launcherConflict=$null -ne $launcherExistingUser -or $null -ne $launcherExistingMachine
  if($launcherExistingUser){$launcherExistingUser.Dispose()}
  if($launcherExistingMachine){$launcherExistingMachine.Dispose()}
  if($launcherConflict){throw 'A reserved launcher registration key already exists. Nothing overwritten. Remove the previous installation first.'}
  if(Test-Path -LiteralPath $launcherBackup){throw 'A registration backup already exists. It has not been overwritten.'}
  $launcherSnapshot=[ordered]@{
   Format='TouchpadStudio-MouseLauncher-2'
   UserSID=$TargetUserSid
   Recorded=(Get-Date).ToString('o')
   CLSID=$launcherGuid
   ClassKey=$launcherClass
   HandlerKey=$launcherHandler
   ClassExisted=$false
   HandlerExisted=$false
   HandlerScope='LocalMachine'
   DLL=$launcherDll
   DLLSHA256=(Get-FileHash -LiteralPath $launcherDll -Algorithm SHA256).Hash
   Application=$launcherExe
  }
  $launcherSnapshot | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $launcherBackup -Encoding UTF8
  $launcherVerified=Get-Content -LiteralPath $launcherBackup -Raw | ConvertFrom-Json
  if($launcherVerified.DLLSHA256 -ne $launcherSnapshot.DLLSHA256 -or $launcherVerified.ClassExisted -or $launcherVerified.HandlerExisted){throw 'Backup verification failed. No registry changes made.'}
  $launcherMadeClass=$false;$launcherMadeHandler=$false
  try{
   $launcherMadeClass=$true
   $launcherKey=$launcherUser.CreateSubKey($launcherClass)
   try{$launcherKey.SetValue('','Touchpad Studio Mouse Properties launcher',[Microsoft.Win32.RegistryValueKind]::String)}finally{$launcherKey.Dispose()}
   $launcherKey=$launcherUser.CreateSubKey($launcherClass+'\InprocServer32')
   try{$launcherKey.SetValue('',$launcherDll,[Microsoft.Win32.RegistryValueKind]::String);$launcherKey.SetValue('ThreadingModel','Apartment',[Microsoft.Win32.RegistryValueKind]::String)}finally{$launcherKey.Dispose()}
   $launcherMadeHandler=$true
   $launcherKey=$launcherMachine.CreateSubKey($launcherHandler)
   try{$launcherKey.SetValue('',$launcherGuid,[Microsoft.Win32.RegistryValueKind]::String)}finally{$launcherKey.Dispose()}
   $launcherKey=$launcherMachine.OpenSubKey($launcherHandler)
   try{if($launcherKey.GetValue('') -ne $launcherGuid){throw 'Handler registration did not verify'}}finally{$launcherKey.Dispose()}
  }catch{
   if($launcherMadeHandler){$launcherMachine.DeleteSubKeyTree($launcherHandler,$false)}
   if($launcherMadeClass){$launcherUser.DeleteSubKeyTree($launcherClass,$false)}
   throw
  }
  $launcherStatus.Success=$true
  $launcherStatus.Message='Installed. Close and reopen Mouse Properties to see the Touchpad Studio tab. No driver or gesture settings changed.'
 }else{
  if(-not(Test-Path -LiteralPath $launcherBackup)){throw 'The registration backup is missing. Nothing removed.'}
  $launcherSaved=Get-Content -LiteralPath $launcherBackup -Raw | ConvertFrom-Json
  if($launcherSaved.Format -ne 'TouchpadStudio-MouseLauncher-2' -or $launcherSaved.UserSID -ne $TargetUserSid -or $launcherSaved.CLSID -ne $launcherGuid -or $launcherSaved.ClassKey -ne $launcherClass -or $launcherSaved.HandlerKey -ne $launcherHandler -or $launcherSaved.ClassExisted -or $launcherSaved.HandlerExisted -or $launcherSaved.HandlerScope -ne 'LocalMachine'){throw 'The backup does not match this registration. Nothing removed.'}
  $launcherKey=$launcherMachine.OpenSubKey($launcherHandler)
  if($launcherKey){try{if($launcherKey.GetValue('') -ne $launcherGuid){throw 'The handler was changed elsewhere. Nothing removed.'}}finally{$launcherKey.Dispose()}}
  $launcherKey=$launcherUser.OpenSubKey($launcherClass+'\InprocServer32')
  if($launcherKey){try{if($launcherKey.GetValue('') -ine $launcherSaved.DLL){throw 'The COM registration now points elsewhere. Nothing removed.'}}finally{$launcherKey.Dispose()}}
  # Delete only our reserved subtrees, restoring their saved absent state.
  $launcherMachine.DeleteSubKeyTree($launcherHandler,$false)
  $launcherUser.DeleteSubKeyTree($launcherClass,$false)
  Move-Item -LiteralPath $launcherBackup -Destination ($launcherBackup+'.removed-'+(Get-Date -Format 'yyyyMMddHHmmss'))
  $launcherStatus.Success=$true
  $launcherStatus.Message='Removed only the Mouse Properties launcher registration. The application, drivers and gesture settings are unchanged.'
 }
}catch{
 $launcherStatus.Message=$_.Exception.Message
}finally{
 if($launcherUser){$launcherUser.Dispose()}
 $launcherStatus | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $launcherResult -Encoding UTF8
}
if(-not $launcherStatus.Success){exit 1}
