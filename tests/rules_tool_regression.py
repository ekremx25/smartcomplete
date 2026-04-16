#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "scripts" / "rules_tool.py"


def write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        check=False,
        text=True,
        capture_output=True,
    )


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="linuxcomplete_rules_tool_") as tmp:
        rules_dir = Path(tmp)

        write(rules_dir / "en_typo_map.txt", "wont\twon't\ndont\tdon't\ndont\tdon't\n")
        write(
            rules_dir / "en_grammar_pair_rules.txt",
            "zeta\tbeta\t5\nalpha\tbeta\t10\nzeta\tbeta\t5\n",
        )
        write(
            rules_dir / "en_grammar_triple_rules.txt",
            "i\tam\tready\t180\ni\tam\tready\t180\n",
        )
        stats = run("--rules-dir", str(rules_dir), "stats")
        if stats.returncode != 0 or "total_rows=8" not in stats.stdout:
            print(stats.stdout, end="")
            print(stats.stderr, end="", file=sys.stderr)
            return 1

        conflict_dir = rules_dir / "conflicts"
        conflict_dir.mkdir()
        write(conflict_dir / "en_typo_map.txt", "")
        write(conflict_dir / "en_grammar_pair_rules.txt", "love\tyou\t200\nlove\tyou\t-50\n")
        write(conflict_dir / "en_grammar_triple_rules.txt", "")
        conflicts = run("--rules-dir", str(conflict_dir), "check-conflicts")
        if conflicts.returncode == 0 or "love | you has both positive and negative scores" not in conflicts.stdout:
            print(conflicts.stdout, end="")
            print(conflicts.stderr, end="", file=sys.stderr)
            return 1
        resolve_preview = run("--rules-dir", str(conflict_dir), "resolve-conflicts")
        if resolve_preview.returncode != 0 or "resolved_conflicts=1" not in resolve_preview.stdout:
            print(resolve_preview.stdout, end="")
            print(resolve_preview.stderr, end="", file=sys.stderr)
            return 1
        resolve_write = run("--rules-dir", str(conflict_dir), "resolve-conflicts", "--write")
        if resolve_write.returncode != 0:
            print(resolve_write.stdout, end="")
            print(resolve_write.stderr, end="", file=sys.stderr)
            return 1
        post_conflicts = run("--rules-dir", str(conflict_dir), "check-conflicts")
        if post_conflicts.returncode != 0:
            print(post_conflicts.stdout, end="")
            print(post_conflicts.stderr, end="", file=sys.stderr)
            return 1

        preview = run("--rules-dir", str(rules_dir), "normalize")
        if preview.returncode != 0:
            print(preview.stdout, end="")
            print(preview.stderr, end="", file=sys.stderr)
            return 1
        if "changed=True" not in preview.stdout:
            print("Expected normalize preview to detect changes.", file=sys.stderr)
            print(preview.stdout, end="")
            return 1

        apply = run("--rules-dir", str(rules_dir), "normalize", "--write")
        if apply.returncode != 0:
            print(apply.stdout, end="")
            print(apply.stderr, end="", file=sys.stderr)
            return 1

        validate = run("--rules-dir", str(rules_dir), "validate")
        if validate.returncode != 0:
            print(validate.stdout, end="")
            print(validate.stderr, end="", file=sys.stderr)
            return 1

        pair_rows = (rules_dir / "en_grammar_pair_rules.txt").read_text(encoding="utf-8").splitlines()
        typo_rows = (rules_dir / "en_typo_map.txt").read_text(encoding="utf-8").splitlines()
        triple_rows = (rules_dir / "en_grammar_triple_rules.txt").read_text(encoding="utf-8").splitlines()

        if pair_rows != ["alpha\tbeta\t10", "zeta\tbeta\t5"]:
            print("Pair rule normalization did not sort/dedupe as expected.", file=sys.stderr)
            return 1
        if typo_rows != ["dont\tdon't", "wont\twon't"]:
            print("Typo normalization did not sort/dedupe as expected.", file=sys.stderr)
            return 1
        if triple_rows != ["i\tam\tready\t180"]:
            print("Triple rule normalization did not dedupe as expected.", file=sys.stderr)
            return 1

    print("Rules tool regression checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
