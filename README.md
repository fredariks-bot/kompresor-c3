# Kompressor K3

Ein Kompressor-VST3-Plug-in im Stil von FabFilter Pro-C 3, gebaut mit JUCE 8 und C++17.

## Funktionen

- **Transfer-Kurve** mit Live-Position (Input-Level) und wählbarem Soft-Knee
- **Spektrum-Analyse** hinter der Kurve (Hann-FFT, 20 Hz – 20 kHz)
- **Gain-Reduction-Meter** mit Peak-Hold
- Threshold, Ratio (1:1 – 20:1), Attack (0,1 – 200 ms), Release (5 – 2000 ms)
- Knee (0 – 24 dB), Range (Maximal-Reduktion), Makeup (±24 dB), Mix (Parallel-Kompression)
- **Lookahead** (0 – 20 ms, latenzkompensiert)
- **Sidechain**: interner Detector oder externe Sidechain, 12 dB/oct Hochpass (20 Hz – 2 kHz)
- **Peak/RMS-Detector**, programmabhängiges **Auto Release**
- **8 Styles** als Preset-Makros: Clean, Vocal, Punch, Bus, Master, Opto, Pump, Limit

## Bauen

Voraussetzungen: CMake ≥ 3.22, C++17-Compiler (MSVC, Xcode/Clang oder GCC),
plus die üblichen Linux-Pakete (`libx11-dev libxext-dev libxinerama-dev libxrandr-dev
libxcursor-dev libfreetype-dev libfontconfig-dev libasound2-dev`) bzw. Xcode/Visual Studio.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8.0.8 wird automatisch per FetchContent geladen. Liegt JUCE bereits als
Geschwister-Verzeichnis `../juce` vor, wird dieses benutzt.

Das fertige Plug-in liegt danach unter:

- Linux: `build/KompressorK3_artefacts/Release/VST3/Kompressor K3.vst3`
- macOS: `.../VST3/Kompressor K3.vst3`
- Windows: `.../VST3/Kompressor K3.vst3`

Zum Installieren `COPY_PLUGIN_AFTER_BUILD=TRUE` beim Konfigurieren setzen:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCOPY_PLUGIN_AFTER_BUILD=TRUE
```

## Tests

Die DSP-Engine (`Source/DSP/CompressorCore.h`) ist reines C++ ohne JUCE-Abhängigkeit
und wird headless getestet:

```sh
./scripts/run_tests.sh
```

Geprüft werden u. a. Stationär-Verstärkungsreduktion (4:1, Hard/Soft-Knee), Range-Limit,
Makeup, Mix-Bypass, Attack-Ballistik, Lookahead-Latenz und externe Sidechain.

## Architektur

```
Source/
  DSP/CompressorCore.h   reine C++-Engine (Gain-Computer, Ballistik, Lookahead, SC-HPF)
  PluginProcessor.{h,cpp} Parameter, Audio-Bus (inkl. Sidechain), Metering, Spektrum-FIFO
  PluginEditor.h         Layout: Display, Style/Detector, Toggles, 10 Drehregler
  UI/TransferDisplay.h   Kurve + Spektrum + GR-Meter
  UI/CustomLookAndFeel.h Dark-Theme, Drehregler, Buttons
```

## Lizenz

Projektcode steht dir frei. JUCE 8 wird unter AGPLv3 bzw. kommerzieller JUCE-Lizenz
vertrieben — siehe `juce/LICENSE.md` nach dem ersten Build (FetchContent).
