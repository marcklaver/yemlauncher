#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>


//-----------------------------------------------------------------------------------------------------------------
// Default name for the log file.
//-----------------------------------------------------------------------------------------------------------------
inline const std::wstring defaultLogFileName = L"YemLauncher.log";



class Logger
{
public:
    Logger();
    ~Logger() noexcept;

    //-------------------------------------------------------------------------------------------------------------
    // Initialize logger with optional filename
    //-------------------------------------------------------------------------------------------------------------
    bool Initialize(const std::wstring& filename = L"");

    //-------------------------------------------------------------------------------------------------------------
    // Log a message
    //-------------------------------------------------------------------------------------------------------------
    void Log(const std::wstring& message) noexcept;
    void LogError(const std::wstring& message) noexcept;
    void LogWarning(const std::wstring& message) noexcept;
    void LogInfo(const std::wstring& message) noexcept;

    //-------------------------------------------------------------------------------------------------------------
    // Disable logging.
    //-------------------------------------------------------------------------------------------------------------
    void DisableLogging();

private:
    //-------------------------------------------------------------------------------------------------------------
    // Return current time formatted as HH:MM:SS.mmm for log entries.
    //-------------------------------------------------------------------------------------------------------------
    std::wstring GetFormattedTime() const;

    //-------------------------------------------------------------------------------------------------------------
    // Return the log file path.
    //-------------------------------------------------------------------------------------------------------------
    std::wstring GetLogFilePath(const std::wstring& filename);

    //-------------------------------------------------------------------------------------------------------------
    // Return a log file path with the current date appended before the extension.
    //-------------------------------------------------------------------------------------------------------------
    std::wstring MakeDatedLogPath(const std::wstring& fullpath);

    std::wfstream m_logFile;                                                                                        // Name of the log file.
    std::wstring m_logFilePath;                                                                                     // Log file path.
    bool m_enabled = false;                                                                                         // Logging enabled (true) or disabled (false).
    std::recursive_mutex m_logMutex;                                                                                // Recursive mutex for thread-safe logging (allows nested locks).
};
