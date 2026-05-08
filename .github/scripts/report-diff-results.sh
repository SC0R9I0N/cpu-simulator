#!/usr/bin/env bash
set -euo pipefail

mapfile -t diff_files < <(find diffs -maxdepth 1 -type f -name '*.diff' | sort)

total=${#diff_files[@]}
if (( total == 0 )); then
  echo "::error title=No diff results::No diff files were generated in diffs/."
  exit 1
fi

passed=0
failed_files=()

for diff_file in "${diff_files[@]}"; do
  if [[ ! -s "$diff_file" ]]; then
    passed=$((passed + 1))
  else
    failed_files+=("$diff_file")
  fi
done

failed=${#failed_files[@]}
required=$(( total / 2 + 1 ))
percent=$(awk -v passed="$passed" -v total="$total" 'BEGIN { printf "%.1f", (passed / total) * 100 }')

echo "::notice title=Diff summary::$passed/$total tests passed ($percent%). Minimum required: $required."

if (( failed > 0 )); then
  echo "::group::Failing diff files ($failed)"
  printf '%s\n' "${failed_files[@]}"
  echo "::endgroup::"
fi

{
  echo "## CI test summary"
  echo
  echo "| Metric | Value |"
  echo "| --- | ---: |"
  echo "| Passed tests | $passed |"
  echo "| Failed tests | $failed |"
  echo "| Total tests | $total |"
  echo "| Pass rate | $percent% |"
  echo "| Minimum required to pass | $required |"
  echo

  if (( failed == 0 )); then
    echo "All diff files are empty."
  else
    echo "### Non-empty diff files"
    echo
    for diff_file in "${failed_files[@]}"; do
      echo "- \`$diff_file\`"
    done
  fi
} >> "$GITHUB_STEP_SUMMARY"

if (( passed < required )); then
  echo "::error title=CI gate failed::Only $passed of $total tests passed. At least $required are required."
  exit 1
fi
