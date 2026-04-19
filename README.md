# YemLauncher

The YemLauncher is a small Windows executable, which can start another Windows application in the background, without creating any window (the application runs truly in the background). This also means, no interaction with users can exist, unless the application explicitly handles it.

---

## Syntax

```
YemLauncher [-log [<file>]] [-timeout [seconds]] [-workingdir <path>] [-command]
            {-powershell|-pwsh} {<command>|<script>} [<arg>]

YemLauncher [-log [<file>]] [-exclude] [-resolve] [-timeout [seconds]] 
            [-workingdir <path>] -application <application> [<arg>]
```

## Options

### `-application` | `-a`

We expect a direct launch of an application. This must be the last option before the actual application is specified.

### `-command` | `-c`

PowerShell expects a direct command, not a script file. The same conditions apply as when running a pwsh command directly from a cmd.exe prompt. If not given a script is expected.

### `-exclude` | `-e`

Excludes the application name from the arguments passed to the application to start. Normally arg[0] should be the application name. If the application does not handle this correctly, use the `-exclude` option to remove the application name and let the first argument be arg[0].

### `-log [<file>]` | `-l [<file>]`

Enable logging for the YemLauncher itself. If `<file>` is given, it is used as the log file. If not given, the log file will be set to `YemLauncher_<yyyymmdd>.log`

If no path is given in `<file>` or when no `<file>` is given the log file will be created in the systems `%TEMP%` folder.

### `-pwh` | `-p`
### `-powershell` | `-wp`

Start pwsh.exe or powershell.exe, the following options are added to the commandline:

- `-NoLogo`
- `-NoProfile`
- `-NonInteractive`
- `-WindowStyle hidden`
- `-Executionpolicy bypass`
- `-File` | `-Command` 
  - If `-command` is given, `-Command` is added to the commandline. Otherwise `-File` is added.

This also means that after the `-pwsh` or `-powershell` option, only the name of a script or a direct command and its arguments is allowed.

### `-resolve` | `-r`

Tries to resolve the name of the application given by searching the path environment variable if the application cannot be found in the active directory.

> **Note:** This will start the first application found, it is preferred to provide the full path to the executable. Use with care!

### `-timeout [seconds]` | `-t [seconds]`

Timeout in seconds for the application launched. After the timeout is passed the application is killed. When no timeout option is given the YemLauncher will return immediately after launching the application and will not end it. If seconds is not given, it will default to 600 seconds (10 minutes).

### `-workingdir <path>` | `-w <path>`

The path to the working directory for the process to be started. If not given, the startup directory is used as working directory.

### `<application>`

The application to start.

### `<arg>`

The arguments for the `<application>` or the powershell `<script>`.

### `<command>` | `<script>`

The command or script to pass to powershell.

## Notes

1. The options `-application`, `-powershell` and `-pwsh` must be the last option on the commandline before specifying the application or script/command and its arguments.

2. Only one of the options `-application`, `-powershell`, `-pwsh` can and must be given. If not, the YemLauncher will exit immediately without logging any action.

3. When searching for the pwsh.exe, we ignore non-stable releases.
