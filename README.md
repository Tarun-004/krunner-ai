# krunner-groq

A native KRunner plugin for KDE Plasma 6 that answers natural-language questions inline using the Groq API. Answers appear directly in KRunner's result list — no extra UI, no popup windows, no browser.

---

## What it does

Type a question into KRunner and an answer appears beneath your query, sourced from a large language model running on Groq's servers. Press Enter to copy the answer to your clipboard.

```
what is tcp
    Groq: TCP, or Transmission Control Protocol, is a connection-oriented
          protocol used for reliable data transfer over the internet.
          Press Enter to copy
```

It is deliberately conservative about when it activates. The plugin only fires for queries that look like genuine natural-language questions — multi-word queries that start with a question word (what, why, how, who, explain, define, ...) or end with a question mark. Single-word queries like `firefox`, `downloads`, or `settings` never trigger it, so KRunner's normal file, app, calculator, and other runners are completely unaffected. The plugin also runs at the lowest possible match priority, so even when it does return an answer, it never outranks a real file or application match.

---

## Requirements

- KDE Plasma 6
- KF6 Runner, KF6 CoreAddons, KF6 Config, KF6 I18n (installed as part of a standard Plasma 6 desktop)
- Qt 6.6 or later
- CMake and extra-cmake-modules

On Arch Linux:

```
sudo pacman -S cmake extra-cmake-modules
```

---

## Getting a Groq API key

Sign up at https://console.groq.com and create a free API key. The free tier is generous and the inference is fast enough that answers typically arrive in well under a second.

---

## Build and install

```bash
git clone https://github.com/your-username/krunner-groq.git
cd krunner-groq
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
sudo make install
```

The plugin is installed to `/usr/lib/qt6/plugins/kf6/krunner/krunner_groq.so`.

---

## Configuration

Create `~/.config/krunner_groqrc` with your API key:

```ini
[General]
ApiKey=gsk_your_key_here
```

The runner reads this file directly at runtime. No environment variables, no session restart, no manual launching required. KRunner is already part of your Plasma session and will pick up the key immediately on its next restart.

Restart KRunner once to load the plugin:

```bash
kquitapp6 krunner 2>/dev/null
```

KRunner will relaunch automatically the next time you open it (Alt+Space or Alt+F2). From this point on everything runs silently in the background as part of your normal Plasma session.

---

## Enabling the plugin

Open System Settings, search for "KRunner" or navigate to Plasma Search, and make sure "Groq AI Answers" is toggled on in the plugin list. It should be enabled by default after installation, but worth checking on the first run.

Alternatively, enable it from the command line:

```bash
kwriteconfig6 --file krunnerrc --group Plugins --key krunner_groqEnabled true
```

---

## How it works

- The runner checks every query against a list of question lead-words and rejects anything that does not look like a natural-language question. Single-word queries are always rejected.
- A 350ms idle debounce prevents API calls from firing on every keystroke. The runner waits until you pause typing, then checks whether the query is still current before making any network request.
- The HTTP request to the Groq API is made on KRunner's dedicated runner worker thread using a local event loop with a 4-second hard timeout. KRunner's UI and all other runners remain fully responsive while the request is in flight.
- If the user clears the query or types something new before the response arrives, the in-flight request is cancelled immediately.
- Answers are displayed using KRunner's native multi-line match layout. No custom UI is involved.

---

## Project structure

```
CMakeLists.txt
src/
    krunner_groq.json     Plugin metadata (name, description, icon)
    groqrunner.h/.cpp     AbstractRunner subclass: query filtering, debounce, match creation
    groqclient.h/.cpp     Async HTTP wrapper around the Groq chat completions API
```

---

## Configuration reference

| File | Key | Description |
|---|---|---|
| `~/.config/krunner_groqrc` | `[General] ApiKey` | Your Groq API key |

The model in use is `llama-3.1-8b-instant`. To switch models, change the `GROQ_MODEL` constant in `src/groqclient.cpp` and rebuild. Other fast models available on Groq include `llama-3.3-70b-versatile` and `gemma2-9b-it`.

---

## License

GPL-2.0-or-later