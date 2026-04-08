# LinuxComplete

LinuxComplete, Wayland ortamında Fcitx5 üzerine kurulu sistem genelinde metin tamamlama ve sonraki kelime tahmini sunan bir Linux input method eklentisidir. Türkçe ve İngilizce için sözlük tabanlı öneri, bağlam farkındalığı, kullanıcı öğrenmesi, typo düzeltme ve emoji kısa kodları gibi özellikleri tek bir hafif motor içinde birleştirir.

## Öne Çıkanlar

- Sistem genelinde çalışır: tarayıcılar, editörler, ofis uygulamaları, mesajlaşma uygulamaları ve terminal emülatörleri dahil
- İki dilli tahmin: Türkçe ve İngilizce için aynı motor içinde otomatik bağlam kullanımı
- Bağlam farkındalığı: prefix araması, bigram verisi, phrase completion ve grammar rule skorları birlikte kullanılır
- Kullanıcı öğrenmesi: seçilen kelimeler ve kelime geçişleri zamanla öneri sırasını iyileştirir
- Opsiyonel AI reranker: mevcut adayları bağlama göre yeniden sıralayabilir
- Typo correction ve contraction desteği: özellikle İngilizce yazım akışında daha doğal öneriler üretir
- Emoji shortcodes: `:smile` benzeri girişler için emoji adayı gösterebilir

## Nasıl Çalışır

Motor üç ana katmandan oluşur:

1. `src/engine/`
Fcitx5 entegrasyonu, tuş olaylarını yakalar, aday listesini oluşturur ve seçilen metni ilgili uygulamaya commit eder.

2. `src/predictor/`
Trie, sözlük, kullanıcı frekansı, n-gram, phrase modeli ve grammar kurallarıyla tahmin üretir.

3. `data/`
Sistem sözlükleri, frekans dosyaları, n-gram verileri, phrase completion listeleri ve predictor kural dosyalarını içerir.

## Özellikler

| Özellik | Açıklama |
|---|---|
| Prefix completion | Yazılan ön eke göre hızlı aday listesi üretir |
| Next-word prediction | Boşluk sonrası bağlama göre yeni kelime önerir |
| Phrase completion | Belirli iki kelimelik tetikleyicilerden çok kelimeli devam önerir |
| User learning | Seçimlerden kullanıcı frekansı ve learned bigram üretir |
| AI reranker | OpenAI Responses API ile mevcut aday listesini isteğe bağlı olarak yeniden sıralar |
| Turkish support | Türkçe karakterler ve Türkçe bağlam kurallarıyla çalışır |
| English support | Contraction, typo correction ve grammar heuristics içerir |
| Emoji support | `:` ile başlayan kısa kodlardan emoji önerir |

## Desteklenen Uygulamalar

LinuxComplete, Fcitx5 input method desteği olan uygulamalarda çalışacak şekilde tasarlanmıştır. Tipik kullanım alanları:

- Firefox, Chrome, Brave
- VS Code, Kate, Gedit
- Kitty, Alacritty, Foot, WezTerm gibi terminal emülatörleri
- Telegram, Discord
- LibreOffice
- GTK ve Qt tabanlı uygulamalar

Not: Gerçek davranış, uygulamanın Fcitx5 ve surrounding text desteğine göre değişebilir. Özellikle otomatik düzeltme ve çevre metin üzerinde yeniden yazma gerektiren davranışlar uygulamaya bağlı olarak farklı sonuç verebilir.

## Kurulum

### Arch Linux / AUR

```bash
yay -S linuxcomplete
```

### Gerekli Paketler

#### Arch Linux

```bash
# Derleme ve çalışma zamanı bağımlılıkları
sudo pacman -S fcitx5 fcitx5-qt fcitx5-gtk fcitx5-configtool cmake gcc nlohmann-json

# İngilizce sözlük (opsiyonel — hunspell tabanlı geniş kelime listesi)
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

### Kaynaktan Derleme

```bash
git clone https://github.com/ekremx25/linuxcomplete.git
cd linuxcomplete
mkdir -p build
cd build
cmake ..
cmake --build . -j"$(nproc)"
sudo cmake --install .
```

### Sözlükleri ve Kuralları Kur

```bash
# Sözlükler
sudo mkdir -p /usr/share/linuxcomplete/{dict,frequency,ngram,rules,config}
sudo cp data/dict/*.txt /usr/share/linuxcomplete/dict/
sudo cp data/frequency/*.txt /usr/share/linuxcomplete/frequency/
sudo cp data/ngram/*.txt /usr/share/linuxcomplete/ngram/
sudo cp data/rules/*.txt /usr/share/linuxcomplete/rules/
sudo cp config/linuxcomplete.conf /usr/share/linuxcomplete/config/

# Kullanıcı veri dizini
mkdir -p ~/.local/share/linuxcomplete/user
```

### Fcitx5 Ortam Değişkenleri

Shell profiline (`~/.bashrc` veya `~/.zshrc`) aşağıdaki değişkenleri ekleyin:

```bash
# LinuxComplete — Input Method Configuration
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export SDL_IM_MODULE=fcitx
export GLFW_IM_MODULE=ibus
```

### Compositor Autostart

#### Hyprland

`~/.config/hypr/execs.conf` veya `hyprland.conf` dosyasına ekleyin:

```bash
exec-once = fcitx5 -d
```

#### Niri

`~/.config/niri/config.kdl` dosyasına ekleyin:

```
spawn-at-startup "fcitx5" "-d"
```

#### Sway

`~/.config/sway/config` dosyasına ekleyin:

```bash
exec fcitx5 -d
```

### Fcitx5 Profil Ayarı

LinuxComplete’i varsayılan input method olarak ayarlamak için:

```bash
# Fcitx5’i durdurun
killall fcitx5

# Profili ayarlayın
cat > ~/.config/fcitx5/profile <<’EOF’
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=linuxcomplete

[Groups/0/Items/0]
Name=linuxcomplete
Layout=

[GroupOrder]
0=Default
EOF

# Fcitx5’i başlatın
fcitx5 -d
```

Alternatif olarak `fcitx5-configtool` ile grafik arayüzden de LinuxComplete’i ekleyebilirsiniz.

## Kullanım

| Tuş | Davranış |
|---|---|
| `Tab` | İlk veya seçili öneriyi kabul eder |
| `↑` / `↓` | Adaylar arasında gezinir |
| `Enter` | Seçili öneriyi kabul eder veya aktif buffer’ı tamamlar |
| `Space` | Kelimeyi commit eder ve sonraki kelime tahminini tetikler |
| `Esc` | Açık aday listesini kapatır |
| `:` | Emoji shortcode modunu başlatabilir |

## Testler

Projede şu anda temel regresyon testleri bulunuyor:

- İngilizce grammar sanity kontrolü
- Predictor learning regression testi
- Predictor quality regression testi
- AI reranker regression testi
- Data quality regression testi
- Rules validation testi

Testleri çalıştırmak için:

```bash
cd build
ctest --output-on-failure
```

## Rules Aracı

`data/rules/` altındaki typo ve grammar dosyalarını doğrulamak veya yeni kayıt eklemek için:

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

Araç şu kontrolleri yapar:

- eksik rule dosyalarını yakalar
- sekme ayracına göre alan sayısını doğrular
- skor alanlarının integer olduğunu kontrol eder
- birebir duplicate satırları hata olarak işaretler
- `normalize` ile dosyaları sıralar ve birebir duplicate satırları temizler
- `stats` ile dosya bazında satır sayılarını özetler
- `check-conflicts` ile aynı key üzerinde pozitif ve negatif skor çakışmalarını raporlar
- `resolve-conflicts` ile aynı key için en güçlü mutlak skoru koruyup çelişkili satırları temizler

## AI Reranker

AI desteği varsayılan olarak kapalıdır. Açıldığında mevcut predictor aday üretmeye devam eder; OpenAI sadece bu adayların sırasını bağlama göre yeniden düzenler. Ağ isteği başarısız olursa sistem otomatik olarak klasik sıralamaya döner.

Yeni AI katmanı şu korumaları içerir:

- `smart fallback`: sadece gerçekten belirsiz veya bağlama duyarlı adaylarda AI çağrılır
- `confidence gating`: klasik skor farkı çok açıksa yerel sıralama korunur
- `cache`: aynı bağlam ve aynı aday listesi için tekrar API çağrısı yapılmaz
- `debug logging`: karar ve fallback nedenleri terminale yazdırılabilir

Örnek kullanım:

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

Alternatif olarak `config/linuxcomplete.conf` içindeki `ai_rerank_enabled`, `ai_smart_fallback`, `ai_debug_logging`, `ai_model`, `ai_timeout_ms`, `ai_uncertainty_gap_threshold` ve `ai_max_cache_entries` alanları da kullanılabilir.

## Teknik Notlar

- C++17 kullanır
- Build sistemi CMake’tir
- Fcitx5 addon olarak yüklenir
- Sözlük araması trie ile yapılır
- Sıralama; sözlük frekansı, kullanıcı frekansı, n-gram ve grammar kurallarının birleşimiyle belirlenir
- AI katmanı yalnızca aday yeniden sıralaması yapar; yeni aday üretmez
- Yükleme katmanı, tekrar eden n-gram ve phrase girdilerini en güçlü skor korunacak şekilde birleştirir
- Typo ve grammar kuralları `data/rules/` altındaki dış veri dosyalarından yüklenir

## Yol Haritası

Gelecek geliştirmeler için [ROADMAP.md](ROADMAP.md) dosyasına bakabilirsiniz.

Öne çıkan alanlar:

- test kapsamını genişletmek
- predictor kurallarını daha modüler hale getirmek
- phrase ve n-gram veri setlerini geliştirmek
- uygulamaya özel davranış farklılıklarını azaltmak
- typo ve grammar veri kaynaklarını kod dışı, doğrulanabilir formatlara taşımak

## Lisans

Bu proje MIT lisansı ile yayınlanmaktadır. Ayrıntılar için [LICENSE](LICENSE) dosyasına bakın.
