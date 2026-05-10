# SDLInput

Defold native extension for raw SDL input handling.

## Structure

```
defold-sdlinput/
├── game.project         # Project settings
├── ext-sdlinput/        # Native extension
│   ├── ext.manifest     # Extension manifest
│   ├── api/             # Script API definitions
│   │   └── sdlinput.script_api
│   ├── src/             # Source code (C/C++)
│   │   └── extension.cpp
│   └── include/         # Public headers
├── main/                # Game scripts and collections
│   ├── main.collection
│   └── controller.script
└── README.md
```

## Build

Open in Defold editor and build, or use the editor CLI.
