#include <algorithm>
#include <filesystem>
#include <windows.h>
#include <string>
#include <cwctype>
#include <vector>
#include "YemHelpers.h"



//-----------------------------------------------------------------------------------------------------------------
// _NormalizeToFullPath
// 
// Use    : Normalize the given file path.
// Input  : path - Path to normalize
// Returns: ---
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring _NormalizeToFullPath(const std::wstring& path)
{
    wchar_t buffer[MAX_PATH];
    DWORD len = GetFullPathNameW(path.c_str(), MAX_PATH, buffer, nullptr);

    if (len == 0 || len >= MAX_PATH)
    {
        return {};                                                                                                  // Failed or path too long
    }

    return std::wstring(buffer);
}



//-----------------------------------------------------------------------------------------------------------------
// _QuoteArgWindows
// 
// Use    : Quote the given arguments following the rules for Windows.
// Input  : arg - Argument.
// Returns: Quoted argument.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring _QuoteArgWindows(const std::wstring& arg)
{
    //-------------------------------------------------------------------------------------------------------------
    // If no quoting needed, return as-is
    //-------------------------------------------------------------------------------------------------------------
    if (arg.find_first_of(L" \t\"") == std::wstring::npos)
        return arg;

    std::wstring result = L"\"";
    size_t len = arg.length();

    for (size_t i = 0; i < len; ++i)
    {
        unsigned backslashes = 0;

        //---------------------------------------------------------------------------------------------------------
        // Count consecutive backslashes
        //---------------------------------------------------------------------------------------------------------
        while (i < len && arg[i] == L'\\')
        {
            ++backslashes;
            ++i;
        }

        if (i == len)
        {
            //-----------------------------------------------------------------------------------------------------
            // Escape all backslashes at end of string
            //-----------------------------------------------------------------------------------------------------
            result.append(backslashes * 2, L'\\');
            break;
        }
        else if (arg[i] == L'"')
        {
            //-----------------------------------------------------------------------------------------------------
            // Escape backslashes and the quote
            //-----------------------------------------------------------------------------------------------------
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
        }
        else
        {
            //-----------------------------------------------------------------------------------------------------
            // Normal characters: keep backslashes as-is
            //-----------------------------------------------------------------------------------------------------
            result.append(backslashes, L'\\');
            result.push_back(arg[i]);
        }
    }

    result.push_back(L'"');
    return result;
}



//-----------------------------------------------------------------------------------------------------------------
// _FileExists
// 
// Use    : Checks if the file exists and is not a directory.
// Input  : path - Path to check.
// Returns: True if the file exists, false otherwise.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
bool _FileExists(const std::wstring& path)
{
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}



//-----------------------------------------------------------------------------------------------------------------
// _ExpandEnvVarsW
// 
// Use    : Expands the windows environment variables in a string.
// Input  : input - string to expand.
// Returns: string - Expanded string.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring _ExpandEnvVarsW(const std::wstring& input)
{
    //-------------------------------------------------------------------------------------------------------------
    // Get required buffer size (including null terminator)
    //-------------------------------------------------------------------------------------------------------------
    DWORD size = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (size == 0)
    {
        return input;                                                                                               // Return original input on failure.
    }

    //-------------------------------------------------------------------------------------------------------------
    // Validate that size doesn't exceed reasonable limits
    //-------------------------------------------------------------------------------------------------------------
    if (size > 32768)  // 32KB limit to prevent excessive allocations
    {
        return input;
    }

    std::vector<wchar_t> buffer(size);

    //-------------------------------------------------------------------------------------------------------------
    // Expand environment variables into the buffer
    //-------------------------------------------------------------------------------------------------------------
    DWORD written = ExpandEnvironmentStringsW(input.c_str(), buffer.data(), size);

    //-------------------------------------------------------------------------------------------------------------
    // Validate return value: written should be > 0 and < size (includes null terminator)
    //-------------------------------------------------------------------------------------------------------------
    if (written == 0 || written > size)
    {
        //---------------------------------------------------------------------------------------------------------
        // Expansion failed or buffer too small → return original input.
        //---------------------------------------------------------------------------------------------------------
        return input;
    }

    return std::wstring(buffer.data());
}



//-----------------------------------------------------------------------------------------------------------------
// _ToLower
// 
// Use    : Converts the given string to lowercase.
// Input  : input - String to convert.
// Returns: Input string in lowercase.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring _ToLower(const std::wstring& input)
{
    std::wstring result;
    result.resize(input.size());
    std::transform(input.begin(),
                   input.end(),
                   result.begin(),
                   [](wchar_t c) 
                   {
                       return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
                   }
    );
    return result;
}



//-----------------------------------------------------------------------------------------------------------------
// _FindExecutableOnPath
// 
// Use    : Searches for the application on the system PATH.
// Input  : applicationPath - Path or executable to search for.
// Returns: Full path to the executable or an empty string if not found.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring _FindExecutableOnPath(const std::wstring& applicationPath)
{
    if (applicationPath.empty())
    {
        return {};                                                                                                  // Nothing to search for.
    }

    //-------------------------------------------------------------------------------------------------------------
    // If the file exists, normalize and check again.
    //-------------------------------------------------------------------------------------------------------------
    if(_FileExists(applicationPath))
    {
        std::wstring fullPath;
        fullPath = _NormalizeToFullPath(applicationPath);                                                           
        if (_FileExists(fullPath))
        {
            return fullPath;                                                                                        
        }
        return {};                                                                                                  // Cannot be found after normalization. Path too long?
    }

    //-------------------------------------------------------------------------------------------------------------
    // SearchPathW requires a writable buffer. First call determines required size.
    //-------------------------------------------------------------------------------------------------------------
    DWORD required = SearchPathW(nullptr,                                                                           // search PATH
                                 applicationPath.c_str(),                                                           // file name
                                 nullptr,                                                                           // no extension override
                                 0,                                                                                 // request buffer size
                                 nullptr,                                                                           // no buffer yet
                                 nullptr);                                                                          // no pointer to file part
    

    //-------------------------------------------------------------------------------------------------------------
    // Not found on PATH
    //-------------------------------------------------------------------------------------------------------------
    if (required == 0)
    {
        return {};                                                                                                  
    }

    //-------------------------------------------------------------------------------------------------------------
    // Get the result.
    //-------------------------------------------------------------------------------------------------------------
    std::wstring buffer;
    buffer.resize(required);
    DWORD written = SearchPathW(nullptr,
                                applicationPath.c_str(),
                                nullptr,
                                required,
                                buffer.data(),
                                nullptr);

    if (written == 0 || written >= required)
    {
        return {};                                                                                                  // Should not happen, but safe fallback
    }

    //-------------------------------------------------------------------------------------------------------------
    // Remove trailing null terminator and return normalized.
    //-------------------------------------------------------------------------------------------------------------
    buffer.resize(written);
    return (_NormalizeToFullPath(buffer));
}



//-----------------------------------------------------------------------------------------------------------------
// _GetFileNameLower
// 
// Use    : Returns the file name from a path in lowercase.
// Input  : path - Path to extract the filename from and transform to lowercase.
// Returns: Lowercase filename.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring _GetFileNameLower(const std::wstring& path)
{
    std::wstring pathLower = std::filesystem::path(path).filename().wstring();
    return _ToLower(pathLower);
}
