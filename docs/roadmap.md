# Project roadmap

This roadmap outlines planned enhancements to the Cpp20 Ultimate Project Template. Timelines are grouped by horizon to help contributors focus on the most impactful upgrades.

## Short-term (next 1–2 releases)
- **Build tooling polish**: finalize cross-platform shell script parity and document invocation examples for popular IDE integrations.
- **CI bootstrap**: add a GitHub Actions workflow that exercises the GCC and Clang build scripts on Linux and runs smoke tests on the sample executable.
- **Sample module curation**: refactor `cmakeapp1` into a minimal demonstration that highlights dependency injection, logging, and configuration patterns.
- **Documentation uplift**: link new docs (roadmap, API overview, changelog) from the README and include guidance for keeping them current.

## Mid-term (next quarter)
- **Preset expansion**: extend `CMakePresets.json` to cover MSVC multi-config builds, cross-compilation targets, and sanitiser builds.
- **Cache optimisation**: introduce ccache/sccache integration with automatic detection during configuration.
- **CI matrix coverage**: configure GitHub Actions to run the test matrix across Windows, Linux, and macOS, with artifact uploads for binaries.
- **Sample module gallery**: add optional modules demonstrating GUI integration (Qt/ImGui) and networking stacks with togglable build options.

## Long-term (6–12 months)
- **Template extensibility**: provide a plugin-style system for scaffolding new applications or libraries, with project generators powered by Python or CMake scripting.
- **Enterprise-ready CI/CD**: publish reference pipelines for Azure DevOps and GitLab CI that mirror the GitHub Actions workflows, including caching and deployment steps.
- **Ecosystem integrations**: support additional package managers (Conan, Hunter) and embed examples for cross-language bindings (Python, Rust, WASM).
- **Sustainability processes**: establish release cadences, backward compatibility policies, and automated changelog generation.

## Contributing to the roadmap
- Open a discussion or issue describing proposed roadmap changes and reference concrete use cases.
- When landing new features, update the relevant horizon with completed work or newly identified follow-up items.
- Periodically review horizons to ensure items remain actionable and reflect community priorities.
