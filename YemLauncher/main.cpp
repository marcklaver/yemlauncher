#include <windows.h>
#include "Logger.h"
#include "ProcessManager.h"
#include "CommandParser.h"
#include "ErrorCodes.h"
#include "YemHelpers.h"



//-----------------------------------------------------------------------------------------------------------------
// _LaunchModeToString
// 
// Use    : Translates a LaunchMode value to a string.
// Input  : mode - LaunchMode mode value.
// Returns: String with translated value.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
static std::wstring _LaunchModeToString(LaunchMode mode)
{
	switch (mode)
	{
		case LaunchMode::Unknown:        return L"Unknown";
		case LaunchMode::PowerShellCore: return L"PowerShellCore";
		case LaunchMode::PowerShell:     return L"PowerShell";
		case LaunchMode::Application:    return L"Application";
		default:                         return L"(Invalid)";
	}
}



//-----------------------------------------------------------------------------------------------------------------
// _IsYemLauncherLaunch
// 
// Use    : Checks if the given path is the YemLauncher itself.
// Input  : resolvedPath - Full path to check.
// Returns: True if it is the Launcher itself (or has the same file name), false otherwise.
// Note(s): ---
//-----------------------------------------------------------------------------------------------------------------
static bool _IsYemLauncherLaunch(const std::wstring& resolvedPath)
{
	bool bResult = false;

	//-------------------------------------------------------------------------------------------------------------
	// Get lowercase of the resolvedPath
	//-------------------------------------------------------------------------------------------------------------
	std::wstring resolvedPathLower = _ToLower(resolvedPath);

	//-------------------------------------------------------------------------------------------------------------
	// Get current executable path in lowercase.
	//-------------------------------------------------------------------------------------------------------------
	wchar_t currentExePath[MAX_PATH] = { 0 };
	DWORD pathLen = GetModuleFileNameW(nullptr, currentExePath, MAX_PATH);

	//-------------------------------------------------------------------------------------------------------------
	// Validate result
	//-------------------------------------------------------------------------------------------------------------
	if (pathLen == 0 || pathLen >= MAX_PATH)
	{
		return false;                                                                                               // Can't verify safely
	}
	std::wstring currentExePathStr(currentExePath);                                                                 // Now safe
	currentExePathStr = _ToLower(currentExePathStr);

	//-------------------------------------------------------------------------------------------------------------
	// Get for both paths the filename only.
	//-------------------------------------------------------------------------------------------------------------
	std::wstring currentExeFileName = _GetFileNameLower(currentExePathStr);
	std::wstring appFileName = _GetFileNameLower(resolvedPathLower);

	//-------------------------------------------------------------------------------------------------------------
	// Check for YemLauncher launch by fulld path and name.
	//-------------------------------------------------------------------------------------------------------------
	if (currentExePathStr == resolvedPathLower)                                                                     // Prevent launching the same executable
	{
		bResult = true;
	}
	else if (currentExeFileName == appFileName)                                                                     // Prevent launching any executable with the same filename as the launcher
	{
		bResult = true;
	}
	return bResult;
}



//-----------------------------------------------------------------------------------------------------------------
// wWinMain
//-----------------------------------------------------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

	//-------------------------------------------------------------------------------------------------------------
	// Get command line arguments, return when parsing fails.
	//-------------------------------------------------------------------------------------------------------------
	int argc       = 0;
	wchar_t** argv = CommandLineToArgvW(lpCmdLine, &argc);
	if (argv == nullptr)
	{
		return YL_ERROR_PARSING;
	}

	//-------------------------------------------------------------------------------------------------------------
	// If no parameters provided, return.
	//-------------------------------------------------------------------------------------------------------------
	if (argc < 1)
	{
		LocalFree(argv);
		return YL_ERROR_NO_ARGUMENTS;
	}

	CommandLineArgvGuard argvGuard(argv);                                                                           // RAII wrapper for argv

    //-------------------------------------------------------------------------------------------------------------
    // Create the logger.
    //-------------------------------------------------------------------------------------------------------------
    Logger logger;
    
	try
	{
		//---------------------------------------------------------------------------------------------------------
		// Parse command line arguments.
		//---------------------------------------------------------------------------------------------------------
		CommandLineOptions options = CommandParser::ParseArguments(argc, argv);

		if (options.validOptions)
		{
			//-----------------------------------------------------------------------------------------------------
			// Initialize logger if requested
			//-----------------------------------------------------------------------------------------------------
			if (options.enableLogging)
			{
				logger.Initialize(options.logFilename);
			}

			//-----------------------------------------------------------------------------------------------------
			// Log the arguments if logging is enabled
			//-----------------------------------------------------------------------------------------------------
			logger.LogInfo(L"Command line  : " + std::wstring(GetCommandLineW()));
			logger.LogInfo(L"Application   : " + options.applicationPath);
			logger.LogInfo(L"Resolved path : " + options.resolvedPath);
			logger.LogInfo(L"Startup path  : " + options.startupPath);
			logger.LogInfo(L"Working path  : " + options.workingPath);
			logger.LogInfo(L"Mode          : " + _LaunchModeToString(options.mode));
			logger.LogInfo(L"Timeout       : " + std::to_wstring(options.timeoutSeconds) + L" seconds");
			logger.LogInfo(L"Argument count: " + std::to_wstring(options.arguments.size()));

			bool success = false;
			if (_FileExists(options.resolvedPath))
			{
				if (_IsYemLauncherLaunch(options.resolvedPath))
				{
					logger.LogError(L"Cannot launch the YemLauncher application itself.");
                    return YL_ERROR_SELF_LAUNCH;
				}
				else
				{
					//---------------------------------------------------------------------------------------------
                    // Create a ProcessManager and attempt to start the process.
					//---------------------------------------------------------------------------------------------
					ProcessManager pm(logger);
					success = pm.StartProcess(options.resolvedPath, options.arguments, options.workingPath, options.timeoutSeconds, options.excludeApplicationName);
				}
			}
			else
			{
				logger.LogError(L"Cannot find the resolved path for the application.");
                return YL_RESOLVED_PATH_NOT_FOUND;
			}

			//-----------------------------------------------------------------------------------------------------
			// Exit with appropriate code
			//-----------------------------------------------------------------------------------------------------
			logger.Log(L"********** YemLauncher exiting **********");
			return success ? YL_SUCCESS : YL_ERROR;
		}
		else
		{
			return YL_ERROR_INVALID_OPTIONS;
		}
	}
	catch (...)
	{
		//---------------------------------------------------------------------------------------------------------
		// Exception occurred during parsing or processing
		//---------------------------------------------------------------------------------------------------------
		return YL_ERROR;
	}
}
