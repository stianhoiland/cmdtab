# cmdtab
Fast and lightweight macOS-style (or Windows XP style, if you want that) Alt-Tab app/window switcher replacement for Windows, written in the Lord's language, C.

![cmdtab-screenshot](https://github.com/stianhoiland/cmdtab/assets/2081712/ec5d0d61-005f-4123-b191-8d5b49d1f7db)

### What's the deal?
1. On Windows Alt-Tab cycles through different windows from different apps all mixed together, showing small window previews
2. On macOS Cmd-Tab cycles through different apps, showing big, clear app icons
3. On macOS there is a separate hotkey that cycles through windows of the same app

Here's a real life comparison GIF between Alt-Tab and **cmdtab** (notice the scrollbar in Alt-Tab, haha):

![comparision-gif](https://github.com/user-attachments/assets/440e2d71-6bbc-4299-acf5-cdc707371193)

So, you like the way Apple does it, but you're using Windows? **cmdtab** for Windows fixes that:

- A hotkey to cycle apps (Chrome → Spotify → File Explorer)
- A different hotkey to cycle windows (Chrome1 → Chrome2 → Chrome3)
- Big readable app icons
- Super lightweight program (150kb)
- Simple, clean, clear, commented C source code (easy to change/fix/extend by you/me/everyone!)
- Lots of tiny, useful QoL features, like reverse, arrow keys & enter, cancel, restore, mouse support, wrap bump, quit app, close window, delayed show...
- So fast!
- The best macOS-style window switcher for Windows!
- C!

### Features
The basics of window switching are easy to understand, but why is **cmdtab** *the best* macOS-style window switcher alternative for Windows? Because it packs so many tiny, useful features into such a small and lightweight package without bloat:

- Configurable hotkey to cycle apps (default Alt-Tab)
- Configurable hotkey to cycle windows of the same app (default Alt-Tilde/Backquote)
- Setting `showAllWindows` to treat windows (not apps) as the first-class object, collapsing the two-level navigation into one (as Windows does it natively).
- Reverse direction by holding Shift
- Arrow keys are supported (selects apps in switcher)
- Enter key is supported (switches to selected window)
- ~~Mouse is supported (click an app icon to switch to that app)~~ (WIP!)
- Big readable app icons (not those tiny bewildering window previews)
- Cancel and close the switcher by pressing Escape
- Smart key capture so key presses don't unexpectedly bleed through to other apps
- Wrap bump makes autorepeat stop on the last icon. Very handy!
- Press Q to quit the selected app, W to close the selected window
- Option to return to the initial window when cancelling switcher by pressing Escape. This doesn't override or block Windows' native Alt-Escape hotkey
- Press F4 while the switcher is open to quit **cmdtab**
- Change appearance and behavior by editing `cmdtab.ini` in the same directory as the executable (example to be found in the code tree)

That's a lot of useful stuff, and the code is small! Go read it, and learn some C while you're at it.

## Installing **cmdtab**
There's no installation. Just download the [latest version](https://github.com/stianhoiland/cmdtab/releases/latest) from the Releases section, unzip, and run. 

### Run as administrator
**cmdtab** cannot see elevated applications like Task Manager unless you "Run as administrator", but works well otherwise.

### Autorun
> [!NOTE]
> The autorun that **cmdtab** enables if you choose "Yes" when it prompts you about autorun is not "Run as administrator". In the future, **cmdtab** will support run automatically as administrator, but for now you must manually configure this by using the command below.

It makes sense to have **cmdtab** "Run as administrator", and it makes sense to have **cmdtab** run automatically on login. Doing either is easy, but doing both, i.e. run automatically as administrator, is not so easy. The only way to run automatically as administrator is to use the Windows Task Scheduler. To create an appropriate scheduled task from the Command Prompt run this command:
```console
schtasks /create /sc onlogon /rl highest /tn "cmdtab elevated autorun" /tr "C:\Users\<YOUR_USER_NAME>\Downloads\cmdtab-v1.5.1-win-x86_64\cmdtab.exe --autorun"
```
You can further customize the scheduled task created by that command by running `taskschd.msc`.

### Uninstalling
**cmdtab** leaves no trace on your system, except for a registry key if you choose "Yes" when  **cmdtab** prompts you about autorun (and the scheduled task mentioned above if you manually created it). You can remove this registry key by using **cmdtab** itself. Just run **cmdtab** one last time before you delete `cmdtab.exe` and choose "No" to autorun. This deletes any registry key **cmdtab** has created.

## Buildling from source
These instructions require `git`, `cmake`, and *Visual Studio* or *MSBuild*.
1. `git clone https://github.com/stianhoiland/cmdtab.git`
2. `cd cmdtab`
3. `mkdir build`
4. `cmake -G "Visual Studio 17 2022" -B build`
5. *(in developer console)* `devenv build\cmdtab.sln /build "MinSizeRel|x64"`
6. or: *(in developer console)* `msbuild build\cmdtab.sln /property:Configuration=MinSizeRel`
7. *(run cmdtab)* `build\MinSizeRel\cmdtab.exe`

## Release history

#### 1.6.0 (2025-09-26)

- `showAllWindows` setting: show windows (not apps) at the top level
- `rowLength` setting: show the resulting longer lists of icons in multiple rows
- `showAllTitles` setting: show abbreviated titles of all apps (or windows) and full title of selected one
- `cmdtab.ini` file: read config settings from file in the executable's directory

#### 1.5.1 (2024-12-12)

- fix bug causing cmdtab to stop remembering new window activations
- fix crash when invoked with 0 windows
- change icon to vibrant blue to suit both light and dark themes
- ensure msgbox gets kbd focus

#### 1.5.0 (2024-12-11)

- first proper version: new build system CMake, instructions in README.md
- add resource info to cmdtab.exe
- cmdtab.exe has a new (placeholder) icon
- fix crash caused by some apps' product name (#2)
- dark mode and accent color awareness
- don't ShowWindow if already foreground
- extensive logging
- massive memory usage reduction
- persistent window activation order
- improve error handling of access failure on getting filepath
- fix text alignment bug caused by not #including math.h
- fix text alignment bias from right to left
- fix stuck modifier keys
- fix alt key / window menu inconsistency compared to Windows Alt-Tab
- impl Alt-Q to quit process
- rely on switcher being top-most window vs. active window

#### 1.0 (some time in 2024)

There was no formal 1.0 release (nor a 1.1, 1.2, 1.3, or 1.4). 
Which of the versions to consider 1.0 is a matter of opinion.

#### 0.1 (some time in 2023)

Initially, cmdtab was only an idea. Then it grew and evolved.