#!/usr/bin/env python3
"""
publish_update.py — Publish OTA update to fagramdesktop/ota repo.

Usage:
    python publish_update.py --version 6005002 --platform win64 --file path/to/tx64upd6005002
    python publish_update.py --version 6005002 --platform win64 --file path/to/tx64upd6005002 --beta

This script:
  1. Copies the update binary (created by Packer) to the local ota repo.
  2. Updates the current4 manifest JSON.
  3. Commits and pushes to GitHub.

Prerequisites:
  - Clone https://github.com/fagramdesktop/ota next to this repo.
  - Git must be configured with push access.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, '..', '..'))
UPDATES_REPO = os.path.normpath(os.path.join(REPO_ROOT, '..', 'ota'))
MANIFEST_NAME = 'current4'

PLATFORMS = ['win64', 'linux']

UPDATE_FILE_PREFIX = {
    'win64': 'tx64upd',
    'linux': 'tlinuxupd',
}


def read_manifest(path):
    if os.path.isfile(path):
        with open(path, 'r') as f:
            return json.load(f)
    return {}


def write_manifest(path, data):
    with open(path, 'w') as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write('\n')


def git_run(cwd, *args):
    result = subprocess.run(
        ['git'] + list(args),
        cwd=cwd,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f'git error: {result.stderr.strip()}', file=sys.stderr)
        sys.exit(1)
    return result.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description='Publish OTA update')
    parser.add_argument('--version', required=True, type=int,
                        help='Integer version (e.g. 6005002)')
    parser.add_argument('--platform', required=True, choices=PLATFORMS,
                        help='Target platform key')
    parser.add_argument('--file', required=True,
                        help='Path to the update file produced by Packer')
    parser.add_argument('--beta', action='store_true',
                        help='Mark as beta channel update')
    parser.add_argument('--updates-repo', default=UPDATES_REPO,
                        help='Path to local ota repo clone')
    parser.add_argument('--no-push', action='store_true',
                        help='Commit but do not push')
    args = parser.parse_args()

    updates_repo = os.path.normpath(args.updates_repo)
    if not os.path.isdir(os.path.join(updates_repo, '.git')):
        print(f'Error: {updates_repo} is not a git repository.', file=sys.stderr)
        print(f'Clone it first: git clone https://github.com/fagramdesktop/ota "{updates_repo}"')
        sys.exit(1)

    if not os.path.isfile(args.file):
        print(f'Error: Update file not found: {args.file}', file=sys.stderr)
        sys.exit(1)

    prefix = UPDATE_FILE_PREFIX.get(args.platform)
    expected_name = f'{prefix}{args.version}'
    actual_name = os.path.basename(args.file)

    if actual_name != expected_name:
        print(f'Warning: Expected filename "{expected_name}", got "{actual_name}"')
        print(f'The file will be renamed to "{expected_name}" in the updates repo.')

    git_run(updates_repo, 'pull', '--rebase', 'origin', 'main')

    dest_file = os.path.join(updates_repo, expected_name)
    shutil.copy2(args.file, dest_file)
    print(f'Copied update file to: {dest_file}')

    manifest_path = os.path.join(updates_repo, MANIFEST_NAME)
    manifest = read_manifest(manifest_path)

    channel = 'beta' if args.beta else 'stable'
    link = f'/{expected_name}'

    if args.platform not in manifest:
        manifest[args.platform] = {}

    manifest[args.platform][channel] = {
        'released': str(args.version),
        'link': link,
    }

    write_manifest(manifest_path, manifest)
    print(f'Updated manifest: {args.platform}/{channel} -> v{args.version}')

    git_run(updates_repo, 'add', expected_name, MANIFEST_NAME)

    version_str = format_version(args.version)
    channel_suffix = ' beta' if args.beta else ''
    commit_msg = f'Update {args.platform} to {version_str}{channel_suffix}'
    git_run(updates_repo, 'commit', '-m', commit_msg)
    print(f'Committed: {commit_msg}')

    if not args.no_push:
        git_run(updates_repo, 'push', 'origin', 'main')
        print('Pushed to origin/main')
    else:
        print('Skipped push (--no-push)')

    print('\nDone! Update published successfully.')
    print(f'URL: https://raw.githubusercontent.com/fagramdesktop/ota/main/{expected_name}')


def format_version(version_int):
    major = version_int // 1000000
    minor = (version_int // 1000) % 1000
    patch = version_int % 1000
    return f'{major}.{minor}.{patch}'


if __name__ == '__main__':
    main()
