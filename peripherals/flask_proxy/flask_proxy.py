#!/usr/bin/env python3
import random
import os
import json
import time
import calendar
from datetime import datetime

from flask import Flask, request, jsonify
from flask_cors import CORS
from eth_hash.auto import keccak
import requests
import json

from binascii import unhexlify

readEndpoint=os.getenv('READ_ENDPOINT')
app = Flask(__name__)
CORS(app)

@app.route("/", methods=["POST"])
def default():
    def forward_request(req):
        return requests.post(readEndpoint, json.dumps(req), headers={"Content-Type":"application/json"}).json()

    request_data = request.get_json()
    if type(request_data) == dict:
        return jsonify(forward_request(request_data))

    res = []
    for r in request_data:
        res.append(forward_request(r))

    return jsonify(res)


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
