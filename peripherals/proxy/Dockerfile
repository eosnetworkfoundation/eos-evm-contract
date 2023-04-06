FROM openresty/openresty:alpine
ARG WRITE_ENDPOINT
ARG READ_ENDPOINT
ARG TEST_ENDPOINT
COPY nginx.conf /usr/local/openresty/nginx/conf/nginx.conf
RUN sed -i "s/WRITE_ENDPOINT/${WRITE_ENDPOINT}/g" /usr/local/openresty/nginx/conf/nginx.conf
RUN sed -i "s/READ_ENDPOINT/${READ_ENDPOINT}/g" /usr/local/openresty/nginx/conf/nginx.conf
RUN sed -i "s/TEST_ENDPOINT/${TEST_ENDPOINT}/g" /usr/local/openresty/nginx/conf/nginx.conf
COPY eth-jsonrpc-access.lua /usr/local/openresty/nginx/eth-jsonrpc-access.lua 
EXPOSE 80 443

