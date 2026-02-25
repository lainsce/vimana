# Cogito

> Get rational about GUI.

## Requirements

- C compiler (`cc`) with C11 support
- Meson
- Ninja
- SDL3
- SDL3_ttf
- freetype2
- SDL3_image (optional, used for themed icon file loading)
- (Optionally) Yis compiler [for extras folder apps]

Example (macOS/Homebrew):

```sh
brew install meson ninja sdl3 sdl3_ttf sdl3_image freetype
```

## SUM Theme Validator

Validate Cogito SUM themes (including `@bring` expansion with source-mapped diagnostics):

```sh
./yis/build/yis sum validate cogito/examples/sum_validator_fixtures/theme.sum
./yis/build/yis sum validate --mode off cogito/examples/sum_validator_fixtures/theme.sum
./yis/build/yis sum validate --mode strict cogito/examples/sum_validator_fixtures/theme.sum
./yis/build/yis sum validate cogito/examples/sum_validator_fixtures
```

Fixtures for validator behavior are in:

```txt
cogito/examples/sum_validator_fixtures/
```

## Build Cogito

Build the Cogito shared library in its own build directory:

```sh
meson setup cogito/build cogito
meson compile -C cogito/build
```

Then run GUI examples with Yis:

```sh
./yis/build/yis run cogito/examples/gui_hello.yi
./yis/build/yis run cogito/examples/gui_gallery.yi
```

If your program uses Cogito, include:

```yis
bring cogito
```

Install Cogito library and headers:

```sh
meson install -C cogito/build
```

- `YIS_COGITO_CFLAGS`: extra C flags for Cogito
- `YIS_COGITO_FLAGS`: extra linker flags for Cogito