Write-Host "=== Disk usage before cleanup ==="
Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'" | Select-Object DeviceID, @{Name="Size(GB)";Expression={[math]::round($_.Size/1GB,2)}}, @{Name="FreeSpace(GB)";Expression={[math]::round($_.FreeSpace/1GB,2)}} | Out-String
Write-Host ""

Write-Host "Stopping database and server services..."
$services = @("postgresql*", "mysql*", "mssql*", "SQLWriter", "SQLBrowser")
foreach ($serviceName in $services) {
    Get-Service -Name $serviceName -ErrorAction SilentlyContinue | Stop-Service -Force -ErrorAction SilentlyContinue
}

function Free-Space {
    param ([string]$Path)
    if (Test-Path $Path) {
        Write-Host "Removing $Path..."
        try {
            Remove-Item -Path $Path -Recurse -Force -ErrorAction Stop
        } catch {
            Write-Warning "First attempt failed for $Path. Trying to take ownership and grant permissions..."
            cmd.exe /c "takeown /F `"$Path`" /A /R /D Y >nul 2>&1"
            cmd.exe /c "icacls `"$Path`" /grant *S-1-5-32-544:F /T /C /Q >nul 2>&1"
            cmd.exe /c "attrib -R -H -S `"$Path\*`" /S /D >nul 2>&1"
            try {
                Remove-Item -Path $Path -Recurse -Force -ErrorAction Stop
            } catch {
                Write-Warning "Could not fully remove ${Path}: $_"
            }
        }
    }
}

$pathsToFree = @(
    "C:\Program Files\dotnet",
    "C:\Program Files\MySQL",
    "C:\Program Files\PostgreSQL",
    "C:\Program Files\MongoDB",
    "C:\Program Files (x86)\Microsoft SDKs\Azure",
    "C:\Program Files\Google\Chrome",
    "C:\Program Files (x86)\Google",
    "C:\Program Files\Mozilla Firefox",
    "C:\Selenium",
    "C:\Program Files (x86)\Microsoft SQL Server",
    "C:\Program Files\Microsoft SQL Server",
    "C:\mysql",
    "C:\Program Files\LibreOffice",
    "C:\Program Files\Android",
    "C:\Android",
    "C:\Program Files (x86)\Windows Kits\10\Testing",
    "C:\Program Files\Julia",
    "C:\Program Files\CodeQL",
    "C:\Program Files\LLVM",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019",
    "C:\pipx",
    "C:\msys64",
    "C:\Strawberry",
    "C:\Program Files\Boost",
    "C:\Program Files\Amazon",
    "C:\Program Files (x86)\Microsoft SDKs\NuGetPackages"
)

Write-Host "Clearing package caches..."
if (Get-Command nuget -ErrorAction SilentlyContinue) { nuget locals all -clear 2>$null }
if (Get-Command npm -ErrorAction SilentlyContinue) { npm cache clean --force 2>$null }

foreach ($path in $pathsToFree) {
    Free-Space $path
}

$toolcache = $env:AGENT_TOOLSDIRECTORY
if (-not $toolcache) {
    $toolcache = "C:\hostedtoolcache\windows"
}
if (Test-Path $toolcache) {
    Get-ChildItem $toolcache | Where-Object { $_.Name -ne "Python" } | ForEach-Object {
        Free-Space $_.FullName
    }
}

Write-Host ""
Write-Host "=== Disk usage after cleanup ==="
Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'" | Select-Object DeviceID, @{Name="Size(GB)";Expression={[math]::round($_.Size/1GB,2)}}, @{Name="FreeSpace(GB)";Expression={[math]::round($_.FreeSpace/1GB,2)}} | Out-String
