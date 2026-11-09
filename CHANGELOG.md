# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.2.0] - 2026-11-04

### Added
- Terminal UI with Anime, Manga, Overview, Sync, Settings and About tabs,
  floating detail card, command palette, visual multi-selection and undo/redo.
- CLI query commands: `ls`, `count`, `show`, `random`, `stats`, with table,
  `--plain`, `--ids`, `--csv` and `--json` output.
- CLI mutation commands: `add`, `set` (absolute and stepped progress), `rm`.
- `export` (JSON, CSV), MyAnimeList XML `import`, and online `search`.
- Provider registry with MyAnimeList, AniList and Kitsu.
- MyAnimeList OAuth login and metadata sync.
- KDL configuration (`catalist.kdl`), themes, keybinds and shell completions.
- Atomic CSV storage with automatic backup snapshots.
