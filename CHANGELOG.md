# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

### Added

- Users can specify access key ID and secret access key for S3 storage in `StorageProperties`.

### Fixed

- A bug where changing device identifiers for the storage device was not being handled correctly.
- A race condition where `camera_get_frame()` might be called after `camera_stop()` when aborting acquisition.

### Changed

- `reserve_image_shape` is now called in `acquire_configure` rather than `acquire_start`.
- Users can now specify the names, ordering, and number of acquisition dimensions.
- The `StorageProperties::filename` field is now `StorageProperties::uri`.
- Files can be specified by URI with an optional `file://` prefix.

## 0.2.0 - 2024-01-05

### Added

- Additional validation in `test-change-external-metadata`.

### Changed

- Simplified CMakeLists.txt in acquire-core-libs, acquire-video-runtime, and acquire-driver-common.
- Moved `write-side-by-side-tiff.cpp` to acquire-driver-common subproject.

### Removed

- PR template.
