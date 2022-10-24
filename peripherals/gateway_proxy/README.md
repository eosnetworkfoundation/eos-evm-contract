## Gateway Proxy

The gateway proxy observes staked tokens and forwards requests to upstream providers based on the proportion of their stake. This is accomplished via a small Javascript application (runnable via nodejs) that periodically probes the staked amounts on chain and adjusts a "wrapped" nginx instance with the discovered upstream providers and their weights.

### Prerequisites

The gateway proxy requires nodejs 16+ and nginx. Remember to perform a `npm install` to source the required libraries. The testing suite requires a C++20 compiler along with boost 1.75+.

### Operation

A number of command line options are required
```
node main.mjs --help
Options:
      --version   Show version number                                  [boolean]
  -c, --config    nginx config template path                 [string] [required]
  -n, --nginx     path to nginx                              [string] [required]
  -a, --address   address of staking contract to query       [string] [required]
  -r, --rpc       RPC endpoint for queries                   [string] [required]
  -f, --fallback  upstream to use when no stakers            [string] [required]
  -e              passed through to nginx -e argument                   [string]
  -g              passed through to nginx -g argument                   [string]
  -p              passed through to nginx -p argument                   [string]
  -h, --help      Show help                                            [boolean]
```

An example innovation might be something like:
```
node main.mjs -a 0xaB9873C4c74469B0f724Cc4157D8F36EF32037aF \
              -c /home/user/nginx.conf                      \
              -n /usr/bin/nginx                             \
              -f 127.0.0.1:6000                             \
              -r http://127.0.0.1:7000
```
Note the unfortunate difference in now the fallback & RPC endpoint are defined.

In the above example, `/home/user/nginx.conf` will be used as the "templated" config (more on this later) handed to the nginx at `/usr/bin/nginx`. The address `0xaB9873C4c74469B0f724Cc4157D8F36EF32037aF` will be polled for stakers via the RPC endpoint at `http://127.0.0.1:7000`. Should there be no stakers, `127.0.0.1:6000` will be used as the upstream for all requests (be aware this could be treated as http or https based on your configuration, more on this later)

At startup, querying the RPC endpoint must succeed and nginx must confirm that the config file is valid. Otherwise, startup fails. Post startup, should the RPC endpoint go offline or the config file not be accepted, the gateway proxy will continue to attempt communication with the RPC endpoint and continue to attempt loading the config while leaving the prior configuration online.

The intent is for the gateway proxy to wrap an nginx instance. The `nginx` process will run as a child process to the `node` process. The `node` process will handle `SIGTERM` which will be forwarded to the `nginx` process and once `nginx` exits, `node` will too. The `node` process will handle `SIGHUP` which will reload the configuration template and then the new configuration will be communicated to `nginx`.

#### Configuration Template
At startup after discovering the current stakers, whenever the stakers change (or their resolved address changes), or on a SIGHUP sent to the `node` process, the contents of the config template is loaded and the string `$STAKERS` is replaced with text that would be appropriate for an nginx `upstream` section.

Additionally, a `daemon off;` is added at the end of the config, so no other `daemon` options may be present.

For example, consider a config template that looks like
```
events {}

http {
    upstream stakers {
$STAKERS
    }

    server {
        location / {
            proxy_pass https://stakers;

            proxy_ssl_verify on;
            proxy_ssl_session_reuse on;
            proxy_ssl_verify_depth 4;

            proxy_ssl_trusted_certificate /etc/ca-certificates/extracted/tls-ca-bundle.pem
        }
    }
}
```
This config template could be populated with something effectively like this
```
events {}

http {
    upstream stakers {
        server 172.253.122.99:443 weight=300000000;  # 0x1431226E230D7e46DCc78b0ad8C1c8FdeC6AA5C0
        server 142.251.163.104:443 weight=150000000;  # 0x3E15134f6413ef974cF050764A011CD8e4b6a8A7
    }

    server {
        location / {
            proxy_pass https://stakers;

            proxy_ssl_verify on;
            proxy_ssl_session_reuse on;
            proxy_ssl_verify_depth 4;

            proxy_ssl_trusted_certificate /etc/ca-certificates/extracted/tls-ca-bundle.pem
        }
    }
}
daemon off;
```

### Caveats & Limitations
The URLs found in the staking contract _must_ have a form of either `https://host.name.invalid/` or `https://host.name.invalid:8000/`. Anything after the trailing slash is ignored. These hostnames will be resolved to only a single IPv4 address before populating the config template. This has the side effect of eliminating SNI from being sent upstream.

All nginx upstreams must all be either http or https. Practically speaking this means all upstreams must be https. This might be inconvenient for the fallback upstream which will need to have a certificate trusted by the single `proxy_ssl_trusted_certificate` configuration.

If a staker has an invalid URL (either because the form is not as defined above, or because the DNS lookup fails), the staker is ignored.

If the staker has less than `minimumStakeUnit` as defined in `main.mjs`, the staker is ignored. This is currently set to 1 Szabo.
