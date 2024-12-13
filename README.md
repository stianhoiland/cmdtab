# cmdtab
Fast and lightweight macOS-style Alt-Tab app/window switcher replacement for Windows, written in the Lord's language, C.

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
- Super lightweight program (35kb)
- Simple, clean, clear, commented C source code (easy to change/fix/extend by you/me/everyone!)
- Lots of tiny, useful QoL features, like reverse, arrow keys & enter, cancel, restore, mouse support, wrap bump, quit app, close window, delayed show...
- So fast!
- The best macOS-style window switcher for Windows!
- C!

### Features
The basics of window switching are easy to understand, but why is **cmdtab** *the best* macOS-style window switcher alternative for Windows? Because it packs so many tiny, useful features into such a small and lightweight package without bloat:

- Configurable hotkey to cycle apps (default Alt-Tab)
- Configurable hotkey to cycle windows of the same app (default Alt-Tilde/Backquote)
- Reverse direction by holding Shift
- Arrow keys are supported (selects apps in switcher)
- Enter key is supported (switches to selected window)
- ~~Mouse is supported (click an app icon to switch to that app)~~ (WIP!)
- Big readable app icons (not those tiny bewildering window previews)
- Cancel and close the switcher by pressing Escape
- Smart key capture so key presses don't unexpectedly bleed through to other apps
- Wrap bump is hard to explain but easy to feel: Try holding Alt-Tab until the end, then press Tab again—works in reverse, too!
- Press Q to quit the selected app
- Press W to close the selected window
- Option to return to the initial window when cancelling switcher by pressing Escape. This doesn't override or block Windows' native Alt-Escape hotkey
- Press F4 while the switcher is open to quit **cmdtab**

That's a lot of useful stuff, and the code is small! Go read it, and learn some C while you're at it.

## Installing **cmdtab**
There's no installation. Just download the [latest version](https://github.com/stianhoiland/cmdtab/releases/latest) from the Releases section, unzip, and run. 
> [!TIP]
> **cmdtab** cannot see elevated applications like Task Manager unless you "Run as administrator".

### Uninstalling
**cmdtab** leaves no trace on your system, except for a registry key if you choose to enable autorun for **cmdtab**. You can remove this registry key by using **cmdtab** itself. Just run **cmdtab** one last time before you delete `cmdtab.exe` and choose "No" to autorun. This deletes any registry key **cmdtab** has created.

## Buildling from source
These instructions require `git`, `cmake`, and *Visual Studio* or *MSBuild*.
1. `git clone https://github.com/stianhoiland/cmdtab.git`
2. `cd cmdtab`
3. `mkdir build`
4. `cmake -G "Visual Studio 17 2022" -B build`
5. *(in developer console)* `devenv build\cmdtab.sln /build "MinSizeRel|x64"`
6. or: *(in developer console)* `msbuild build\cmdtab.sln /property:Configuration=MinSizeRel`
7. *(run cmdtab)* `build\MinSizeRel\cmdtab.exe`
