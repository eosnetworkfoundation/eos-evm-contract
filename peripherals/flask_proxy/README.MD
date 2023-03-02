This proxy is a temporary solution for supporting batch requests to silkworm. It should be removed when the silkworm can support batch requests.

It is built with flask. So we use gunicorn to deploy it as flask server is not designed for prod. A Dockerfile is prepared in case we want to use docker.

Use env READ_ENDPOINT to config the endpoint.

## Build and Run with Docker
### Build
``` 
sudo docker build . -t flask_proxy
```
### Run


```
sudo docker run -eREAD_ENDPOINT=READ_ENDPOINT=http://xxx:xxx -d --log-driver=none --publish=5000:80 --name=flask_proxy --restart=always flask_proxy
```

## Run on host using gunicorn
```
gunicorn -eREAD_ENDPOINT=http://xxx:xxx -w4 -b0.0.0.0:5000 flask_proxy:app
```
