# -*- coding: utf-8 -*-
# @author: AbhiTheModder

# -------------------------------------------------------------------------------
#                                    IMPORTS
# -------------------------------------------------------------------------------
from os import environ
import argparse
from pyrogram import Client, enums
from pyrogram.types import InputMediaDocument
# -------------------------------------------------------------------------------
#                                    VARIABLES
# -------------------------------------------------------------------------------
API_HASH = environ.get("API_HASH")  # Get it from https://my.telegram.org
API_ID = int(environ.get("API_ID"))  # Get it from https://my.telegram.org
BOT_TOKEN = environ.get("BOT_TOKEN")  # Create a new bot from @BotFather and get it's token
# -------------------------------------------------------------------------------
#                                    FUNCTIONS
# -------------------------------------------------------------------------------
app = Client("Uploader", api_id=API_ID, api_hash=API_HASH, bot_token=BOT_TOKEN)


async def upload_single(file, chat_id_list, caption):
    async with app:
        for chat_id in chat_id_list:
            await app.send_document(
                chat_id=chat_id,
                document=file,
                caption=caption,
                parse_mode=enums.ParseMode.MARKDOWN,
            )
        print("Upload Successful!")


async def upload_group(files, chat_id_list, caption):
    async with app:
        media = []
        last_index = len(files) - 1
        for i, file in enumerate(files):
            media.append(InputMediaDocument(
                media=file,
                caption=caption if i == last_index else None,
                parse_mode=enums.ParseMode.MARKDOWN if i == last_index else None,
            ))
        for chat_id in chat_id_list:
            await app.send_media_group(chat_id=chat_id, media=media)
        print("Group Upload Successful!")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog="Uploader", description="Telegram file uploader")

    parser.add_argument("file", type=str, nargs="?", help="File to upload (single file mode)")
    parser.add_argument("--files", type=str, nargs="+", help="Files to upload as a grouped post")
    parser.add_argument("--chat-id", type=int, help="Chat ID(s) to upload the file to", required=True, nargs="+")
    parser.add_argument("--caption", type=str, help="Caption for the file", required=True)

    args = parser.parse_args()

    if args.files:
        app.run(upload_group(args.files, args.chat_id, args.caption))
    elif args.file:
        app.run(upload_single(args.file, args.chat_id, args.caption))
    else:
        parser.error("Either 'file' or '--files' must be provided")