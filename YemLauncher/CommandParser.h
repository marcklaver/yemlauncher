#pragma once
#include <string>
#include <vector>



const unsigned int DEFAULT_TIMEOUT_SECONDS = 600;                                                                   // Default timeout of 600 seconds if -timeout value is not specified.



enum class LaunchMode
{
    Unknown           = 0,
    PowerShellCore    = 1,                                                                                          // -pwsh action
    PowerShell        = 2,                                                                                          // -powershell action 
    Application       = 3                                                                                           // Direct application launch
};



struct CommandLineOptions
{
    LaunchMode                mode                   = LaunchMode::Unknown;
    unsigned int              timeoutSeconds         = 0;                                                           // Timeout in seconds, 0 means no timeout but direct return of the launcher.
    bool                      enableLogging          = false;                                                       // If true, logging is enabled.
	bool                      validOptions           = false;                                                       // If true, the parsed options are valid.
    bool                      PowerShellCommand      = false;                                                       // -command is found on the command line if true. Else it must be a file.
    bool                      useResolvedPath        = false;                                                       // -resolve is found on the command line if true, which means we should attempt to resolve the application path before launching.
    bool                      excludeApplicationName = false;                                                       // -exclude is found on the command line if true, which means the application name will not be included as argv[0] in the command line arguments.
    std::wstring              logFilename;                                                                          // The filename to use for logging if logging is enabled.
    std::wstring              applicationPath;                                                                      // The application path as provided on the command line (which may be resolved later if -resolve is used).
    std::wstring              resolvedPath;                                                                         // The resolved application path after processing (which may be the same as applicationPath if -resolve is not used).
    std::wstring              startupPath;                                                                          // The startup path (current directory at launch).
    std::wstring              workingPath;                                                                          // The working directory to launch the application with, which is the same as startupPath if not specified.
    std::vector<std::wstring> arguments;                                                                            // The list of arguments to pass to the application.
};



class CommandParser
{
public:
    //-------------------------------------------------------------------------------------------------------------
    // Parses command line arguments and returns a CommandLineOptions struct with the parsed values.
    //-------------------------------------------------------------------------------------------------------------
    static CommandLineOptions ParseArguments(int argc, wchar_t* argv[]);

private:
    //-------------------------------------------------------------------------------------------------------------
    // Constructor.
    //-------------------------------------------------------------------------------------------------------------
    CommandParser() = default;

    //-------------------------------------------------------------------------------------------------------------
    // Checks if the given option is a valid YemLauncher option or action.
    //-------------------------------------------------------------------------------------------------------------
    static bool IsYemLauncherOption(std::wstring option);
};
