import yargs from 'yargs';

export function args() {
   return yargs(process.argv.slice(2))
      .option('config', {
         alias: 'c',
         description: 'nginx config template path',
         type: 'string',
         nargs: 1,
         demandOption: true
      })
      .option('nginx', {
         alias: 'n',
         description: 'path to nginx',
         type: 'string',
         nargs: 1,
         demandOption: true
      })
      .option('address', {
         alias: 'a',
         description: 'address of staking contract to query',
         type: 'string',
         nargs: 1,
         demandOption: true
      })
      .option('rpc', {
         alias: 'r',
         description: 'RPC endpoint for queries',
         type: 'string',
         nargs: 1,
         demandOption: true
      })
      .option('fallback', {
         alias: 'f',
         description: 'upstream to use when no stakers',
         type: 'string',
         nargs: 1,
         demandOption: true
      })
      .option('e', {
         description: 'passed through to nginx -e argument',
         type: 'string',
         nargs: 1,
      })
      .option('g', {
         description: 'passed through to nginx -g argument',
         type: 'string',
         nargs: 1,
      })
      .option('p', {
         description: 'passed through to nginx -p argument',
         type: 'string',
         nargs: 1,
      })
      .help()
      .alias('help', 'h').argv;
}

