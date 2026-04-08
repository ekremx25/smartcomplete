#!/usr/bin/env python3
from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import sys


RULE_SPECS = {
    "en_typo_map.txt": 2,
    "en_grammar_pair_rules.txt": 3,
    "en_grammar_triple_rules.txt": 4,
    "tr_phrase_rules.txt": 3,
    "tr_context_rules.txt": 4,
}


@dataclass
class ValidationIssue:
    level: str
    message: str


@dataclass
class NormalizeResult:
    filename: str
    changed: bool
    duplicate_count: int
    rewritten_rows: int


@dataclass
class ConflictResolutionResult:
    filename: str
    changed: bool
    resolved_conflicts: int
    removed_rows: int
    remaining_rows: int


def read_rows(path: Path) -> list[list[str]]:
    rows: list[list[str]] = []
    for lineno, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        rows.append([str(lineno), *raw_line.split("\t")])
    return rows


def validate_file(path: Path, expected_fields: int) -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []
    rows = read_rows(path)
    seen: dict[tuple[str, ...], int] = {}

    for row in rows:
        lineno = row[0]
        fields = row[1:]
        if len(fields) != expected_fields:
            issues.append(
                ValidationIssue(
                    "error",
                    f"{path.name}:{lineno} expected {expected_fields} tab-separated fields, got {len(fields)}",
                )
            )
            continue

        if any(not field for field in fields):
            issues.append(
                ValidationIssue("error", f"{path.name}:{lineno} contains an empty field")
            )

        if expected_fields >= 3:
            try:
                int(fields[-1])
            except ValueError:
                issues.append(
                    ValidationIssue(
                        "error",
                        f"{path.name}:{lineno} score must be an integer, got {fields[-1]!r}",
                    )
                )

        key = tuple(fields)
        if key in seen:
            issues.append(
                ValidationIssue(
                    "error",
                    f"{path.name}:{lineno} duplicates line {seen[key]} exactly",
                )
            )
        else:
            seen[key] = int(lineno)

    return issues


def validate_rules_dir(rules_dir: Path) -> int:
    issues: list[ValidationIssue] = []

    if not rules_dir.exists():
        print(f"ERROR: rules directory not found: {rules_dir}")
        return 1

    for filename, expected_fields in RULE_SPECS.items():
        path = rules_dir / filename
        if not path.exists():
            issues.append(ValidationIssue("error", f"missing rules file: {filename}"))
            continue
        issues.extend(validate_file(path, expected_fields))

    for issue in issues:
        print(f"{issue.level.upper()}: {issue.message}")

    if any(issue.level == "error" for issue in issues):
        print(f"Validation failed with {sum(i.level == 'error' for i in issues)} error(s).")
        return 1

    print(f"Rules validation passed for {rules_dir}.")
    return 0


def sort_key(fields: list[str]) -> tuple[str, ...]:
    return tuple(field.casefold() for field in fields)


def normalize_file(path: Path, expected_fields: int, write: bool) -> NormalizeResult:
    rows = read_rows(path)
    valid_rows: list[list[str]] = []
    seen: set[tuple[str, ...]] = set()
    duplicate_count = 0

    for row in rows:
        fields = row[1:]
        if len(fields) != expected_fields:
            continue
        if any(not field for field in fields):
            continue
        if expected_fields >= 3:
            try:
                int(fields[-1])
            except ValueError:
                continue

        key = tuple(fields)
        if key in seen:
            duplicate_count += 1
            continue
        seen.add(key)
        valid_rows.append(fields)

    sorted_rows = sorted(valid_rows, key=sort_key)
    original_rows = [row[1:] for row in rows]
    changed = sorted_rows != original_rows

    if write and changed:
        path.write_text(
            "".join("\t".join(row) + "\n" for row in sorted_rows),
            encoding="utf-8",
        )

    return NormalizeResult(
        filename=path.name,
        changed=changed,
        duplicate_count=duplicate_count,
        rewritten_rows=len(sorted_rows),
    )


def normalize_rules_dir(rules_dir: Path, write: bool) -> int:
    if not rules_dir.exists():
        print(f"ERROR: rules directory not found: {rules_dir}")
        return 1

    changed_any = False
    for filename, expected_fields in RULE_SPECS.items():
        path = rules_dir / filename
        if not path.exists():
            print(f"ERROR: missing rules file: {filename}")
            return 1
        result = normalize_file(path, expected_fields, write=write)
        changed_any = changed_any or result.changed
        mode = "rewrote" if write and result.changed else "checked"
        print(
            f"{mode}: {result.filename} "
            f"(rows={result.rewritten_rows}, duplicates_removed={result.duplicate_count}, changed={result.changed})"
        )

    if not write and changed_any:
        print("Normalization preview found changes. Re-run with --write to apply them.")
    elif write:
        print(f"Normalization complete for {rules_dir}.")
    else:
        print(f"No normalization changes required for {rules_dir}.")

    return 0


def stats_rules_dir(rules_dir: Path) -> int:
    if not rules_dir.exists():
        print(f"ERROR: rules directory not found: {rules_dir}")
        return 1

    total_rows = 0
    for filename, expected_fields in RULE_SPECS.items():
        path = rules_dir / filename
        if not path.exists():
            print(f"ERROR: missing rules file: {filename}")
            return 1
        rows = read_rows(path)
        valid_rows = 0
        for row in rows:
            fields = row[1:]
            if len(fields) != expected_fields:
                continue
            if any(not field for field in fields):
                continue
            if expected_fields >= 3:
                try:
                    int(fields[-1])
                except ValueError:
                    continue
            valid_rows += 1
        total_rows += valid_rows
        print(f"{filename}: rows={valid_rows}")

    print(f"total_rows={total_rows}")
    return 0


def check_conflicts(rules_dir: Path) -> int:
    if not rules_dir.exists():
        print(f"ERROR: rules directory not found: {rules_dir}")
        return 1

    conflicts: list[str] = []

    for filename, expected_fields in RULE_SPECS.items():
        if expected_fields < 3:
            continue
        path = rules_dir / filename
        if not path.exists():
            print(f"ERROR: missing rules file: {filename}")
            return 1

        buckets: dict[tuple[str, ...], set[int]] = {}
        for row in read_rows(path):
            fields = row[1:]
            if len(fields) != expected_fields:
                continue
            try:
                score = int(fields[-1])
            except ValueError:
                continue
            key = tuple(fields[:-1])
            sign = 1 if score > 0 else -1 if score < 0 else 0
            buckets.setdefault(key, set()).add(sign)

        for key, signs in sorted(buckets.items(), key=lambda item: tuple(part.casefold() for part in item[0])):
            if 1 in signs and -1 in signs:
                conflicts.append(f"{filename}: {' | '.join(key)} has both positive and negative scores")

    if conflicts:
        for conflict in conflicts:
            print(conflict)
        print(f"Found {len(conflicts)} conflict(s).")
        return 1

    print(f"No score-sign conflicts found in {rules_dir}.")
    return 0


def resolve_conflicts_file(path: Path, expected_fields: int, write: bool) -> ConflictResolutionResult:
    rows = read_rows(path)
    grouped: dict[tuple[str, ...], list[tuple[int, list[str]]]] = {}

    for row in rows:
        fields = row[1:]
        if len(fields) != expected_fields:
            continue
        if expected_fields < 3:
            key = tuple(fields)
            grouped.setdefault(key, []).append((0, fields))
            continue
        try:
            score = int(fields[-1])
        except ValueError:
            continue
        key = tuple(fields[:-1])
        grouped.setdefault(key, []).append((score, fields))

    resolved_conflicts = 0
    removed_rows = 0
    kept_rows: list[list[str]] = []

    for _, items in grouped.items():
        if expected_fields < 3:
            kept_rows.append(items[0][1])
            removed_rows += max(0, len(items) - 1)
            continue

        best_score, best_fields = max(items, key=lambda item: (abs(item[0]), item[0]))
        signs = {1 if score > 0 else -1 if score < 0 else 0 for score, _ in items}
        if 1 in signs and -1 in signs:
            resolved_conflicts += 1
        removed_rows += max(0, len(items) - 1)
        _ = best_score
        kept_rows.append(best_fields)

    kept_rows.sort(key=sort_key)
    original_rows = [row[1:] for row in rows]
    changed = kept_rows != original_rows

    if write and changed:
        path.write_text(
            "".join("\t".join(row) + "\n" for row in kept_rows),
            encoding="utf-8",
        )

    return ConflictResolutionResult(
        filename=path.name,
        changed=changed,
        resolved_conflicts=resolved_conflicts,
        removed_rows=removed_rows,
        remaining_rows=len(kept_rows),
    )


def resolve_conflicts_dir(rules_dir: Path, write: bool) -> int:
    if not rules_dir.exists():
        print(f"ERROR: rules directory not found: {rules_dir}")
        return 1

    changed_any = False
    total_resolved = 0
    total_removed = 0

    for filename, expected_fields in RULE_SPECS.items():
        path = rules_dir / filename
        if not path.exists():
            print(f"ERROR: missing rules file: {filename}")
            return 1
        result = resolve_conflicts_file(path, expected_fields, write=write)
        changed_any = changed_any or result.changed
        total_resolved += result.resolved_conflicts
        total_removed += result.removed_rows
        mode = "rewrote" if write and result.changed else "checked"
        print(
            f"{mode}: {result.filename} "
            f"(rows={result.remaining_rows}, removed={result.removed_rows}, "
            f"resolved_conflicts={result.resolved_conflicts}, changed={result.changed})"
        )

    if not write and changed_any:
        print("Conflict resolution preview found changes. Re-run with --write to apply them.")
    elif write:
        print(
            f"Conflict resolution complete for {rules_dir}. "
            f"resolved_conflicts={total_resolved}, removed_rows={total_removed}"
        )
    else:
        print(f"No conflict resolution changes required for {rules_dir}.")

    return 0


def add_row(path: Path, fields: list[str]) -> int:
    expected_fields = RULE_SPECS[path.name]
    if len(fields) != expected_fields:
        print(
            f"ERROR: {path.name} expects {expected_fields} fields but received {len(fields)}.",
            file=sys.stderr,
        )
        return 1

    rows = [row[1:] for row in read_rows(path)] if path.exists() else []
    new_row = fields

    if new_row in rows:
        print(f"No change: identical row already exists in {path.name}.")
        return 0

    rows.append(new_row)
    rows.sort(key=sort_key)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "".join("\t".join(row) + "\n" for row in rows),
        encoding="utf-8",
    )
    print(f"Added row to {path.name}: {' | '.join(fields)}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="LinuxComplete rules tool")
    parser.add_argument(
        "--rules-dir",
        default=str(Path(__file__).resolve().parent.parent / "data" / "rules"),
        help="Directory that contains rule files",
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("validate", help="Validate all rule files")

    normalize = subparsers.add_parser(
        "normalize", help="Sort files and remove exact duplicates"
    )
    normalize.add_argument(
        "--write",
        action="store_true",
        help="Write normalized output back to disk",
    )

    subparsers.add_parser("stats", help="Show per-file row counts")
    subparsers.add_parser(
        "check-conflicts",
        help="Report pair/triple keys that have both positive and negative scores",
    )

    resolve = subparsers.add_parser(
        "resolve-conflicts",
        help="Keep the strongest score for conflicting pair/triple keys",
    )
    resolve.add_argument(
        "--write",
        action="store_true",
        help="Write resolved output back to disk",
    )

    add_typo = subparsers.add_parser("add-typo", help="Add a typo mapping")
    add_typo.add_argument("typo")
    add_typo.add_argument("correction")

    add_pair = subparsers.add_parser("add-pair", help="Add a pair rule")
    add_pair.add_argument("filename", choices=["en_grammar_pair_rules.txt", "tr_phrase_rules.txt"])
    add_pair.add_argument("prev")
    add_pair.add_argument("cand")
    add_pair.add_argument("score")

    add_triple = subparsers.add_parser("add-triple", help="Add a triple rule")
    add_triple.add_argument(
        "filename", choices=["en_grammar_triple_rules.txt", "tr_context_rules.txt"]
    )
    add_triple.add_argument("prev2")
    add_triple.add_argument("prev1")
    add_triple.add_argument("cand")
    add_triple.add_argument("score")

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    rules_dir = Path(args.rules_dir)

    if args.command == "validate":
        return validate_rules_dir(rules_dir)

    if args.command == "normalize":
        return normalize_rules_dir(rules_dir, write=args.write)

    if args.command == "stats":
        return stats_rules_dir(rules_dir)

    if args.command == "check-conflicts":
        return check_conflicts(rules_dir)

    if args.command == "resolve-conflicts":
        return resolve_conflicts_dir(rules_dir, write=args.write)

    if args.command == "add-typo":
        return add_row(rules_dir / "en_typo_map.txt", [args.typo, args.correction])

    if args.command == "add-pair":
        return add_row(rules_dir / args.filename, [args.prev, args.cand, args.score])

    if args.command == "add-triple":
        return add_row(
            rules_dir / args.filename,
            [args.prev2, args.prev1, args.cand, args.score],
        )

    parser.error("Unknown command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
