import os
import json
from urllib3 import PoolManager
from flask import Flask, request, jsonify
from flask_cors import CORS
import json

from binascii import unhexlify
from urllib.parse import urlparse

readEndpoint=os.getenv('READ_ENDPOINT')
o = urlparse(readEndpoint)

app = Flask(__name__)
CORS(app)
pool = PoolManager(num_pools=2).connection_from_host(o.hostname,o.port)
@app.route("/", methods=["POST"])
def default():
    def forward_request(req):
        try:
            return json.loads(pool.request("POST", "/", body=json.dumps(req).encode('utf-8'), headers={"Content-Type":"application/json"}).data.decode('utf8'))
        except Exception as e:
            return {"error":str(e)}

    request_data = request.get_json()
    if type(request_data) == dict:
        return jsonify(forward_request(request_data))
    if len(request_data) > 4096:
        return "Entity too Large", 413
    res = []
    for r in request_data:
        res.append(forward_request(r))

    return jsonify(res)


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
