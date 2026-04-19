#include "ProcessManager.h"



//-----------------------------------------------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------------------------------------------
ProcessManager::ProcessManager(Logger& logger)
    : m_logger(logger), m_processHandle(nullptr), m_jobHandle(nullptr)
{
}



//-----------------------------------------------------------------------------------------------------------------
// StartProcess
// 
// Use    : Starts the process.
// Input  : applicationPath        - Path to the application to start.
//          arguments              - Arguments to pass to the application to start.
//          workingPath            - Working path for the new process.
//          timeoutSeconds         - If > 0, the process will be monitored. If it exceeds this value, it will be 
//                                   killed.
//          excludeApplicationName - If true, we exclude the application name from argv[0]
// Returns: True on success, false otherwise.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
bool ProcessManager::StartProcess(const std::wstring& applicationPath,
    const std::vector<std::wstring>& arguments,
    const std::wstring& workingPath,
    const unsigned int timeoutSeconds,
    const bool excludeApplicationName)
{
    bool bResult = false;

    //-------------------------------------------------------------------------------------------------------------
    // Rebuild the cmdline to hold the application (as argv[0]), unless excluded.
    //-------------------------------------------------------------------------------------------------------------
    std::wstring cmdLine;
    if (!excludeApplicationName)
    {
        cmdLine = L"\"" + applicationPath + L"\"";
    }
    for (const auto& arg : arguments)
    {
        cmdLine += L" ";
        cmdLine += _QuoteArgWindows(arg);
    }
    m_logger.LogInfo(L"Starting process: " + cmdLine);

    //-------------------------------------------------------------------------------------------------------------
    // Validate and get the pointer to the working path.
    //-------------------------------------------------------------------------------------------------------------
    const wchar_t* lpWorkingPath = nullptr;
    if (!workingPath.empty())
    {
        DWORD attrs = GetFileAttributesW(workingPath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES)
        {
            m_logger.LogWarning(L"Working directory does not exist: " + workingPath + L". Proceeding without setting working directory.");
        }
        else if (!(attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            m_logger.LogWarning(L"Working path is not a directory: " + workingPath + L". Proceeding without setting working directory.");
        }
        else
        {
            lpWorkingPath = workingPath.c_str();
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    // Create the process hidden.
    //-------------------------------------------------------------------------------------------------------------
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    //-------------------------------------------------------------------------------------------------------------
    // Set creation flags: CREATE_NO_WINDOW to hide the console window, CREATE_UNICODE_ENVIRONMENT for Unicode 
    // environment block.
    //-------------------------------------------------------------------------------------------------------------
    DWORD flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;

    //-------------------------------------------------------------------------------------------------------------
    // Initialize process information structure.
    //-------------------------------------------------------------------------------------------------------------
    PROCESS_INFORMATION pi = {};

    //-------------------------------------------------------------------------------------------------------------
    // Create a mutable buffer for CreateProcessW, so we are sure it is terminated correctly.
    //-------------------------------------------------------------------------------------------------------------
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    //-------------------------------------------------------------------------------------------------------------
    // Create the process (applicationPath as lpApplicationName, cmdLine as mutable buffer)
    //-------------------------------------------------------------------------------------------------------------
    BOOL bSuccess = CreateProcessW(
        applicationPath.c_str(),
        cmdBuf.data(),
        nullptr,
        nullptr,
        FALSE,
        flags,
        nullptr,
        lpWorkingPath,
        &si,
        &pi
    );

    //-------------------------------------------------------------------------------------------------------------
    // Check if process creation succeeded and log the result. If it failed, we won't have a valid process handle 
    // to monitor or terminate.
    //-------------------------------------------------------------------------------------------------------------
    if (bSuccess)
    {
        //---------------------------------------------------------------------------------------------------------
        // Store process handle and close thread handle.
        //---------------------------------------------------------------------------------------------------------
        m_processHandle.Reset(pi.hProcess);
        CloseHandle(pi.hThread);
        m_logger.LogInfo(L"Process started successfully with PID: " + std::to_wstring(pi.dwProcessId));

        //---------------------------------------------------------------------------------------------------------
        // If timeout is specified, create a job object to manage the process and monitor it.
        //---------------------------------------------------------------------------------------------------------
        if (timeoutSeconds > 0)
        {
            //-----------------------------------------------------------------------------------------------------
            // Create a job object and set kill-on-job-close so children are terminated on timeout.
            //-----------------------------------------------------------------------------------------------------
            m_jobHandle.Reset(CreateJobObjectW(nullptr, nullptr));
            if (!m_jobHandle.IsValid())
            {
                DWORD error = GetLastError();
                m_logger.LogWarning(L"Failed to create job object. Child processes may survive. Error: " + std::to_wstring(error));
            }
            else
            {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
                jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

                if (!SetInformationJobObject(m_jobHandle.Get(), JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
                {
                    DWORD error = GetLastError();
                    m_jobHandle.Close();
                    m_logger.LogWarning(L"Failed to set job object information. Child processes may survive. Error: " + std::to_wstring(error));
                }
                else
                {
                    if (!AssignProcessToJobObject(m_jobHandle.Get(), m_processHandle.Get()))
                    {
                        DWORD error = GetLastError();
                        m_jobHandle.Close();
                        m_logger.LogWarning(L"Failed to assign process to job object. Child processes may survive. Error: " + std::to_wstring(error));
                    }
                    else
                    {
                        m_logger.LogInfo(L"Process assigned to job object with KILL_ON_JOB_CLOSE.");
                    }
                }
            }

            bResult = MonitorProcessTimeout(timeoutSeconds);
        }
        else
        {
            //-----------------------------------------------------------------------------------------------------
            // No timeout: process continues running independently after started by the launcher, exit immediately.
            //-----------------------------------------------------------------------------------------------------
            bResult = true;
            m_logger.LogInfo(L"No timeout given. Launcher returns immediately.");
        }
    }
    else
    {
        DWORD error = GetLastError();
        m_logger.LogError(L"Failed to create the new process. Error code: " + std::to_wstring(error));
    }
    return bResult;
}



//-----------------------------------------------------------------------------------------------------------------
// MonitorProcessTimeout
// 
// Use    : Monitors the process and kills it if the timeout has passed.
// Input  : timeoutSeconds - The timeout in seconds before terminating the process.
// Returns: True when the process is not terminated, false otherwise.
// Note(s): 1) The exit code of the process is logged in the log file.
//-----------------------------------------------------------------------------------------------------------------
bool ProcessManager::MonitorProcessTimeout(unsigned int timeoutSeconds)
{
    bool bResult = false;
    m_logger.LogInfo(L"Monitoring the process till the timeout threshold is reached.");

    //-------------------------------------------------------------------------------------------------------------
    // Prevent a timeout to high.
    //-------------------------------------------------------------------------------------------------------------
    const unsigned int MAX_TIMEOUT_SECONDS = (INFINITE / 1000) - 1;
    unsigned int clamped                   = (timeoutSeconds > MAX_TIMEOUT_SECONDS) ? MAX_TIMEOUT_SECONDS : timeoutSeconds;
    DWORD timeoutMs                        = clamped * 1000;

    //-------------------------------------------------------------------------------------------------------------
    // Monitor the process.
    //-------------------------------------------------------------------------------------------------------------
    DWORD waitResult = WaitForSingleObject(m_processHandle.Get(), timeoutMs);

    //-------------------------------------------------------------------------------------------------------------
    // WaitForSingleObject returned.
    //-------------------------------------------------------------------------------------------------------------
    if (waitResult == WAIT_TIMEOUT)
    {
        m_logger.LogInfo(L"Timeout expired. Terminating process and its children.");

        //---------------------------------------------------------------------------------------------------------
        // Preferred: close the job handle to trigger JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        //---------------------------------------------------------------------------------------------------------
        if (m_jobHandle.IsValid())
        {
            //-----------------------------------------------------------------------------------------------------
            // Closing the job handle will terminate all processes in the job if the limit was set.
            //-----------------------------------------------------------------------------------------------------
            m_jobHandle.Close();
            m_logger.LogInfo(L"Closed job handle to terminate all processes in the job.");
            //-----------------------------------------------------------------------------------------------------
            // Wait for process to exit after job closure (job termination is asynchronous)
            //-----------------------------------------------------------------------------------------------------
            DWORD waitForJobResult = WaitForSingleObject(m_processHandle.Get(), 5000);                              // 5 seconds max
            if (waitForJobResult == WAIT_OBJECT_0)
            {
                m_logger.LogInfo(L"Process exited after job closure.");
            }
            else if (waitForJobResult == WAIT_TIMEOUT)
            {
                m_logger.LogWarning(L"Process did not exit within grace period after job closure.");
            }
        }
        else
        {
            //-----------------------------------------------------------------------------------------------------
            // Fallback: try to terminate the main process only
            //-----------------------------------------------------------------------------------------------------
            if (TerminateProcess(m_processHandle.Get(), 1))
            {
                m_logger.LogInfo(L"Process terminated successfully");
                //-------------------------------------------------------------------------------------------------
                // Wait briefly for process to exit after termination (TerminateProcess is asynchronous)
                //-------------------------------------------------------------------------------------------------
                DWORD waitForTermResult = WaitForSingleObject(m_processHandle.Get(), 5000);                         // 5 seconds max
                if (waitForTermResult == WAIT_OBJECT_0)
                {
                    m_logger.LogInfo(L"Process exited after termination.");
                }
                else if (waitForTermResult == WAIT_TIMEOUT)
                {
                    m_logger.LogWarning(L"Process did not exit within grace period after termination.");
                }
            }
            else
            {
                DWORD error = GetLastError();
                m_logger.LogError(L"Failed to terminate process. Error code: " + std::to_wstring(error));
            }
        }
    }
    else if (waitResult == WAIT_OBJECT_0)
    {
        DWORD exitCode;
        if (GetExitCodeProcess(m_processHandle.Get(), &exitCode))
        {
            m_logger.LogInfo(L"Process exited with code: " + std::to_wstring(exitCode));
            bResult = true;                                                                                         // Process exited successfully before timeout.
        }
        else
        {
            DWORD error = GetLastError();
            m_logger.LogError(L"Failed to get process exit code. Error code: " + std::to_wstring(error));
        }
    }
    else
    {
        DWORD error = GetLastError();
        m_logger.LogError(L"Error waiting for process. Error code: " + std::to_wstring(error));
    }
    return bResult;
}
