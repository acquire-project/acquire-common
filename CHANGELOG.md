# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

### Changed

- Users can now specify chunk and shard dimensions in t, c, x, y, and x, as well as the append dimension.

## 0.2.0 - 2024-01-05

### Added

- Additional validation in `test-change-external-metadata`.

### Changed

- Simplified CMakeLists.txt in acquire-core-libs, acquire-video-runtime, and acquire-driver-common.
- Moved `write-side-by-side-tiff.cpp` to acquire-driver-common subproject.

### Removed

- PR template.
