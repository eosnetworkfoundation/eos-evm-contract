import os
import binascii
from datetime import datetime, timedelta
from telegram import Update
from telegram.helpers import escape_markdown
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes
import requests
import inflect
from web3 import Web3
from sqlitedict import SqliteDict
from loguru import logger
from dotenv import load_dotenv

load_dotenv()

TELEGRAM_TOKEN   = os.environ.get("TELEGRAM_TOKEN","")
TELEGRAM_CHAT_ID = int(os.environ.get("TELEGRAM_CHAT_ID",""))
PRIVATE_KEY      = os.environ.get("PRIVATE_KEY","")
ENDPOINT         = os.environ.get("ENDPOINT","")
DB_PATH          = os.environ.get("DB_PATH","data.db")
FIRSTAMOUNT      = float(os.environ.get("FIRSTAMOUNT","50"))
AMOUNT           = float(os.environ.get("AMOUNT","5"))
PERIOD           = int(os.environ.get("PERIOD","86400"))
CHAINID          = int(os.environ.get("CHAINID","15557"))
EXPLORER         = os.environ.get("EXPLORER","")

def pretty_time_delta(seconds, lang=inflect.engine()):
    if not seconds:
        return f"0 seconds"
    seconds = int(seconds)
    days, seconds = divmod(seconds, 86400)
    hours, seconds = divmod(seconds, 3600)
    minutes, seconds = divmod(seconds, 60)
    measures = (
        (days, "day"),
        (hours, "hour"),
        (minutes, "minute"),
        (seconds, "second"),
    )
    return lang.join(
        [f"{count} {lang.plural(noun, count)}" for (count, noun) in measures if count]
    )

async def testcoins(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:

    try:
        chat_id = update.message.chat_id
        if TELEGRAM_CHAT_ID and chat_id != TELEGRAM_CHAT_ID:
            logger.debug("Invalid chat id ({id})", id=chat_id)
            return

        if not len(context.args) > 0 or not Web3.is_address(context.args[0]):
            await update.message.reply_text(f'Please specify a valid address')
            return

        now = datetime.timestamp(datetime.utcnow())
        with SqliteDict(DB_PATH) as db:
            user_data = db.get(update.effective_user.id, None)
            if not user_data:
                to_send = FIRSTAMOUNT
                db[update.effective_user.id]=now
            else:
                a = db[update.effective_user.id]
                delta = (now - db[update.effective_user.id])
                if delta < PERIOD:
                    to_send = 0
                    next_request = pretty_time_delta(PERIOD - delta)
                    await update.message.reply_text(f'Next request in {next_request}')
                else:
                    to_send = AMOUNT
                    db[update.effective_user.id]=now

            if to_send == 0: return
            w3 = Web3(Web3.HTTPProvider(ENDPOINT))
            
            # HACK, fix get_transaction_count response in silkrpc
            nonce=0
            try:
                nonce = w3.eth.get_transaction_count(w3.eth.account.from_key(PRIVATE_KEY).address)
            except:
                pass

            rawtx = binascii.hexlify(w3.eth.account.sign_transaction(dict(
                gasPrice=Web3.to_wei(150, 'gwei'),
                nonce=nonce,
                gas=21000,
                to=Web3.to_checksum_address(context.args[0]),
                value=Web3.to_wei(to_send, 'ether'),
                data=b'',
            ), PRIVATE_KEY).rawTransaction).decode('utf8')

            # HACK, web3.py eth_sendRawTransaction does not work (json format error)
            txid = requests.post(ENDPOINT, json={
                "jsonrpc":"2.0",
                "method":"eth_sendRawTransaction",
                "params": ["0x"+rawtx],"id":1}).json()['result']

            logger.debug("sending {txid}", txid=txid)
            await update.message.reply_markdown(f'{escape_markdown(str(to_send))} EOS (testnet) [sent]({EXPLORER}/tx/{txid})')
            db.commit()
    except:
        logger.exception("Unexpected error occurred:")
        await update.message.reply_text(f'An error ocurred while sending the transaction')


app = ApplicationBuilder().token(TELEGRAM_TOKEN).build()
app.add_handler(CommandHandler("testcoins", testcoins))
app.run_polling()