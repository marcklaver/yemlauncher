#include <filesystem>
#include <windows.h>
#include <string>
#include <vector>
#include <string_view>
#include <cwchar>
#include <cstdlib>
#include "CommandParser.h"
#include "YemHelpers.h"



//-----------------------------------------------------------------------------------------------------------------
// SemVer
// 
// Semantic version structure
//-----------------------------------------------------------------------------------------------------------------
struct SemVer
{
    int major = 0;
    int minor = 0;
    int patch = 0;

    bool isValid          = false;
    bool isSpecialRelease = false;

    static SemVer Parse(const std::wstring& str)
    {
        SemVer sv;
        if (str.find(L"-") != std::wstring::npos)                                                                   // Detect special release marker.
        {
            sv.isSpecialRelease = true;
        }
        
        std::wstring core = str.substr(0, str.find(L"-"));                                                          // Split at '-' to ignore the markers for special releases.
        int a = 0, b = 0, c = 0;
        if (swscanf_s(core.c_str(), L"%d.%d.%d", &a, &b, &c) == 3)
        {
            sv.major = a;
            sv.minor = b;
            sv.patch = c;
            sv.isValid = true;
        }

        return sv;
    }

    bool operator>(const SemVer& other) const
    {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch > other.patch;
    }
};



//-----------------------------------------------------------------------------------------------------------------
// ReadRegistryString
// 
// Use    : Reads a REG_SZ from the registry.
// Input  : root      - Root key (hive).
//          subkey    - Subkey to search under the root (hive).
//          valueName - Value to search
//          out       - Value read from the registry.
// Returns: bool - true on success, false otherwise.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
static bool ReadRegistryString(HKEY root, const wchar_t* subkey, const wchar_t* valueName, std::wstring& out)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        return false;
    }

    wchar_t buffer[1024];
    DWORD size = sizeof(buffer);
    DWORD type = 0;

    LONG res = RegGetValueW(hKey, nullptr, valueName, RRF_RT_REG_SZ, &type, buffer, &size);
    RegCloseKey(hKey);
    if (res != ERROR_SUCCESS)
    {
        return false;
    }

    out.assign(buffer);
    return true;
}



//-----------------------------------------------------------------------------------------------------------------
// ParseVersionFromFolder
// 
// Use    : Extract the semantic version from the folder name.
// Input  : path - Path to parse.
// Returns: SemVer - Semantic version structure with the found information.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
static SemVer ParseVersionFromFolder(const std::wstring& path)
{
    //-------------------------------------------------------------------------------------------------------------
    // Extract folder name
    //-------------------------------------------------------------------------------------------------------------
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
    {
        return {};                                                                                                  // No folder component: invalid
    }
    std::wstring folder = path.substr(pos + 1);

    //-------------------------------------------------------------------------------------------------------------
    // Reject anything containing characters other than digits and dots
    //-------------------------------------------------------------------------------------------------------------
    for (wchar_t ch : folder)
    {
        if ((ch < L'0' || ch > L'9') && ch != L'.')
        {
            return {};                                                                                              // Contains preview/beta/letters/etc: invalid
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    // Count dots to decide which pattern to parse
    //-------------------------------------------------------------------------------------------------------------
    int dotCount = 0;
    for (wchar_t ch : folder)
    {
        if (ch == L'.')
        {
            dotCount++;
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    // Set the version values
    //-------------------------------------------------------------------------------------------------------------
    int a = 0;
    int b = 0;
    int c = 0;
    SemVer sv;

    if (dotCount == 2)
    {
        if (swscanf_s(folder.c_str(), L"%d.%d.%d", &a, &b, &c) == 3)
        {
            sv.major   = a;
            sv.minor   = b;
            sv.patch   = c;
            sv.isValid = true;
            return sv;
        }
        return {};
    }

    if (dotCount == 1)
    {
        if (swscanf_s(folder.c_str(), L"%d.%d", &a, &b) == 2)
        {
            sv.major   = a;
            sv.minor   = b;
            sv.patch   = 0;
            sv.isValid = true;
            return sv;
        }
        return {};
    }

    if (dotCount == 0)
    {
        if (swscanf_s(folder.c_str(), L"%d", &a) == 1)
        {
            sv.major   = a;
            sv.minor   = 0;
            sv.patch   = 0;
            sv.isValid = true;
            return sv;
        }
        return {};
    }
    return {};                                                                                                      // Should never reach here, but safe fallback
}



//-----------------------------------------------------------------------------------------------------------------
// FindPwshViaRegistry
// 
// Use    : Searches the registry for the latest stable version of PowerShell.
// Input  : ---
// Returns: Full path to the found version or an empty string if not found.
// Note(s): 1) Root registry key containing all MSI/Winget-installed PowerShell versions.
//             Each subkey under InstalledVersions is a GUID representing one installation.
//             Inside each subkey:
//                 - InstallLocation     (REG_SZ)  -> folder containing pwsh.exe
//                 - SemanticVersion     (REG_SZ)  -> version string (7.4.1, 7.3.10, etc.)
//
//          2) We enumerate all subkeys, filter out prerelease versions, and select the highest stable version.
//-------------------------------------------------------------------------------------------------------------
static std::wstring FindPwshViaRegistry()
{
    constexpr const wchar_t* rootKey = L"SOFTWARE\\Microsoft\\PowerShellCore\\InstalledVersions";

    //-------------------------------------------------------------------------------------------------------------
    // Open the root key using RAII.
    //-------------------------------------------------------------------------------------------------------------
    HKEY rawRoot = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, rootKey, 0, KEY_READ | KEY_WOW64_64KEY, &rawRoot) != ERROR_SUCCESS)
    {
        return L"";                                                                                                 // Registry key missing or unreadable.
    }
    RegKey hRoot(rawRoot);                                                                                          // RAII takes ownership.

    //-------------------------------------------------------------------------------------------------------------
    // Track the best (highest) stable version found.
    //-------------------------------------------------------------------------------------------------------------
    SemVer bestVer;                                                                                                 // Invalid by default.
    std::wstring bestPath;

    //-------------------------------------------------------------------------------------------------------------
    // Enumerate all subkeys (each representing one installed PowerShell version).
    //-------------------------------------------------------------------------------------------------------------
    wchar_t subKeyName[128];
    DWORD index = 0;

    while (true)
    {
        DWORD subKeyLen = static_cast<DWORD>(std::size(subKeyName));
        FILETIME ft{};                                                                                              // Unused but required by API.

        LONG result = RegEnumKeyExW(hRoot.get(),
                                    index++,
                                    subKeyName,
                                    &subKeyLen,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    &ft
        );

        if (result == ERROR_NO_MORE_ITEMS)
        {
            break;                                                                                                  // Enumeration complete.
        }

        if (result != ERROR_SUCCESS)
        {
            return L"";                                                                                             // Unexpected registry error.
        }

        //---------------------------------------------------------------------------------------------------------
        // Build full path to the subkey: SOFTWARE\...\InstalledVersions\<GUID>
        //---------------------------------------------------------------------------------------------------------
        std::wstring fullSubKey = std::wstring(rootKey) + L"\\" + subKeyName;

        //---------------------------------------------------------------------------------------------------------
        // InstallLocation is mandatory. If missing, skip this entry.
        //---------------------------------------------------------------------------------------------------------
        std::wstring installPath;
        if (!ReadRegistryString(HKEY_LOCAL_MACHINE, fullSubKey.c_str(), L"InstallLocation", installPath))
        {
            continue;
        }

        //---------------------------------------------------------------------------------------------------------
        // Verify pwsh.exe exists in this installation folder.
        //---------------------------------------------------------------------------------------------------------
        std::wstring exe = installPath + L"\\pwsh.exe";
        if (!_FileExists(exe))
        {
            continue;
        }

        //---------------------------------------------------------------------------------------------------------
        // Determine version:
        //   1. Prefer SemanticVersion (PowerShell 7.4+ MSI installers)
        //   2. Fallback: parse version from folder name
        //---------------------------------------------------------------------------------------------------------
        std::wstring semverStr;
        SemVer ver;

        if (ReadRegistryString(HKEY_LOCAL_MACHINE, fullSubKey.c_str(), L"SemanticVersion", semverStr))
        {
            ver = SemVer::Parse(semverStr);
        }
        else
        {
            ver = ParseVersionFromFolder(installPath);
        }

        //---------------------------------------------------------------------------------------------------------
        // Skip invalid versions.
        //---------------------------------------------------------------------------------------------------------
        if (!ver.isValid)
        {
            continue;
        }

        //---------------------------------------------------------------------------------------------------------
        // Skip prerelease versions (preview, rc, beta, alpha).
        //---------------------------------------------------------------------------------------------------------
        if (ver.isSpecialRelease)
        {
            continue;
        }

        //---------------------------------------------------------------------------------------------------------
        // Keep the highest stable version found so far.
        //---------------------------------------------------------------------------------------------------------
        if (!bestVer.isValid || ver > bestVer)
        {
            bestVer  = ver;
            bestPath = exe;
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    // If we found a valid stable pwsh.exe, return its absolute normalized path.
    //-------------------------------------------------------------------------------------------------------------
    if (!bestPath.empty())
    {
        return _NormalizeToFullPath(bestPath);
    }

    //-------------------------------------------------------------------------------------------------------------
    // Nothing found.
    //-------------------------------------------------------------------------------------------------------------
    return L"";
}



//-----------------------------------------------------------------------------------------------------------------
// FindPwshOnKnownLocations
// 
// Use    : Searches the predefined known locations for pwsh.exe. We return the first one we find.
// Input  : ---
// Returns: ---
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
static std::wstring FindPwshOnKnownLocations()
{
    const std::vector<std::wstring> roots = {
        L"C:\\Program Files\\PowerShell\\7\\pwsh.exe",
        L"C:\\Program Files (x86)\\PowerShell\\7\\pwsh.exe"
    };

    for (const auto& path : roots)
    {
        if (_FileExists(path))
        {
            return std::filesystem::absolute(path).wstring();                                                       // We found one, return it.
        }
    }
    return L"";                                                                                                     // Nothing found.
}


                
//-----------------------------------------------------------------------------------------------------------------
// GetPwshPath
// 
// Use    : Returns the full path to pwsh.exe to use.
// Input  : ---
// Returns: Full path to the pwsh.exe found.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
static std::wstring GetPwshPath()
{
    //-------------------------------------------------------------------------------------------------------------
    // Search the registry (authoritative, stable-only, highest version).
    //-------------------------------------------------------------------------------------------------------------
    if (auto p = FindPwshViaRegistry(); !p.empty())
    {
        return p;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Search on known install directories.
    //-------------------------------------------------------------------------------------------------------------
    if (auto p = FindPwshOnKnownLocations(); !p.empty())
    {
        return p;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Search the system PATH.
    //-------------------------------------------------------------------------------------------------------------
    if (auto p = _FindExecutableOnPath(L"pwsh.exe"); !p.empty())
    {
        return p;
    }

    return L"";
}



//-----------------------------------------------------------------------------------------------------------------
// GetPowerShellPath
// 
// Use    : Returns the full path to powershell.exe
// Input  : ---
// Returns: Full path to powershell.exe
// Note(s): 1) We only search on the default path.
//-----------------------------------------------------------------------------------------------------------------
static std::wstring GetPowerShellPath()
{
    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD len = GetEnvironmentVariableW(L"SystemRoot", buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
    {
        return L"";
    }
    
    std::wstring FilePath = std::wstring(buffer) + L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    if (_FileExists(FilePath))
    {
        return FilePath;
    }

    return L"";
}



//-----------------------------------------------------------------------------------------------------------------
// IsYemLauncherOption
// 
// Use    : Checks if the given option is a YemLauncher option.
// Input  : option
// Returns: True if it is a YemLauncher option, false otherwise.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
bool CommandParser::IsYemLauncherOption(std::wstring option)
{
    bool bResult = false;

    //-------------------------------------------------------------------------------------------------
    // Get lowerstring value.
    //-------------------------------------------------------------------------------------------------
    option = _ToLower(option);

    //-------------------------------------------------------------------------------------------------
    // Check if its recognized option.
    //-------------------------------------------------------------------------------------------------
    if (option == L"-log"         || option == L"-l" ||
        option == L"-application" || option == L"-a" ||
        option == L"-exclude"     || option == L"-e" ||
        option == L"-resolve"     || option == L"-r" ||
        option == L"-timeout"     || option == L"-t" ||
        option == L"-workingdir"  || option == L"-w" ||
        option == L"-command"     || option == L"-c" ||
        option == L"-pwsh"        || option == L"-p" ||
        option == L"-powershell"  || option == L"-wp")
        {
        bResult = true;
    }
    return bResult;
}



//-----------------------------------------------------------------------------------------------------------------
// ParseArguments
// 
// Use    : Parse the commandline and set the found values.
// Input  : argc   - Count of arguments.
//          argv[] - Array of arguments.
// Returns: Options found on the commandline.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
CommandLineOptions CommandParser::ParseArguments(int argc, wchar_t* argv[])
{
    CommandLineOptions options;
    options.mode            = LaunchMode::Unknown;
    options.timeoutSeconds  = 0;
    options.enableLogging   = false;
    options.logFilename     = L"";
    options.applicationPath = L"";
    options.resolvedPath    = L"";
    options.startupPath     = std::filesystem::current_path().wstring();
    options.workingPath     = std::filesystem::current_path().wstring();

    int i                = 0;
    bool ActionSpecified = false;                                                                                   // Whether we've encountered an action (-pwsh, -powershell, or -application) that determines the launch mode.

    //-------------------------------------------------------------------------------------------------------------
    // Parse parameters
    //-------------------------------------------------------------------------------------------------------------
    while (i < argc)
    {
        std::wstring arg(argv[i]);
        arg = _ToLower(argv[i]);

        if ((arg == L"-timeout") || (arg == L"-t"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            i++;
            if (i < argc)
            {
                try
                {
                    options.timeoutSeconds = std::stoul(argv[i]);
                    i++;
                    continue;
                }
                catch (...)
                {
                    options.timeoutSeconds = DEFAULT_TIMEOUT_SECONDS;
                    continue;
                }
            }
            else
            {
                options.timeoutSeconds = DEFAULT_TIMEOUT_SECONDS;
                continue;
            }
        }
		else if ((arg == L"-log") || (arg == L"-l"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            options.enableLogging = true;
            i++;

            //-----------------------------------------------------------------------------------------------------
            // Check if next argument is a filename or another option.
            //-----------------------------------------------------------------------------------------------------
            if (i < argc)
            {
                if (!(CommandParser::IsYemLauncherOption(argv[i])))
                {
                    options.logFilename = _ExpandEnvVarsW(argv[i]);
                    i++;
                    continue;
                }
            }
            continue;
        }
        else if ((arg == L"-workingdir") || (arg == L"-w"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            i++;
            if (i < argc)
            {
                //-----------------------------------------------------------------------------------------------------
                // Check if next argument is a working path or another option.
                //-----------------------------------------------------------------------------------------------------
                if (!(CommandParser::IsYemLauncherOption(argv[i])))
                {
                    options.workingPath = argv[i];
                    i++;
                    continue;
                }
                else
                {
                    options.workingPath = options.startupPath;                                                      // Fallback to the startupPath.
                }
            }
        }
        else if ((arg == L"-command") || (arg == L"-c"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            options.PowerShellCommand = true;
            i++;
            continue;
        }
        else if ((arg == L"-resolve") || (arg == L"-r"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            options.useResolvedPath = true;
            i++;
            continue;
        }
        else if ((arg == L"-exclude") || (arg == L"-e"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            options.excludeApplicationName = true;
            i++;
            continue;
        }
        else if ((arg == L"-pwsh" || arg == L"-p"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            i++;
            ActionSpecified         = true;
            options.mode            = LaunchMode::PowerShellCore;
            options.applicationPath = GetPwshPath();
			options.arguments.push_back(L"-NoLogo");
            options.arguments.push_back(L"-NoProfile");
            options.arguments.push_back(L"-NonInteractive");
            options.arguments.push_back(L"-WindowStyle");
            options.arguments.push_back(L"Hidden");
            options.arguments.push_back(L"-Executionpolicy");
            options.arguments.push_back(L"bypass");
            continue;
        }
        else if ((arg == L"-powershell" || arg == L"-wp"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            i++;
            ActionSpecified         = true;
            options.mode            = LaunchMode::PowerShell;
            options.applicationPath = GetPowerShellPath();
            options.arguments.push_back(L"-NoLogo");
            options.arguments.push_back(L"-NoProfile");
            options.arguments.push_back(L"-NonInteractive");
            options.arguments.push_back(L"-WindowStyle");
            options.arguments.push_back(L"Hidden");
            options.arguments.push_back(L"-Executionpolicy");
            options.arguments.push_back(L"bypass");
            continue;
        }
        else if ((arg == L"-application") || (arg == L"-a"))
        {
            if (ActionSpecified)                                                                                    // Action is already specified, so we have an invalid commandline.
            {
                return options;
            }
            i++;
            ActionSpecified = true;
            options.mode    = LaunchMode::Application;
            continue;
        }
        else
        {
			//-----------------------------------------------------------------------------------------------------
			// Not a recognized option, so we should be passed all options for the launcher.
            //-----------------------------------------------------------------------------------------------------
            if ((options.mode == LaunchMode::PowerShell) || (options.mode == LaunchMode::PowerShellCore))
            {
                //-------------------------------------------------------------------------------------------------
                // Set what to do for the powershell command.
                //-------------------------------------------------------------------------------------------------
                if (options.PowerShellCommand)
                {
                    options.arguments.push_back(L"-Command");
                }
                else
                {
                    options.arguments.push_back(L"-File");
                }
            }
            else if (options.mode == LaunchMode::Application)
            {
                //-------------------------------------------------------------------------------------------------
                // We have an application to launch, get the application name which must be next.
                //-------------------------------------------------------------------------------------------------
                options.applicationPath = _ExpandEnvVarsW(argv[i]);
                i++;
            }
            else
            {
                //-------------------------------------------------------------------------------------------------
                // We do not have an acttion defined, so an invalid commandline.
                //-------------------------------------------------------------------------------------------------
                return options;
            }
            //-----------------------------------------------------------------------------------------------------
            // Retrieve the rest of the arguments. These are arguments to pass to the application (or powershell).
            //-----------------------------------------------------------------------------------------------------
            while (i < argc)
            {
                options.arguments.push_back(_ExpandEnvVarsW(argv[i]));
                i++;
            }
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    // Resolve the path if requested.
    //-------------------------------------------------------------------------------------------------------------
    if (options.useResolvedPath)
    {
        options.resolvedPath = _FindExecutableOnPath(options.applicationPath);
    }
    else
    {
        options.resolvedPath = options.applicationPath;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Check if we have valid options.
    //-------------------------------------------------------------------------------------------------------------
    bool validOptions = (options.mode != LaunchMode::Unknown && !options.applicationPath.empty() && !options.resolvedPath.empty());
    if (validOptions)
    {
        //---------------------------------------------------------------------------------------------------------
        // The resolved application must exist, otherwise we have nothing to start.
        // With PowerShell we do not allow exclude or resolve.
        //---------------------------------------------------------------------------------------------------------
        if (_FileExists(options.resolvedPath))
        {
            if (options.mode == LaunchMode::PowerShell || options.mode == LaunchMode::PowerShellCore)
            {
                if (options.useResolvedPath)
                {
                    validOptions = false;
                }
                if (options.excludeApplicationName)
                {
                    validOptions = false;
                }
            }
        }
        else
        {
            validOptions = false;
        }
    }
    options.validOptions = validOptions;
    return options;
}
