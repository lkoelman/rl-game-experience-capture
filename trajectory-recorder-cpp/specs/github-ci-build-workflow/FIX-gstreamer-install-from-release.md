  # Fix GStreamer CI Dependency Download

  ## Summary

  Replace direct CI downloads from gstreamer.freedesktop.org with a stable mirrored dependency source controlled by the repository, because the upstream site is blocking GitHub-hosted runner requests
  with a bot challenge. Use GitHub Release assets for the GStreamer runtime/development MSIs plus checksum verification, then keep optional caching only as a speed optimization.

  ## Recommendation

  Use a dedicated GitHub Release such as ci-deps-gstreamer-1.26.9 containing:

  - gstreamer-1.0-msvc-x86_64-1.26.9.msi


  ## Alternatives Considered

    reuse for files that do not change often, not as artifact distribution.
  - GitHub Actions artifacts: Not recommended for installer storage. GitHub docs distinguish artifacts as outputs from completed workflow runs, while dependencies should come from caches or stable
    package/binary sources.
  - Self-hosted runner: Reliable if this project grows, but adds machine maintenance and security responsibility.
  - Cloud mirror such as S3/Azure Blob: Viable, but adds external infrastructure. GitHub Release assets are simpler for this repo.

  - Update the monorepo-root .github/workflows/windows-build.yml.
  - Replace the Install GStreamer Invoke-WebRequest URLs with GitHub Release asset URLs under https://github.com/lkoelman/rl-game-experience-capture/releases/download/ci-deps-gstreamer-1.26.9/....
  - Download SHA256SUMS.txt and verify both MSI hashes before installation.
  - Keep GSTREAMER_VERSION: 1.26.9 and GSTREAMER_ROOT: C:\gstreamer\1.0\msvc_x86_64.
  - Add working-directory: trajectory-recorder-cpp to build-related steps or set defaults.run.working-directory: trajectory-recorder-cpp where appropriate, because scripts/build.ps1 runs conan
    install . and must execute from the component directory.

  ## Release Asset Preparation

  - Manually download the official GStreamer MSVC x64 runtime and development installers once from a trusted non-CI environment.
  - Compute SHA256 hashes locally:

    Get-FileHash .\gstreamer-1.0-msvc-x86_64-1.26.9.msi -Algorithm SHA256
    Get-FileHash .\gstreamer-1.0-devel-msvc-x86_64-1.26.9.msi -Algorithm SHA256
  - Create a GitHub Release tagged ci-deps-gstreamer-1.26.9.
  - Upload both MSI files and SHA256SUMS.txt.
  - Document in README.md that CI uses mirrored GStreamer installers from a pinned GitHub Release, while local developers can still install GStreamer normally.

  ## Test Plan

  - Validate the workflow YAML by inspection or a GitHub Actions linter.
  - Run workflow_dispatch on the updated workflow.
  - Confirm the GStreamer step downloads from GitHub Release assets, verifies checksums, installs both MSIs, and finds C:\gstreamer\1.0\msvc_x86_64.
  - Confirm scripts/build.ps1 runs from trajectory-recorder-cpp, not the monorepo root.
  - Confirm the artifact still contains only:
      - record_session.exe
      - validate_recording.exe
      - map_actions.exe
  - GStreamer remains pinned at 1.26.9 until intentionally updated.
  - Standard GitHub-hosted windows-2022 runners remain the target; no custom runner image is introduced for this fix.
  - Sources consulted: GitHub dependency caching docs, GitHub artifact docs, and GitHub custom image docs.