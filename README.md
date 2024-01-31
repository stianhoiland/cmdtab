# cmdtab[^1]
The best macOS-style Alt-Tab window/app switcher replacement for Windows, written in straight<sup>delicious</sup> C.

## What's the deal?

1. On Windows, Alt-Tab cycles through a mix of different windows and applications with small window previews.
2. On macOS, Cmd-Tab cycles through (only) apps, with big, clear icons (sort of like the taskbar on Windows: only app icons).
3. On macOS, there is an additional hotkey that cycles through (only) windows of the same app.

You like the way Apple does it, but you're using Windows? CmdTab for Windows fixes that:

- Hotkey *(default Alt-Tab)* to switch apps: Chrome → Spotify → File Explorer
- Hotkey *(default Alt-`)* to switch windows: Chrome1 → Chrome2 → Chrome3
- Big readable app icons (not those tiny bewildering window previews)
- Super small and lightweight program (currently less than 200kb)
- Simple, clean, clear, commented C source code (easy to change/fix/extend by you/me/everyone!)
- Lots of tiny, useful QoL features, like reverse, arrow keys, cancel, restore, wrap bump, quit app, close window, delayed show...
- So fast!
- The best macOS-style window switcher on Windows!
- C!

### Features
The basics of window switching are easy to understand, but why is CmdTab *the best* Mac style window switcher alternative for Windows? Because it packs so many tiny, useful features into such a small and lightweight package without bloat (because it's well coded!):

- Alt-Tab to cycle apps (configurable hotkey)
- Alt-` to cycle windows of same app (configurable hotkey)
- Reverse direction by holding Shift
- Arrow keys are supported as an alternative to pressing Tab (7 lines of code to support arrow keys!)
- Close the switcher to cancel switching by pressing Escape (while Alt is still down)
- Big readable app icons
- ~~Click an app icon with the mouse to switch to that app (WIP!)~~
- Smart key capture so keys don't unexpected bleed through to other apps
- Wrap bump is hard to explain but easy to feel: Try holding Alt-Tab until the end, then press Tab again (while Alt is still down)—works in reverse, too!
- When cycling windows of the same app, cancel and return to the initial window by pressing Escape (while Alt is still down). This doesn't override or block Windows' native Alt-Escape hotkey
- Press Q (while Alt is still down) to close the selected application (by closing all its windows)
- Press W or F4 (while Alt is still down) to close the selected window
- Delayed show[^2] prevents the switcher from indecipherably flashing and disappearing when all you wanted was to instantly Alt-Tab *once* to get back to the previous app[^3].

That's a lot of useful stuff, and the code is so small! Go read it, and learn some C while you're at it.

[^1]: Keywords (you're welcome): *CmdTab for Windows // macOS window switcher on Windows // Alt-Tab replacement alternative // Mac style Alt-Tab window switching replacement for Windows*
[^2]: I had to explain "delayed show" to my gf. Here are some (of very many) sentences she understood:
  The actual window switching mechanism is completely independent of the UI.
  You don't have to wait for the UI to show on screen before you switch.
  You can press as fast as you can, even if you don't see anything on screen!
  It's not the UI that decides which window you switch to, it's the keyboard keys you press.
  The UI just pops up to give you a visual representation of what's gonna happen.
  (The UI doesn't pop up if you Alt-Tab super quickly—that's the whole point of delayed show) 
[^3]: Don't worry, there is absolutely no delay in the actual switching, it's instant—switching is actually completely independent of the switcher UI. Only *showing the visual UI* is delayed by a couple of milliseconds, to avoid wasting precious (brain and CPU-) cycles by showing up, confusing you, only to instantly disappear again[^4].
[^4]: Omg why is this so hard to explain
