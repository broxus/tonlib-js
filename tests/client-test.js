const tl = require('..');
const msigAbi = JSON.stringify(require('./msig.abi.json'));

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

  fnGetCustodians() {
    return this.tonlibClient.send(new tl.FtabiGetFunction({
      abi: msigAbi,
      name: 'getCustodians'
    }));
  }

  generateKeyPair() {
    return this.tonlibClient.send(new tl.GenerateKeyPair());
  }

  runLocal(accountAddress, fn, call) {
    return this.tonlibClient.send(new tl.FtabiRunLocal({
      address: new tl.AccountAddress({accountAddress}),
      function: fn,
      call: new tl.FtabiFunctionCallJson({
        value: JSON.stringify(call)
      })
    }));
  }
}

const client = new TestClient();

(async () => {
  try {
    await client.init();

    const state = await client.getMasterchainInfo();
    console.log(state.last._props);

    const fnGetCustodians = await client.fnGetCustodians();
    console.log(fnGetCustodians._props);

    const keyPair = await client.generateKeyPair();
    console.error(keyPair._props);

    const result = await client.runLocal('0:2e4492152c323667733ba555ad0165642ae7e5e346e6b1077ab6866ae39a3dc3', fnGetCustodians, {
      header: {
        pubkey: Buffer.from(keyPair.publicKey).toString('base64')
      }
    });
    console.log(result);
  } catch (e) {
    console.error(e);
  }
})();
