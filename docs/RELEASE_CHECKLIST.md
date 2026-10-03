# Release checklist

## Source and public contract

- [ ] `VERSION` contains the intended final version; candidate overrides are explicit.
- [ ] Public API/ABI changes and supported platforms are documented.
- [ ] Incompatible generated planner or bridge changes increment their ABI identifiers.
- [ ] Repository and SDK READMEs, release notes and examples match the packaged API.
- [ ] Copyright, license and third-party notices are current.
- [ ] No binaries, build trees or local settings are staged for commit.
- [ ] `git diff --check` passes and the release commit has a clean working tree.

## Windows

- [ ] Generate and rebuild the development solution in Debug, Profile and Release.
- [ ] Run the regression suite, allocation and hot reload pipeline self-tests.
- [ ] Perform the visual HTNDemo smoke test.
- [ ] Run `BuildAndValidateSDK.bat` for the final version.
- [ ] Confirm eight SDK variants, 24 external consumer executions and eight object/export checks.
- [ ] Verify incompatible CRT/unknown variant rejection and all Natvis project attachments.
- [ ] Record any external engine integration result and its platform/configuration scope.

## Linux

- [ ] Follow [Linux setup](LINUX.md), including native Premake and `PREMAKE5`.
- [ ] Build/test Debug, Release, Profile and ProfileDetailed with GCC; record Clang coverage separately.
- [ ] Verify language-server protocol, allocation, benchmarks and hot reload checks.
- [ ] Perform the visual demo smoke test and record the display/rendering environment.
- [ ] Keep HTNEditor excluded and state that limitation in the release notes.
- [ ] Run `bash BuildAndValidateSDK.sh` for the final version.
- [ ] Confirm four SDK variants with both GCC and Clang external consumers: 40 checks total.
- [ ] Confirm all six expected configuration rejections across the two compilers.
- [ ] Record the tested distro, architecture, compiler and standard-library ABI.

## Both packages

- [ ] Final archives identify the final version, not an `rc` or earlier candidate, in manifest/provenance.
- [ ] Each manifest records the correct platform, variants and ABI identifiers.
- [ ] `CHECKSUMS.sha256` covers every packaged file except itself.
- [ ] Headers, libraries, tools, symbols, Natvis, examples and current release documentation are present.
- [ ] Tests, demos, editors, build trees and repository-only files are absent.
- [ ] Validate from fresh extractions outside the source tree: Windows `ValidatePackage.cmd`; Linux `ValidatePackage.sh` with both compilers.
- [ ] Preserve the actual build/consumer logs; resolve all release-blocking failures.
- [ ] Finalize the release notes' validation section from those results.

## Publication

- [ ] Commit the reviewed source/docs and tag that exact public commit as `v<VERSION>`.
- [ ] Attach the Windows `.zip`, Linux `.tar.gz`, and their two `.sha256` sidecars.
- [ ] Download both published archives and compare their hashes against the validated local artifacts.
- [ ] Leave previously published artifacts unchanged.

Do not rename candidate archives into final releases: their internal versions and
build provenance would still identify the candidate. Build/package the final
version after the release source and documentation are ready.
