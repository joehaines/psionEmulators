#!/usr/bin/env bash
# Publish this repository's tracked content to the public mirror
# (github.com/joehaines/psionEmulators) as a single commit on main.
#
# The mirror is a snapshot, not a fork of this history: each run replaces
# its whole tree with the tracked files of one commit here and commits
# the difference. Private history, branch names and untracked working
# files never cross over — only what `git archive` emits for the chosen
# ref, minus anything listed in EXCLUDE below.
#
#   bash scripts/sync-public.sh                  # publish HEAD
#   bash scripts/sync-public.sh --dry-run        # show the diff, push nothing
#   bash scripts/sync-public.sh -m "Netpad MMC support"
#   bash scripts/sync-public.sh --ref v1.2       # publish some other commit
#
# The mirror clone is kept in .public-sync/ (gitignored) and reused, so
# only the first run pays for the clone. Pushing needs credentials for
# the public repo: whatever git already uses for github.com, or set
# PUBLIC_REPO_URL to an SSH remote.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIRROR="${PUBLIC_SYNC_DIR:-$REPO/.public-sync}"
PUBLIC_URL="${PUBLIC_REPO_URL:-https://github.com/joehaines/psionEmulators.git}"
BRANCH=main

# Tracked paths deleted from the mirror after extraction, so they are
# never published (and are removed there if a past sync published them).
# Repo-root-relative; directories are fine.
#
# Everything else development-only — reference/, .env, the local API
# config — is untracked here, so it can never reach the archive at all.
EXCLUDE=(
  # Runs *this* script; belongs to the private repo only.
  .github/workflows/sync-public.yml
)

ref=HEAD
message=""
dry_run=0
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) dry_run=1; shift ;;
    --ref)     ref="$2"; shift 2 ;;
    -m|--message) message="$2"; shift 2 ;;
    -h|--help) awk 'NR>1 && /^#/ { sub(/^# ?/, ""); print; next } NR>1 { exit }' \
                 "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

cd "$REPO"

if ! git rev-parse --verify --quiet "$ref^{commit}" >/dev/null; then
  echo "not a commit: $ref" >&2
  exit 2
fi
src_sha="$(git rev-parse --short "$ref")"

# Only committed content is published, so an uncommitted change is
# almost always a mistake about to be missed rather than an intent.
if [ "$ref" = HEAD ] && [ -n "$(git status --porcelain)" ]; then
  echo "warning: working tree has uncommitted changes; publishing $src_sha as committed" >&2
fi

# ── Mirror clone ────────────────────────────────────────────────────
if [ -d "$MIRROR/.git" ]; then
  echo "==> fetching $BRANCH into $MIRROR"
  git -C "$MIRROR" remote set-url origin "$PUBLIC_URL"
  git -C "$MIRROR" fetch --quiet origin "$BRANCH"
  # --force because a previous --dry-run leaves the mirror staged, and
  # the tree is about to be replaced wholesale regardless.
  git -C "$MIRROR" checkout --quiet --force -B "$BRANCH" "origin/$BRANCH"
else
  echo "==> cloning $PUBLIC_URL into $MIRROR (first run — this takes a few minutes)"
  rm -rf "$MIRROR"
  git clone --quiet --branch "$BRANCH" "$PUBLIC_URL" "$MIRROR"
fi

# ── Replace the mirror's tree with this repo's tracked content ──────
# Wipe first so files deleted here are deleted there too; .git is the
# only thing that survives.
echo "==> staging $src_sha"
find "$MIRROR" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
git archive --format=tar "$ref" | tar -x -C "$MIRROR"

for path in ${EXCLUDE+"${EXCLUDE[@]}"}; do
  rm -rf "${MIRROR:?}/$path"
done

# ── Diff ────────────────────────────────────────────────────────────
git -C "$MIRROR" add -A
if git -C "$MIRROR" diff --cached --quiet; then
  echo "==> public mirror already matches $src_sha — nothing to publish"
  exit 0
fi

echo
git -C "$MIRROR" diff --cached --stat | tail -25
echo

added=$(git -C "$MIRROR" diff --cached --name-only --diff-filter=A | wc -l | tr -d ' ')
modified=$(git -C "$MIRROR" diff --cached --name-only --diff-filter=M | wc -l | tr -d ' ')
deleted=$(git -C "$MIRROR" diff --cached --name-only --diff-filter=D | wc -l | tr -d ' ')

if [ "$dry_run" = 1 ]; then
  echo "==> dry run: $added added, $modified modified, $deleted deleted — nothing pushed"
  echo "    (the mirror in $MIRROR is left staged; the next run resets it)"
  exit 0
fi

# ── Commit and push ─────────────────────────────────────────────────
if [ -z "$message" ]; then
  message="Update from source ($(date -u +%Y-%m-%d))"
fi

# A CI runner has no git identity of its own, and an empty one makes
# `git commit` fail outright — fall back to a name for the mirror.
author_name="${GIT_AUTHOR_NAME:-$(git config user.name || true)}"
author_email="${GIT_AUTHOR_EMAIL:-$(git config user.email || true)}"
: "${author_name:=psion-mirror-sync}"
: "${author_email:=psion-mirror-sync@users.noreply.github.com}"

git -C "$MIRROR" -c user.name="$author_name" -c user.email="$author_email" \
  commit --quiet -m "$message" -m "$added added, $modified modified, $deleted deleted."

echo "==> pushing to $BRANCH"
for attempt in 1 2 3 4 5; do
  if out="$(git -C "$MIRROR" push origin "$BRANCH" 2>&1)"; then
    printf '%s\n' "$out"
    echo "==> published $src_sha: $added added, $modified modified, $deleted deleted"
    exit 0
  fi
  printf '%s\n' "$out" >&2

  # Only a flaky connection is worth another go. A refusal is a decision
  # the far end already made — retrying just repeats it more slowly.
  if printf '%s' "$out" | grep -qE '\[remote rejected\]|\[rejected\]|403|denied|protected branch'; then
    if printf '%s' "$out" | grep -q 'workflow'; then
      echo >&2
      echo "The token may not write .github/workflows/. On a fine-grained PAT that" >&2
      echo "is the 'Workflows' permission (Read and write), separate from Contents;" >&2
      echo "on a classic one it is the 'workflow' scope. Either grant it, or add" >&2
      echo ".github/workflows/ to EXCLUDE in this script to keep workflows out of" >&2
      echo "the mirror — note that removing ones already published is itself a" >&2
      echo "workflow-file change, so the first such sync still needs the permission." >&2
    fi
    echo >&2
    echo "push was rejected, not retrying; the commit is waiting in $MIRROR" >&2
    exit 1
  fi

  if [ "$attempt" = 5 ]; then break; fi
  delay=$((2 ** attempt))
  echo "push failed, retrying in ${delay}s" >&2
  sleep "$delay"
done

echo "push failed after 5 attempts; the commit is waiting in $MIRROR" >&2
exit 1
