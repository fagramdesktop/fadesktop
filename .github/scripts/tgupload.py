# -*- coding: utf-8 -*-
# @author: Burhanverse

import argparse
import asyncio
import html
import re
from os import environ

from pyrogram import Client, enums, types
from pyrogram.parser.html import HTML
from pyrogram.types import InputMediaDocument

API_HASH = environ.get("API_HASH") or environ.get("API_HASH_CONFIG")
API_ID = int(environ.get("API_ID") or environ.get("API_ID_CONFIG") or 0)
BOT_TOKEN = environ.get("BOT_TOKEN") or environ.get("TELEGRAM_BOT_TOKEN")
MAX_CAPTION_LENGTH = 1024


def format_changelog(text: str, repo: str = None) -> str:
    escaped = html.escape(text.strip())
    # Convert markdown links [text](url) to HTML <a href="url">text</a>
    escaped = re.sub(r"\[([^\]]+)\]\((https?://[^\)]+)\)", r"""<a href="\2">\1</a>""", escaped)
    if repo:
        escaped = re.sub(r"(?<![\w/])#([0-9]+)\b", rf"""<a href="https://github.com/{repo}/pull/\1">#\1</a>""", escaped)
    return escaped


def resolve_caption(args) -> str:
    if getattr(args, "caption", None):
        return args.caption
    if getattr(args, "caption_file", None):
        with open(args.caption_file, "r", encoding="utf-8") as f:
            return f.read()

    changelog = getattr(args, "changelog", None)
    if not changelog and getattr(args, "changelog_file", None):
        with open(args.changelog_file, "r", encoding="utf-8") as f:
            changelog = f.read()
    if not changelog and "CHANGELOG" in environ:
        changelog = environ["CHANGELOG"]

    title = getattr(args, "title", None) or environ.get("TITLE")
    tgd_version = getattr(args, "tgd_version", None) or environ.get("TGD_VERSION")
    build_type = getattr(args, "build_type", None) or environ.get("BUILD_TYPE")
    branch = getattr(args, "branch", None) or environ.get("BRANCH")
    checksum = getattr(args, "checksum", None) or environ.get("CHECKSUM")
    repo = getattr(args, "repo", None) or environ.get("GITHUB_REPOSITORY")

    lines = []
    if title:
        lines.append(f"<b>{html.escape(title)}</b>")
    if tgd_version:
        lines.append(f"TGD Base: <code>{html.escape(tgd_version)}</code>")
    if build_type:
        lines.append(f"Build Type: <code>{html.escape(build_type)}</code>")
    if branch:
        lines.append(f"Branch: <code>{html.escape(branch)}</code>")
    if checksum:
        lines.append(f"SHA256: <code>{html.escape(checksum)}</code>")
    if changelog:
        lines.append("")
        escaped_changelog = format_changelog(changelog, repo=repo)
        lines.append(f"<blockquote expandable>{escaped_changelog}</blockquote>")

    return "\n".join(lines)


async def prepare_caption(app: Client, caption: str) -> str:
    if not caption:
        return caption

    parsed = await app.parser.parse(caption, enums.ParseMode.HTML)
    text = parsed["message"]
    raw_entities = parsed["entities"] or []

    if len(text) <= MAX_CAPTION_LENGTH:
        return caption

    type_entities = [await types.MessageEntity._parse(app, e, {}) for e in raw_entities]
    text = text[:MAX_CAPTION_LENGTH - 3] + "..."
    truncated_entities = []
    for e in type_entities:
        if e.offset >= MAX_CAPTION_LENGTH:
            continue
        if e.offset + e.length > MAX_CAPTION_LENGTH:
            e.length = MAX_CAPTION_LENGTH - e.offset
        truncated_entities.append(e)

    return HTML.unparse(text, truncated_entities)


async def upload_single(app, file, chat_id_list, caption, message_thread_id=None):
    caption = await prepare_caption(app, caption)
    for chat_id in chat_id_list:
        await app.send_document(
            chat_id=chat_id,
            document=file,
            caption=caption,
            parse_mode=enums.ParseMode.HTML,
            message_thread_id=message_thread_id,
        )
    print("Upload Successful!")


async def upload_group(app, files, chat_id_list, caption, message_thread_id=None):
    media = []
    caption = await prepare_caption(app, caption)
    for i, file in enumerate(files):
        media.append(InputMediaDocument(
            media=file,
            caption=caption if i == 0 else "",
            parse_mode=enums.ParseMode.HTML if i == 0 else None,
        ))
    for chat_id in chat_id_list:
        await app.send_media_group(
            chat_id=chat_id,
            media=media,
            message_thread_id=message_thread_id,
        )
    print("Group Upload Successful!")


async def main():
    parser = argparse.ArgumentParser(prog="Uploader", description="Telegram file uploader")
    parser.add_argument("file", type=str, nargs="?", help="File to upload (single file mode)")
    parser.add_argument("--files", type=str, nargs="+", help="Files to upload as a grouped post")
    parser.add_argument("--chat-id", type=int, help="Chat ID(s) to upload the file to", nargs="+")
    parser.add_argument("--topic-id", "--message-thread-id", type=int, dest="message_thread_id", help="Forum topic/thread ID")
    parser.add_argument("--caption", type=str, help="Caption for the file")
    parser.add_argument("--caption-file", type=str, help="File containing caption")
    parser.add_argument("--title", type=str, help="Title for the post")
    parser.add_argument("--tgd-version", type=str, help="TGD Base version")
    parser.add_argument("--build-type", type=str, help="Build type (ci or rel)")
    parser.add_argument("--branch", type=str, help="Git branch name")
    parser.add_argument("--checksum", type=str, help="SHA256 Checksum")
    parser.add_argument("--repo", type=str, help="GitHub repository (owner/repo)")
    parser.add_argument("--changelog", type=str, help="Changelog text")
    parser.add_argument("--changelog-file", type=str, help="File containing changelog text")
    args = parser.parse_args()

    if not args.files and not args.file:
        parser.error("Either 'file' or '--files' must be provided")

    chat_id_list = args.chat_id
    if not chat_id_list:
        env_chat = environ.get("CHAT_ID") or environ.get("TELEGRAM_CHAT_ID")
        if env_chat:
            chat_id_list = [int(x.strip()) for x in env_chat.split() if x.strip()]
    if not chat_id_list:
        parser.error("Chat ID must be provided via --chat-id or CHAT_ID / TELEGRAM_CHAT_ID env var")

    message_thread_id = args.message_thread_id
    if message_thread_id is None:
        env_thread = environ.get("TOPIC_ID") or environ.get("MESSAGE_THREAD_ID") or environ.get("TELEGRAM_THREAD_ID")
        if env_thread:
            message_thread_id = int(env_thread)

    caption = resolve_caption(args)
    if not caption:
        parser.error("A caption, changelog, or structured fields (--title, --caption, etc.) must be provided")

    app = Client("Uploader", api_id=API_ID, api_hash=API_HASH, bot_token=BOT_TOKEN)
    async with app:
        if args.files:
            await upload_group(app, args.files, chat_id_list, caption, message_thread_id=message_thread_id)
        else:
            await upload_single(app, args.file, chat_id_list, caption, message_thread_id=message_thread_id)


if __name__ == "__main__":
    asyncio.run(main())