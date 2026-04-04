# Vimana

> Get rational about GUI.

## Requirements

- C compiler (`cc`) with C11 support
- Meson
- Ninja
- SDL3
- Yis compiler [for extras folder apps]

Example (macOS/Homebrew):

```sh
brew install meson ninja sdl3
```

## Build Vimana

Build the Vimana shared library in its own build directory:

```sh
meson setup vimana/build vimana
meson compile -C vimana/build
```

If your Yis program uses Vimana, include:

```yis
bring vimana
```

Install Vimana library and headers:

```sh
meson install -C vimana/build
```