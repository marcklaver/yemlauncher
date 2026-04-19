#pragma once

constexpr int YL_SUCCESS                 =   0;
constexpr int YL_ERROR                   =  99;                                                                     // General error
constexpr int YL_ERROR_PARSING           = 100;                                                                     // General parsing error
constexpr int YL_ERROR_NO_ARGUMENTS      = 101;                                                                     // No arguments provided
constexpr int YL_ERROR_SELF_LAUNCH       = 102;                                                                     // Attempt to launch the application itself
constexpr int YL_ERROR_INVALID_OPTIONS   = 103;                                                                     // Invalid combination of options
constexpr int YL_RESOLVED_PATH_NOT_FOUND = 104;                                                                     // Application to launch not found at the resolved path
