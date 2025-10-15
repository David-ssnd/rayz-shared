# RayZ Shared Library

Common code, headers, and libraries shared between Target and Weapon devices.

## Structure

- `include/` - Shared header files (protocols, data structures, constants)
- `lib/` - Shared library implementations
- `examples/` - Example usage

## Usage in Target/Weapon Projects

In your `platformio.ini`, add:

```ini
lib_extra_dirs = ../shared/lib
build_flags = -I../shared/include
```

## Shared Components

### Communication Protocol
- Message formats
- Packet structures
- Command definitions

### Common Libraries
- Network utilities
- Sensor abstractions
- Data serialization

## Versioning

This library follows semantic versioning. When you need a specific version:

```bash
cd esp32/shared
git checkout v1.2.0
```

Each Target/Weapon version can reference the shared library version it's compatible with.
