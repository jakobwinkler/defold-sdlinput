# SDLInput

Defold native extension wrapping SDL3 gamepad, joystick, and HID APIs. Gives raw access to gamepads beyond what Defold's built-in input system provides, especially rumble, LEDs, and sensors (gyro/accel).

Built on SDL3's gamepad layer. Tested on Linux, Windows, and WASM (web).

- Web does not support sensors due to browser issues (it's a mess). Make sure to interact with the page before querying devices.
- MacOS should be possible with some more work, but I have no way to test.

The project structure and API is a hot mess right now, but I guess it works.

## Usage

See example project in repository root.

## Dependencies

The native extension is built by shipping a prebuilt, stripped-down .a/.lib
file to extender (to avoid working around the environment setup in extender).
The files are in the repo for convenience and can be build using the provided
docker images in `ext-sdlinput/scripts`.

## License

MIT. See `LICENSE`.
