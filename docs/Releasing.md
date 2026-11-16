# Checklist for making a SlickShot release

Releases are tag-driven. Pushing a `v*` tag runs
`.github/workflows/Windows-pack.yml`, which builds the portable ZIP and the
installer, runs both smoke tests, uploads the build artifacts, creates the
GitHub release, and attaches the installer/portable files with their checksums.
There are no manual release-upload steps.

## Before the tag

- [ ] Confirm CI is green on `master` (`Building(CMake)` and
      `test-clang-format`).
- [ ] Confirm the update-check URL in `CMakeLists.txt` (`GIT_API_URL`) still
      points at `https://api.github.com/repos/khentmoba/slickshot/releases/latest`.
- [ ] Update `FLAMESHOT_VERSION` in `CMakeLists.txt` to the new version. The
      binary is still named `flameshot.exe` by design, but the product and the
      release artifacts are named SlickShot.
- [ ] Open a PR, review it, and merge it to `master`.

## Cutting the release

- [ ] Create and push an annotated tag on `master` whose name matches the
      version exactly: `v<FLAMESHOT_VERSION>` (for example, tag `v0.1.3` for
      `FLAMESHOT_VERSION 0.1.3`). The `Publish GitHub release` job verifies
      that the tag and `CMakeLists.txt` agree.
- [ ] Wait for the `Packaging(Windows)` workflow to finish. On non-tag pushes
      and pull requests it only builds and smoke-tests; only a `v*` tag uploads
      artifacts and publishes the release.

## After the workflow finishes

- [ ] Open the release at `https://github.com/khentmoba/slickshot/releases` and
      check the assets: the installer `.exe` and its `.sha256sum`, plus the
      portable `.zip` and its `.sha256sum`.
- [ ] Check that the generated release notes make sense; edit them in GitHub
      if needed.
- [ ] Spot-check that an installer download installs and runs on a clean
      Windows profile.

## Inherited Linux packaging

The files under `packaging/` and `data/appdata/` (for example
`data/appdata/org.flameshot.Flameshot.metainfo.xml`) come from Flameshot. They
are **not** part of the automated SlickShot release and are not published by
this workflow. Update them only if you also maintain a Linux package from this
fork.
