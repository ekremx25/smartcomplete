# SmartComplete

**System-wide intelligent text prediction for Linux** — context-aware completion, next-word prediction, phrase completion, typo correction, emoji shortcodes, and optional AI reranking. Built on Fcitx5, works across all Wayland and X11 applications.

---

## Install (one command)

```bash
git clone https://github.com/ekremx25/smartcomplete.git
cd smartcomplete
./install.sh
```

That's it. The installer detects your distro (Arch, Debian, Fedora), installs dependencies, builds the project, runs the test suite, installs the Fcitx5 addon, configures environment variables, sets up the Fcitx5 profile, wires autostart into your compositor (Hyprland, Niri, Sway, GNOME, KDE), and verifies the installation — all in one run.

**Uninstall:** `./uninstall.sh` (add `--purge` to also delete your learned words).

**GUI launcher:** double-click `SmartComplete-Install.desktop` from your file manager.

---

## How it works

SmartComplete combines six prediction strategies into a single ranked candidate list:

| Strategy | Source | What it gives you |
|---|---|---|
| **Trie prefix search** | 84K-word dictionary | Fast completions as you type |
| **Bigram prediction** | 48K word-pair frequencies | Next-word suggestions after space |
| **Phrase completion** | 787 multi-word triggers | Common expressions (`how are` → `you doing`) |
| **Grammar rules** | 26K+ contextual patterns | Natural verb/modal/preposition usage |
| **Typo correction** | Edit-distance + rules | Fixes common misspellings |
| **Emoji shortcodes** | 303 entries | `:smile` → 😊, `:heart` → ❤️ |

Every selection you make **improves future rankings** — user frequency gets a 10× multiplier in the scoring model, persisted to disk.

---

## Architecture

Layered design, each layer independently testable:

```
┌─────────────────────────────────────────┐
│  Fcitx5 Engine                          │  src/engine/
│    key events → candidate list → commit │  (engine, engine_config, text_utils)
├─────────────────────────────────────────┤
│  Prediction Engine                      │  src/predictor/
│    6 strategies + AI reranker + scoring │
├─────────────────────────────────────────┤
│  Data Layer                             │  data/
│    dict, frequency, ngram, rules, emoji │
└─────────────────────────────────────────┘
```

Key design decisions:
- **Data-driven**: zero hardcoded word/rule data — all 26K+ rules and 303 emoji are loaded from external `data/*` files
- **O(1) rule lookups**: grammar rules indexed by hash map (26K rules, constant-time queries)
- **Shared utilities**: `text_utils` module eliminates duplicated lowercase/fold/case-match logic
- **Secure AI path**: OpenAI calls use `fork()/execvp()` directly (no shell subprocess, no command injection surface)
- **Terminal pass-through**: input methods make no sense in shells — SmartComplete stays out of the way in 27 known terminal emulators and shells

---

## Usage

| Key | Action |
|---|---|
| `Tab` | Accept first or selected suggestion |
| `↑` / `↓` | Navigate candidates |
| `Enter` | Accept selected, or commit buffer |
| `Space` | Commit word + show next-word prediction |
| `Esc` | Dismiss candidates |
| `:smile` | Emoji shortcode mode |

Common auto-corrections: `dont` → `don't`, `i am` → `I'm`, `you are` → `you're`, `we are` → `we're`, `they are` → `they're`, `it is` → `it's`.

---

## Terminal pass-through

SmartComplete automatically disables itself in programs where shell completion handles prediction natively. Default blocklist:

**Terminal emulators:** kitty, alacritty, foot, wezterm, xterm, urxvt, rxvt, st, gnome-terminal, konsole, xfce4-terminal, lxterminal, mate-terminal, deepin-terminal, terminator, tilix, hyper, terminology, blackbox, ptyxis, cool-retro-term, termite

**Shells / multiplexers:** zsh, bash, fish, tmux, screen

Customize via `~/.config/linuxcomplete/linuxcomplete.conf`:

```jsonc
{
    // Add to the default list without replacing it:
    "disabled_programs_extend": ["vim", "emacs", "code"],

    // Or replace the default list entirely:
    "disabled_programs": ["only", "these", "apps"]
}
```

---

## AI reranker (optional)

SmartComplete's core prediction works fully offline. When enabled, AI reranking **only reorders candidates already generated locally** — it never invents new words, and if the network fails the system transparently falls back to local ranking.

### Design safeguards

- **Confidence gating**: AI only invoked for ambiguous candidates (score gap below threshold)
- **LRU cache**: identical contexts skip redundant API calls (128 entries default)
- **Hard timeout**: 1.2s default; after that, local ranking wins
- **No shell subprocess**: curl runs via `fork()/execvp()` with direct argv — no command injection
- **RAII temp files**: payload files auto-cleaned even on crashes
- **API key never logged**: passed via HTTP header only, never to argv or temp files

### Configuration

Via environment variables:

```bash
export OPENAI_API_KEY=sk-...
export LINUXCOMPLETE_AI_ENABLED=1
export LINUXCOMPLETE_AI_MODEL=gpt-4o-mini        # default
export LINUXCOMPLETE_AI_TIMEOUT_MS=1200
export LINUXCOMPLETE_AI_SMART_FALLBACK=1
export LINUXCOMPLETE_AI_GAP_THRESHOLD=160
export LINUXCOMPLETE_AI_MAX_CACHE_ENTRIES=128
export LINUXCOMPLETE_AI_DEBUG=1
```

Or via `config/linuxcomplete.conf` — same field names without the `LINUXCOMPLETE_AI_` prefix (e.g. `ai_model`, `ai_timeout_ms`).

---

## Testing

```bash
cd build
ctest --output-on-failure
```

11 test suites covering every module:

| Test | Coverage |
|---|---|
| `english_grammar_sanity` | Rule file format, grammar assertions |
| `predictor_learning_regression` | User frequency boost, bigram learning |
| `predictor_quality_regression` | Candidate ranking, deduplication |
| `ai_reranker_regression` | Confidence gating, cache, JSON parsing |
| `data_quality_regression` | N-gram/phrase format, duplicate detection |
| `text_utils` | String manipulation helpers |
| `grammar_index` | O(1) hash map lookups |
| `emoji_index` | Prefix-bucketed emoji search |
| `blocklist` | Terminal pass-through logic |
| `rules_validation` | `rules_tool.py` format checks |
| `rules_tool_regression` | CLI tool end-to-end behavior |

---

## Rules tool

Manage grammar and typo data files:

```bash
python3 scripts/rules_tool.py validate                                   # check format
python3 scripts/rules_tool.py normalize --write                          # sort + dedup
python3 scripts/rules_tool.py stats                                      # row counts
python3 scripts/rules_tool.py check-conflicts                            # detect contradictions
python3 scripts/rules_tool.py resolve-conflicts --write                  # keep strongest score
python3 scripts/rules_tool.py add-typo dontt "don't"
python3 scripts/rules_tool.py add-pair en_grammar_pair_rules.txt hello world 120
python3 scripts/rules_tool.py add-triple en_grammar_triple_rules.txt i am ready 180
```

---

## Development

Manual build (without the installer):

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc)"
ctest --output-on-failure
sudo cmake --install .
```

Dependencies:
- C++17 compiler (gcc/clang)
- CMake ≥ 3.20
- Fcitx5 + dev headers
- nlohmann/json ≥ 3.2

Project layout:

```
smartcomplete/
├── install.sh, uninstall.sh        # one-click scripts
├── SmartComplete-Install.desktop   # GUI launcher
├── src/engine/                     # Fcitx5 integration
├── src/predictor/                  # prediction engine
├── data/                           # dictionaries, rules, emoji
├── tests/                          # unit + regression tests
└── scripts/                        # install + rules_tool.py
```

---

## Supported applications

Any application with Fcitx5 support:

- **Browsers**: Firefox, Chrome, Brave
- **Editors**: VS Code, Kate, Gedit, Sublime Text
- **Messaging**: Telegram, Discord, Element
- **Office**: LibreOffice
- **All GTK and Qt applications**

> **Not supported** (by design): terminal emulators and shells — see [Terminal pass-through](#terminal-pass-through).

> **Note:** auto-correction features that rewrite surrounding text (e.g. `i am` → `I'm` with deletion) depend on the application exposing Fcitx5's `SurroundingText` capability. If an app doesn't support it, SmartComplete falls back to commit-only mode (no deletion).

---

## License

MIT — see [LICENSE](LICENSE).
