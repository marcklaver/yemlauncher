#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <ctime>
#include "version.h"
#include "Logger.h"
#include "YemHelpers.h"



//-----------------------------------------------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------------------------------------------
Logger::Logger() : m_enabled(false)
{
}



//-----------------------------------------------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------------------------------------------
Logger::~Logger() noexcept
{
    if (m_logFile.is_open())
    {
        m_logFile.close();
    }
}



//-----------------------------------------------------------------------------------------------------------------
// DisableLogging
// 
// Use    : Disables logging.
// Input  : ---
// Returns: ---
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
void Logger::DisableLogging()
{
    std::lock_guard<std::recursive_mutex> lock(m_logMutex);
    m_enabled = false;
    if (m_logFile.is_open())
    {
        m_logFile.close();
    }
}



//-----------------------------------------------------------------------------------------------------------------
// Initialize
// 
// Use    : Initialize and enables logging.
// Input  : filename - Name of the log file.
// Returns: True on success and enabled, false otherwise.
// Note(s): 1) If filename is empty, we use the default logfile name.
//-----------------------------------------------------------------------------------------------------------------
bool Logger::Initialize(const std::wstring& filename)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);
	std::wstring nameToUse = filename;

	if (filename.empty())
	{
		nameToUse = defaultLogFileName;                                                                             // use default log name.
	}

	m_logFilePath = GetLogFilePath(nameToUse);

	try
	{
		m_logFile.open(m_logFilePath, std::ios::app);
		if (m_logFile.is_open())
		{
			m_enabled = true;
			Log(L"********** YemLauncher (v" + std::wstring(VER_FILEVERSION_WSTR) + L") started * *********");
		}
	}
	catch (const std::exception&)
	{
		DisableLogging();
	}

	return m_enabled;
}



//-----------------------------------------------------------------------------------------------------------------
// MakeDatedLogPath
// 
// Use    : Adds a date string to the given file.
// Input  : fullpath - Full path the the file to rewrite.
// Returns: Full path to the file, extended with a date string.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring Logger::MakeDatedLogPath(const std::wstring& fullpath)
{
    namespace fs = std::filesystem;
    fs::path p(fullpath);

    //-------------------------------------------------------------------------------------------------------------
    // Extract filename without extension
    //-------------------------------------------------------------------------------------------------------------
    std::wstring stem = p.stem().wstring();

    //-------------------------------------------------------------------------------------------------------------
    // Build date string YYYYMMDD
    //-------------------------------------------------------------------------------------------------------------
    auto now      = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);

    std::wstringstream date;
    date << std::put_time(&tm, L"%Y%m%d");

    //-------------------------------------------------------------------------------------------------------------
    // Construct new filename
    //-------------------------------------------------------------------------------------------------------------
    std::wstring newFile = stem + L"_" + date.str() + L".log";

    //-------------------------------------------------------------------------------------------------------------
    // Reattach original directory
    //-------------------------------------------------------------------------------------------------------------
    fs::path newFullPath = p.parent_path() / newFile;

    return newFullPath.wstring();
}



//-----------------------------------------------------------------------------------------------------------------
// GetLogFilePath
// 
// Use    : Create a full path to the log file. Generating a log file per day.
// Input  : filename - Name of the log file used to build the full path.
// Returns: wstring - Path to the log file.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring Logger::GetLogFilePath(const std::wstring& filename)
{
    if (!filename.empty() && filename.find(L"\\") != std::wstring::npos)
    {
        if(!(_NormalizeToFullPath(filename)).empty())
        {
            //-----------------------------------------------------------------------------------------------------
            // Valid full/partial path provided, use it.
            //-----------------------------------------------------------------------------------------------------
            return MakeDatedLogPath(filename);
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    // Try to read system TEMP from registry; fallback to filename if unavailable.
    //-------------------------------------------------------------------------------------------------------------
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
                      0,
                      KEY_READ,
                      &hKey) != ERROR_SUCCESS)
    {
        return filename;
    }

	//-------------------------------------------------------------------------------------------------------------
	// We have the key. Process it.
	//-------------------------------------------------------------------------------------------------------------
	wchar_t buffer[MAX_PATH];
	DWORD bufferSize = sizeof(buffer);
	DWORD type = 0;

	if (RegQueryValueExW(hKey,
						 L"TEMP",
						 nullptr,
						 &type,
						 reinterpret_cast<LPBYTE>(buffer),
						 &bufferSize) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return _NormalizeToFullPath(filename);                                                                      // No value or error, fallback to filename only
	}

    //-------------------------------------------------------------------------------------------------------------
    // Close the registry key as we no longer need it
    //-------------------------------------------------------------------------------------------------------------
    RegCloseKey(hKey);

	//-------------------------------------------------------------------------------------------------------------
	// Validate that the registry value did fit in our buffer and is non-empty
	//-------------------------------------------------------------------------------------------------------------
	if (bufferSize > sizeof(buffer) || bufferSize == 0)
	{
        return _NormalizeToFullPath(filename);                                                                      // No value or error, fallback to filename only
    }

    //-------------------------------------------------------------------------------------------------------------
    // Expand %SystemRoot% etc.
    //-------------------------------------------------------------------------------------------------------------
    wchar_t expanded[MAX_PATH];
    if (!ExpandEnvironmentStringsW(buffer, expanded, MAX_PATH))
    {
        return _NormalizeToFullPath(filename);                                                                      // No value or error, fallback to filename only
    }

	std::wstring logPath = std::wstring(expanded) + L"\\" + filename;
    return _NormalizeToFullPath(MakeDatedLogPath(logPath));
}



//-----------------------------------------------------------------------------------------------------------------
// GetFormattedTime
// 
// Use    : Returns the current date and time formatted for the log file.
// Input  : ---
// Returns: The formatted date/time string.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
std::wstring Logger::GetFormattedTime() const
{
    SYSTEMTIME st;
    GetLocalTime(&st);

    std::wostringstream oss;
    oss << std::setfill(L'0')
        << std::setw(2) << st.wHour << L":"
        << std::setw(2) << st.wMinute << L":"
        << std::setw(2) << st.wSecond << L"."
        << std::setw(3) << st.wMilliseconds;

    return oss.str();
}



//-----------------------------------------------------------------------------------------------------------------
// Log
// 
// Use    : Log a message to the log file.
// Input  : message - Message to log.
// Returns: ---
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
void Logger::Log(const std::wstring& message) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_logMutex);
    if (!m_enabled || !m_logFile.is_open())
    {
        return;
    }

    std::wstring timeStr = L"[" + GetFormattedTime() + L"]";
    std::wstring logEntry = timeStr + L" " + message + L"\n";

    m_logFile << logEntry;
    m_logFile.flush();
}



//-----------------------------------------------------------------------------------------------------------------
// LogError
// 
// Use    : Logs an error message to the log file.*
// Input  : message - Message to log.
// Returns: ---
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
void Logger::LogError(const std::wstring& message) noexcept
{
    std::wstring errorMsg = L"ERROR    : " + message;
    Log(errorMsg);
}



//-----------------------------------------------------------------------------------------------------------------
// LogWarning
// 
// Use    : Logs a warning  message to the log file.
// Input  : message - Message to log.
// Returns: ---
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
void Logger::LogWarning(const std::wstring& message) noexcept
{
    std::wstring warningMsg = L"WARNING  : " + message;
    Log(warningMsg);
}



//-----------------------------------------------------------------------------------------------------------------
// LogInfo
// 
// Use    : Logs a normal message to the log file.
// Input  : message - Message to log.
// Returns: ---
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
void Logger::LogInfo(const std::wstring& message) noexcept
{
    std::wstring infoMsg = L"         : " + message;
    Log(infoMsg);
}
