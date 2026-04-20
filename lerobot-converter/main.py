import argparse
from pathlib import Path

from game2lerobot.converter import (
    convert_sessions,
    load_action_mapping_profile,
    load_game_definition,
)


def build_parser() -> argparse.ArgumentParser:
    """Create the batch converter CLI parser used by the console entrypoint."""

    parser = argparse.ArgumentParser(description="Convert recorded gameplay sessions to a LeRobotDataset.")
    parser.add_argument("--session-root", type=Path, required=True)
    parser.add_argument("--game-definition", type=Path, required=True)
    parser.add_argument("--action-mapping", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--repo-id", required=True)
    parser.add_argument("--task", required=True)
    parser.add_argument("--max-pre-action-seconds", type=float, required=True)
    parser.add_argument("--strict", action="store_true")
    return parser


def main(argv: list[str] | None = None):
    """Parse CLI arguments and run one batch conversion."""

    args = build_parser().parse_args(argv)
    return convert_sessions(
        session_root=args.session_root,
        game_definition=load_game_definition(args.game_definition),
        action_mapping=load_action_mapping_profile(args.action_mapping),
        output_root=args.output_root,
        repo_id=args.repo_id,
        task=args.task,
        max_pre_action_seconds=args.max_pre_action_seconds,
        strict=args.strict,
    )


if __name__ == "__main__":
    main()
