import argparse
import logging
from pathlib import Path

from .parsing import load_action_mapping_profile, load_game_definition
from .pipeline import convert_sessions


LOG_LEVELS = {
    "debug": logging.DEBUG,
    "info": logging.INFO,
    "warning": logging.WARNING,
    "error": logging.ERROR,
    "critical": logging.CRITICAL,
}


def build_parser() -> argparse.ArgumentParser:
    """Create the batch converter CLI parser used by the console entrypoint."""

    parser = argparse.ArgumentParser(
        description="Convert recorded gameplay sessions to a LeRobotDataset."
    )
    parser.add_argument("--session-root", type=Path, required=True)
    parser.add_argument("--game-definition", type=Path, required=True)
    parser.add_argument("--action-mapping", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--repo-id", required=True)
    parser.add_argument("--task", required=True)
    parser.add_argument("--max-pre-action-seconds", type=float)
    parser.add_argument(
        "--no-reencode",
        action="store_true",
        help="Write source MP4 video into the LeRobot dataset without re-encoding.",
    )
    parser.add_argument("--strict", action="store_true")
    parser.add_argument(
        "--verbosity",
        choices=tuple(LOG_LEVELS),
        default="info",
        help="Logging verbosity level.",
    )
    return parser


def main(argv: list[str] | None = None):
    """Parse CLI arguments and run one batch conversion."""

    args = build_parser().parse_args(argv)
    logging.basicConfig(
        level=LOG_LEVELS[args.verbosity],
        format="%(levelname)s:%(name)s:%(message)s",
    )
    return convert_sessions(
        session_root=args.session_root,
        game_definition=load_game_definition(args.game_definition),
        action_mapping=load_action_mapping_profile(args.action_mapping),
        output_root=args.output_root,
        repo_id=args.repo_id,
        task=args.task,
        max_pre_action_seconds=args.max_pre_action_seconds,
        no_reencode=args.no_reencode,
        strict=args.strict,
    )
