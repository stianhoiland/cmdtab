# cmdtab
The best Mac-style Alt-Tab window/app switcher replacement for Windows, written in the Lord's language.

![screenshot2-cropped2](https://github.com/stianhoiland/cmdtab/assets/2081712/ec5d0d61-005f-4123-b191-8d5b49d1f7db)

### What's the deal?
1. On Windows Alt-Tab cycles through different windows from different apps all mixed together, showing small window previews
2. On macOS Cmd-Tab cycles through apps, showing big, clear icons
3. On macOS there is a separate hotkey that cycles through windows of the same app

Here's a real life comparison GIF between Alt-Tab and **cmdtab**:

![screenshot-comparision2](https://github.com/user-attachments/assets/440e2d71-6bbc-4299-acf5-cdc707371193)

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

## Buildling from source
These instructions require `git`, `cmake`, and *Visual Studio* or *MSBuild*.
1. `git clone https://github.com/stianhoiland/cmdtab.git`
2. `cd cmdtab`
3. `mkdir build`
4. `cmake -G "Visual Studio 17 2022" -B build`
5. *(in developer console)* `devenv build\cmdtab.sln /build "MinSizeRel|x64"`
6. or: *(in developer console)* `msbuild build\cmdtab.sln /property:Configuration=MinSizeRel`
7. *(run cmdtab)* `build\MinSizeRel\cmdtab.exe`
