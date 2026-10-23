#!/usr/bin/env python3
"""Run small graphs with one worker, uneven splits, and empty partitions."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile


def run_benchmark(program, thread_count, vertex_count, work_dir):
    name = program.name
    degree = min(4, vertex_count - 1)
    if name in ("apsp", "bc", "bc_non_partitioned"):
        args = [thread_count, vertex_count, degree]
    elif name.startswith("community_"):
        args = [0, thread_count, 1, vertex_count, degree]
    elif name == "tsp":
        args = [thread_count, 6]
    else:
        args = [0, thread_count, vertex_count, degree]

    work_dir.mkdir()
    result = subprocess.run(
        [str(program), *map(str, args)], cwd=work_dir, text=True,
        capture_output=True, timeout=15,
    )
    assert result.returncode == 0, (program, args, result.stdout, result.stderr)

    if name in ("bfs", "bfs_atomic"):
        rows = (work_dir / "myfile.txt").read_text().splitlines()
        rows = [list(map(int, row.split())) for row in rows if row.strip()]
        expected_count = vertex_count - (name == "bfs_atomic")
        assert rows == [[i, 0] for i in range(expected_count)], (name, args, rows)

    if name.startswith("sssp"):
        return (work_dir / "myfile.txt").read_text()
    if name == "tsp":
        return re.search(r"Overall Best Tour:\s*Cost: (\d+)", result.stdout).group(1)
    if "tri" in name:
        return re.search(r"Triangles\s*=\s*(\d+)", result.stdout).group(1)
    return None


def main():
    build_dir = Path(sys.argv[1]).resolve()
    run_count = 0
    with tempfile.TemporaryDirectory(prefix="crono-test-") as directory:
        for name in sys.argv[2:]:
            expected = {}
            cases = [(1, 17), (3, 17), (4, 17), (1, 3), (4, 3)]
            if name == "tsp":
                cases = [(1, 6), (2, 6), (4, 6)]
            for thread_count, vertex_count in cases:
                work_dir = Path(directory) / f"{name}-{thread_count}-{vertex_count}"
                result = run_benchmark(build_dir / name, thread_count, vertex_count, work_dir)
                if thread_count == 1:
                    expected[vertex_count] = result
                else:
                    assert result == expected[vertex_count], (name, thread_count, vertex_count)
                run_count += 1
    print(f"Passed {run_count} benchmark runs.")


if __name__ == "__main__":
    main()
