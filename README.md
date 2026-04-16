# SmartComplete

SmartComplete (LinuxComplete) is a system-wide text completion and next-word prediction engine for Linux, built on Fcitx5 for Wayland environments. It combines dictionary-based suggestions, context awareness, user learning, typo correction, and emoji shortcodes in a single lightweight input method engine.

## Highlights

- **System-wide**: works across browsers, editors, office suites, messaging apps, and terminal emulators
- **Context-aware prediction**: prefix search, bigram data, phrase completion, and grammar rule scoring work together
- **User learning**: selected words and word transitions improve suggestion ranking over time
- **Optional AI reranker**: reorders existing candidates based on context via OpenAI API
- **Typo correction and contraction support**: produces more natural suggestions during English typing
- **Emoji shortcodes**: suggests emoji candidates for `:smile`-style inputs

## How It Works

The engine is organized into three layers:

1. **`src/engine/`** — Fcitx5 integration: captures key events, builds candidate lists, and commits selected text to the active application.

2. **`src/predictor/`** — Prediction logic: generates suggestions using a trie, dictionary, user frequency data, n-gram model, phrase model, and grammar rules.

3. **`data/`** — Static data: system dictionaries, frequency files, n-gram data, phrase completion lists, and grammar rule files.

## Features

| Feature | Description |
|---|---|
| Prefix completion | Fast candidate list based on typed prefix |
| Next-word prediction | Suggests next word based on context after space |
| Phrase completion | Multi-word continuations from two-word triggers |
| User learning | Builds user frequency and learned bigrams from selections |
| AI reranker | Optionally reorders candidate list via OpenAI Responses API |
| Typo correction | Suggests corrections for common misspellings |
| Contraction support | Handles English contractions and grammar heuristics |
| Emoji support | Suggests emojis from `:` shortcode prefixes |

## Supported Applications

SmartComplete works with any application that supports Fcitx5 input methods:

- Firefox, Chrome, Brave
- VS Code, Kate, Gedit
- Kitty, Alacritty, Foot, WezTerm (terminal emulators)
- Telegram, Discord
- LibreOffice
- GTK and Qt applications

> **Note:** Actual behavior may vary depending on the application's Fcitx5 and surrounding text support. Auto-correction and surrounding text rewrite features are application-dependent.

## Installation

### Prerequisites

#### Arch Linux

```bash
sudo pacman -S fcitx5 fcitx5-qt fcitx5-gtk fcitx5-configtool cmake gcc nlohmann-json

# Optional: hunspell-based extended word list
sudo pacman -S hunspell-en_us
```

#### Debian / Ubuntu

```bash
sudo apt install fcitx5 fcitx5-frontend-qt5 fcitx5-frontend-gtk3 fcitx5-config-qt cmake g++ nlohmann-json3-dev
```

#### Fedora

```bash
sudo dnf install fcitx5 fcitx5-qt fcitx5-gtk fcitx5-configtool cmake gcc-c++ json-devel
```

### Building from Source

```bash
git clone https://github.com/ekremx25/smartcomplete.git
cd smartcomplete
mkdir -p build && cd build
cmake ..
cmake --build . -j"$(nproc)"
```

### Installing Data Files

```bash
sudo mkdir -p /usr/share/linuxcomplete/{dict,frequency,ngram,rules,config}
sudo cp data/dict/*.txt /usr/share/linuxcomplete/dict/
sudo cp data/frequency/*.txt /usr/share/linuxcomplete/frequency/
sudo cp data/ngram/*.txt /usr/share/linuxcomplete/ngram/
sudo cp data/rules/*.txt /usr/share/linuxcomplete/rules/
sudo cp config/linuxcomplete.conf /usr/share/linuxcomplete/config/

# Create user data directory
mkdir -p ~/.local/share/linuxcomplete/user
```

### Fcitx5 Environment Variables

Add the following to your shell profile (`~/.bashrc` or `~/.zshrc`):

```bash
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export SDL_IM_MODULE=fcitx
export GLFW_IM_MODULE=ibus
```

### Compositor Autostart

#### Hyprland

Add to `~/.config/hypr/execs.conf` or `hyprland.conf`:

```bash
exec-once = fcitx5 -d
```

#### Niri

Add to `~/.config/niri/config.kdl`:

```
spawn-at-startup "fcitx5" "-d"
```

#### Sway

Add to `~/.config/sway/config`:

```bash
exec fcitx5 -d
```

### Fcitx5 Profile Setup

To set SmartComplete as the default input method:

```bash
killall fcitx5 2>/dev/null || true

mkdir -p ~/.config/fcitx5

cat > ~/.config/fcitx5/profile <<'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=linuxcomplete

[Groups/0/Items/0]
Name=keyboard-us
Layout=us

[Groups/0/Items/1]
Name=linuxcomplete
Layout=

[GroupOrder]
0=Default
EOF

fcitx5 -d
```

Alternatively, use `fcitx5-configtool` to add SmartComplete via the GUI.

## Usage

| Key | Behavior |
|---|---|
| `Tab` | Accept the first or selected suggestion |
| `Up` / `Down` | Navigate between candidates |
| `Enter` | Accept the selected suggestion or commit the active buffer |
| `Space` | Commit the word and trigger next-word prediction |
| `Esc` | Dismiss the open candidate list |
| `:` | Start emoji shortcode mode |

## Tests

The project includes the following regression tests:

- English grammar sanity check
- Predictor learning regression test
- Predictor quality regression test
- AI reranker regression test
- Data quality regression test
- Rules validation test

To run all tests:

```bash
cd build
ctest --output-on-failure
```

## Rules Tool

Validate, normalize, or add entries to the grammar and typo rule files under `data/rules/`:

```bash
python3 scripts/rules_tool.py validate
python3 scripts/rules_tool.py normalize
python3 scripts/rules_tool.py normalize --write
python3 scripts/rules_tool.py stats
python3 scripts/rules_tool.py check-conflicts
python3 scripts/rules_tool.py resolve-conflicts
python3 scripts/rules_tool.py resolve-conflicts --write
python3 scripts/rules_tool.py add-typo dontt "don't"
python3 scripts/rules_tool.py add-pair en_grammar_pair_rules.txt hello world 120
python3 scripts/rules_tool.py add-triple en_grammar_triple_rules.txt i am ready 180
```

The tool performs the following checks:

- Detects missing rule files
- Validates field count against tab-delimited format
- Verifies score fields are integers
- Flags exact duplicate rows as errors
- `normalize` sorts files and removes exact duplicates
- `stats` summarizes row counts per file
- `check-conflicts` reports same-key entries with both positive and negative scores
- `resolve-conflicts` keeps the strongest absolute score and removes conflicting rows

## AI Reranker

AI support is disabled by default. When enabled, the predictor continues generating candidates locally; OpenAI only reorders them based on context. If the network request fails, the system automatically falls back to local ranking.

The AI layer includes the following safeguards:

- **Smart fallback**: AI is only invoked for genuinely ambiguous or context-sensitive candidates
- **Confidence gating**: local ranking is preserved when the score gap is decisive
- **Cache**: identical context and candidate lists skip redundant API calls
- **Debug logging**: decisions and fallback reasons can be printed to the terminal

Configuration via environment variables:

```bash
export OPENAI_API_KEY=...
export LINUXCOMPLETE_AI_ENABLED=1
export LINUXCOMPLETE_AI_MODEL=gpt-5-mini
export LINUXCOMPLETE_AI_TIMEOUT_MS=1200
export LINUXCOMPLETE_AI_SMART_FALLBACK=1
export LINUXCOMPLETE_AI_GAP_THRESHOLD=160
export LINUXCOMPLETE_AI_MAX_CACHE_ENTRIES=128
export LINUXCOMPLETE_AI_DEBUG=1
```

Alternatively, configure via `config/linuxcomplete.conf` fields: `ai_rerank_enabled`, `ai_smart_fallback`, `ai_debug_logging`, `ai_model`, `ai_timeout_ms`, `ai_uncertainty_gap_threshold`, and `ai_max_cache_entries`.

## Technical Notes

- Written in C++17
- Build system: CMake
- Installed as a Fcitx5 addon
- Dictionary lookup uses a UTF-32 trie
- Ranking combines dictionary frequency, user frequency, n-gram scores, and grammar rules
- AI layer only reranks candidates; it does not generate new ones
- The loading layer merges duplicate n-gram and phrase entries, keeping the strongest score
- Typo and grammar rules are loaded from external data files under `data/rules/`

## Roadmap

See [ROADMAP.md](ROADMAP.md) for planned improvements.

Key areas:

- Expand test coverage
- Make predictor rules more modular
- Improve phrase and n-gram datasets
- Reduce application-specific behavioral differences
- Move typo and grammar data sources to external, verifiable formats

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
