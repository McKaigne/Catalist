#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"

namespace catalist::cli {

int runCompletion(const commandArgs& args, commandContext&) {
  if (args.empty()) {
    emitError("usage", "usage: catalist completion <bash|zsh|fish>", false);
    return 2;
  }

  std::string shell = args[0];
  std::transform(shell.begin(), shell.end(), shell.begin(), ::tolower);

  if (shell == "bash") {
    std::cout << R"bash(# Bash completion for catalist
_catalist_completions() {
  local cur prev subcommands
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  subcommands="ls show count random search add set rm stats export import config completion"

  if [ $COMP_CWORD -eq 1 ]; then
    COMPREPLY=( $(compgen -W "${subcommands} --version --help -v -h" -- "${cur}") )
    return 0
  fi

  case "${prev}" in
    --type|-t)
      COMPREPLY=( $(compgen -W "anime manga" -- "${cur}") )
      return 0
      ;;
    --status|-s)
      COMPREPLY=( $(compgen -W "watching reading completed on-hold dropped planned" -- "${cur}") )
      return 0
      ;;
    --sort)
      COMPREPLY=( $(compgen -W "date title status rating progress id" -- "${cur}") )
      return 0
      ;;
    --progress|-p)
      COMPREPLY=( $(compgen -W "add a sub s" -- "${cur}") )
      return 0
      ;;
    export)
      COMPREPLY=( $(compgen -W "json csv" -- "${cur}") )
      return 0
      ;;
    config)
      COMPREPLY=( $(compgen -W "list get set" -- "${cur}") )
      return 0
      ;;
    completion)
      COMPREPLY=( $(compgen -W "bash zsh fish" -- "${cur}") )
      return 0
      ;;
  esac

  COMPREPLY=( $(compgen -W "--type --status --rating --unrated --year --search --sort --reverse --limit --plain --ids --csv --json" -- "${cur}") )
}
complete -F _catalist_completions catalist
)bash";
    return 0;
  }

  if (shell == "zsh") {
    std::cout << R"zsh(#compdef catalist

_catalist() {
  local -a commands
  commands=(
    'ls:List watchlist entries'
    'show:Show detailed entry information'
    'count:Count matching records'
    'random:Pick random entry'
    'search:Search MyAnimeList online'
    'add:Add new record'
    'set:Update existing record'
    'rm:Remove entry'
    'stats:Display analytics telemetry'
    'export:Dump database to stdout'
    'import:Ingest MyAnimeList XML export'
    'config:Inspect or change preferences'
    'completion:Generate shell autocompletions'
  )

  _arguments -C \
    '1: :->command' \
    '*:: :->args'

  case $state in
    command)
      _describe -t commands 'catalist command' commands
      ;;
    args)
      case $words[1] in
        ls|count|random)
          _arguments \
            '(-t --type)'{-t,--type}'[Media type]:type:(anime manga)' \
            '(-s --status)'{-s,--status}'[Watch status]:status:(watching reading completed on-hold dropped planned)' \
            '(-r --rating)'{-r,--rating}'[Rating spec]:rating:' \
            '--unrated[Unrated entries only]' \
            '(-y --year)'{-y,--year}'[Filter year]:year:' \
            '(-q --search)'{-q,--search}'[Search title]:query:' \
            '--sort[Sort field]:sort:(date title status rating progress id)' \
            '(-R --reverse)'{-R,--reverse}'[Reverse sort]' \
            '(-l --limit)'{-l,--limit}'[Limit count]:limit:' \
            '--plain[Titles only]' \
            '--ids[IDs only]' \
            '--csv[Output CSV]' \
            '--json[Output JSON]'
          ;;
        set)
          _arguments \
            '(-p --progress)'{-p,--progress}'[Progress value or delta]:val:(add a sub s)' \
            '(-r --rating)'{-r,--rating}'[Rating score]:score:' \
            '(-s --status)'{-s,--status}'[Status]:status:(watching reading completed on-hold dropped planned)' \
            '--title[New title]:title:' \
            '-n[Notes]:notes:'
          ;;
        export)
          _arguments '1:format:(json csv)'
          ;;
        config)
          _arguments '1:action:(list get set)'
          ;;
        completion)
          _arguments '1:shell:(bash zsh fish)'
          ;;
      esac
      ;;
  esac
}

_catalist "$@"
)zsh";
    return 0;
  }

  if (shell == "fish") {
    std::cout << R"fish(# Fish completion for catalist
set -l commands ls show count random search add set rm stats export import config completion

complete -c catalist -f
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "ls" -d "List entries"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "show" -d "Show entry details"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "count" -d "Count entries"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "random" -d "Sample random entry"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "search" -d "Search MyAnimeList"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "add" -d "Add new record"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "set" -d "Update record"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "rm" -d "Remove entry"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "stats" -d "Analytics telemetry"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "export" -d "Dump database"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "config" -d "Modify configuration"
complete -c catalist -n "not __fish_seen_subcommand_from $commands" -a "completion" -d "Generate shell completion"

complete -c catalist -n "__fish_seen_subcommand_from ls count random" -l type -s t -r -a "anime manga"
complete -c catalist -n "__fish_seen_subcommand_from ls count random" -l status -s s -r -a "watching reading completed on-hold dropped planned"
complete -c catalist -n "__fish_seen_subcommand_from ls count random" -l sort -r -a "date title status rating progress id"
complete -c catalist -n "__fish_seen_subcommand_from export" -a "json csv"
complete -c catalist -n "__fish_seen_subcommand_from completion" -a "bash zsh fish"
complete -c catalist -n "__fish_seen_subcommand_from config" -a "list get set"
)fish";
    return 0;
  }

  emitError("usage",
            "unsupported shell '" + shell + "'. Supported: bash, zsh, fish.",
            false);
  return 2;
}

}  // namespace catalist::cli
