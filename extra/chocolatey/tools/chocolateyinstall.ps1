
$url64 = "https://github.com/houmain/keymapper/releases/download/4.4.4/keymapper-4.4.4-win64.msi"
$checksum64 = "AA4457F00B27C10B74FBBDB5EC58CF2C9AA7C550553B21F98A6DE245B3DC1BA2"

Install-ChocolateyPackage -PackageName "keymapper" -FileType "msi" -SilentArgs "/quiet" -Url64bit "$url64" -ChecksumType "sha256" -Checksum64 $checksum64 -validExitCodes @(0,3010)
