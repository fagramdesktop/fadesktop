# -*- coding: utf-8 -*-
# @author: AbhiTheModder

# -------------------------------------------------------------------------------
#                                    IMPORTS
# -------------------------------------------------------------------------------
from os import environ
import argparse
from pyrogram import Client, enums
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


async def main(file, chat_id_list, caption):
    async with app:
        for chat_id in chat_id_list:
            await app.send_document(
                chat_id=chat_id,
                document=file,
                caption=caption,
                parse_mode=enums.ParseMode.MARKDOWN,
            )
        print("Upload Successful!")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog="Uploader", description="Telegram file uploader")

    parser.add_argument("file", type=str, help="File to upload")
    parser.add_argument("--chat-id", type=int, help="Chat ID(s) to upload the file to", required=True, nargs="+")
    parser.add_argument("--caption", type=str, help="Caption for the file", required=True)

    args = parser.parse_args()

    app.run(main(args.file, args.chat_id, args.caption))