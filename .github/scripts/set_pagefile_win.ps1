param(
    [System.UInt64] $Size = 16gb,
    [System.String] $Drive = "C:"
)

Write-Host "Configuring $Size ($([math]::Round($Size/1GB, 2)) GB) pagefile on $Drive..."

# Configure WMI setting for pagefile
try {
    Set-CimInstance -Query "Select * from Win32_ComputerSystem" -Property @{AutomaticManagedPagefile = $False} -ErrorAction SilentlyContinue
    $pf = Get-CimInstance Win32_PageFileSetting -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "$Drive*" }
    $sizeMB = [int]($Size / 1MB)
    if ($pf) {
        $pf | Set-CimInstance -Property @{InitialSize = $sizeMB; MaximumSize = $sizeMB} -ErrorAction SilentlyContinue
    } else {
        New-CimInstance -ClassName Win32_PageFileSetting -Property @{Name = "$Drive\pagefile.sys"; InitialSize = $sizeMB; MaximumSize = $sizeMB} -ErrorAction SilentlyContinue
    }
} catch {
    Write-Warning "WMI PageFile configuration notice: $_"
}

# Create / expand runtime paging file using Native NT API (pagefile2.sys to avoid collision with locked default pagefile.sys)
$source = @'
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace Util
{
    public class PageFile
    {
        [StructLayout(LayoutKind.Sequential)]
        struct LUID { internal uint LowPart; internal uint HighPart; }
        [StructLayout(LayoutKind.Sequential)]
        struct LUID_AND_ATTRIBUTES { internal LUID Luid; internal uint Attributes; }
        [StructLayout(LayoutKind.Sequential)]
        struct TOKEN_PRIVILEGE { internal uint PrivilegeCount; internal LUID_AND_ATTRIBUTES Privilege; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        struct UNICODE_STRING { internal ushort length; internal ushort maximumLength; internal string buffer; }

        [DllImport("advapi32.dll", ExactSpelling = true, CharSet = CharSet.Unicode, SetLastError = true)]
        static extern bool LookupPrivilegeValueW(string lpSystemName, string lpName, out LUID luid);

        [DllImport("advapi32.dll", SetLastError = true)]
        static extern bool AdjustTokenPrivileges(IntPtr tokenHandle, bool disableAll, ref TOKEN_PRIVILEGE newState, uint bufferLen, IntPtr prev, IntPtr prevLen);

        [DllImport("advapi32.dll", ExactSpelling = true, SetLastError = true)]
        static extern bool OpenProcessToken(IntPtr processHandle, int desiredAccess, out IntPtr tokenHandle);

        [DllImport("kernel32.dll", ExactSpelling = true, SetLastError = true)]
        static extern bool CloseHandle(IntPtr handle);

        [DllImport("ntdll.dll", CharSet = CharSet.Unicode)]
        static extern int NtCreatePagingFile(ref UNICODE_STRING pageFileName, ref long minimumSize, ref long maximumSize, uint flags);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        static extern uint QueryDosDeviceW(string lpDeviceName, StringBuilder lpTargetPath, int ucchMax);

        public static int Create(long minSize, long maxSize, string driveLetter, string fileName)
        {
            LookupPrivilegeValueW(null, "SeCreatePagefilePrivilege", out LUID luid);
            OpenProcessToken(Process.GetCurrentProcess().Handle, 0x0020 | 0x0008, out IntPtr hToken);
            TOKEN_PRIVILEGE tp = new TOKEN_PRIVILEGE { PrivilegeCount = 1, Privilege = new LUID_AND_ATTRIBUTES { Luid = luid, Attributes = 2 } };
            AdjustTokenPrivileges(hToken, false, ref tp, 0, IntPtr.Zero, IntPtr.Zero);
            CloseHandle(hToken);

            StringBuilder volumePath = new StringBuilder(260);
            QueryDosDeviceW(driveLetter, volumePath, volumePath.Capacity);

            string pageFilePath = volumePath.ToString() + "\\" + fileName;
            UNICODE_STRING uStr = new UNICODE_STRING {
                length = (ushort)(pageFilePath.Length * 2),
                maximumLength = (ushort)(2 * (pageFilePath.Length + 1)),
                buffer = pageFilePath
            };

            return NtCreatePagingFile(ref uStr, ref minSize, ref maxSize, 0);
        }
    }
}
'@

try {
    Add-Type -TypeDefinition $source -ErrorAction Stop
    $min = [long]$Size
    $max = [long]$Size
    
    # Try pagefile2.sys first to avoid conflict with existing default locked C:\pagefile.sys
    $res = [Util.PageFile]::Create($min, $max, $Drive, "pagefile2.sys")
    if ($res -ne 0) {
        Write-Host "Trying pagefile.sys (NTSTATUS: 0x$($res.ToString('X')))..."
        $res = [Util.PageFile]::Create($min, $max, $Drive, "pagefile.sys")
    }
    
    if ($res -eq 0) {
        Write-Host "Pagefile successfully attached dynamically to $Drive."
    } else {
        Write-Warning "NtCreatePagingFile returned NTSTATUS: 0x$($res.ToString('X')) (fallback to WMI settings)."
    }
} catch {
    Write-Warning "Could not dynamically create pagefile: $_"
}

# Print memory status
Get-CimInstance Win32_OperatingSystem | Select-Object @{Name="TotalVisibleMemory(GB)";Expression={[math]::Round($_.TotalVisibleMemorySize/1MB,2)}}, @{Name="FreePhysicalMemory(GB)";Expression={[math]::Round($_.FreePhysicalMemory/1MB,2)}}, @{Name="TotalVirtualMemory(GB)";Expression={[math]::Round($_.TotalVirtualMemorySize/1MB,2)}}, @{Name="FreeVirtualMemory(GB)";Expression={[math]::Round($_.FreeVirtualMemory/1MB,2)}} | Format-List
