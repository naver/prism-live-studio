
$ProductName = "OBS-Studio"
$CheckoutDir = Resolve-Path -Path "$PSScriptRoot\..\..\src\obs-studio"
$DepsBuildDir = "${CheckoutDir}/../obs-build-dependencies"
$ObsBuildDir = "${CheckoutDir}/build"

. ${CheckoutDir}/CI/include/build_support_windows.ps1

"$WindowsDepsVersion;$WindowsVlcVersion;$WindowsCefVersion"