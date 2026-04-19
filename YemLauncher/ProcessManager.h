#pragma once

#include <string>
#include <vector>
#include "Logger.h"
#include "YemHelpers.h"                                                                                             // For HandleGuard

//-----------------------------------------------------------------------------------------------------------------
// Manages the lifecycle of a launched process, including creation, monitoring, and cleanup.
//-----------------------------------------------------------------------------------------------------------------
class ProcessManager
{
public:
    ProcessManager(Logger& logger);                                                                                 // Constructor takes a reference to a Logger for logging events and errors.
    ~ProcessManager() = default;                                                                                    // Destructor relies on HandleGuard to clean up handles.

    //-------------------------------------------------------------------------------------------------------------
    // Start the process with the given application path, arguments, working directory, timeout and exclusion flag.
    // If timeoutSeconds > 0, the process will be monitored and terminated if it exceeds the timeout.
    // If excludeApplicationName is true, the applicationPath will not be included as argv[0] in the command line.
    // Returns true if the process was started successfully (and monitored if timeout was set), false otherwise.
    // Logs detailed information about each step and any errors encountered in the log file (if enabled).
    //-------------------------------------------------------------------------------------------------------------
    bool StartProcess(const std::wstring& applicationPath,                                                          
                      const std::vector<std::wstring>& arguments,
                      const std::wstring& workingPath,
                      const unsigned int timeoutSeconds,
                      const bool excludeApplicationName);

private:
    Logger& m_logger;
    HandleGuard m_processHandle;
    HandleGuard m_jobHandle;

    //-------------------------------------------------------------------------------------------------------------
    // Monitor the process and kills it if the timeout has passed. Returns true if the process exited before the 
    // timeout, false if it was terminated due to timeout or if an error occurred. Logs detailed information about
    // the monitoring and termination process (if enabled).
    //-------------------------------------------------------------------------------------------------------------
    bool MonitorProcessTimeout(unsigned int timeoutSeconds);
};
