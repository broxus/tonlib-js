//const tl = require('..');
const tl = require('../cmake-build-debug/lib/tonlib-js.abi-83');

const CONFIG = `{
  "liteservers": [
    {
      "ip": 916349379,
      "port": 3031,
      "id": {
        "@type": "pub.ed25519",
        "key": "uNRRL+6enQjuiZ/s6Z+vO7yxUUR7uxdfzIy+RxkECrc="
      }
    }
  ],
  "validator": {
     "@type": "validator.config.global",
     "zero_state": {
        "workchain": -1,
        "shard": -9223372036854775808,
        "seqno": 0,
        "root_hash": "WP/KGheNr/cF3lQhblQzyb0ufYUAcNM004mXhHq56EU=",
        "file_hash": "0nC4eylStbp9qnCq8KjDYb789NjS25L5ZA1UQwcIOOQ="
     }
  }
}`;

class TestClient {
  constructor() {
    this.tonlibClient = new tl.TonlibClient();
  }

  init() {
    return this.tonlibClient.send(new tl.Init({
      options: new tl.Options({
        config: new tl.Config({
          config: CONFIG,
          blockchainName: 'mainnet',
          useCallbacksForNetwork: false,
          ignoreCache: true
        }),
        keystoreType: new tl.KeyStoreTypeInMemory()
      })
    }));
  }

  getMasterchainInfo() {
    return this.tonlibClient.send(new tl.LiteServerGetMasterchainInfo());
  }
}

(async () => {
  const client = new TestClient();

  try {
    await client.init();

    const state = await client.getMasterchainInfo();
    console.log(state);
  } catch (e) {
    console.error(e);
  }
})();

process.stdin.resume();
