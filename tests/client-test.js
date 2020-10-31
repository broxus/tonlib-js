const {TonlibClient} = require('..');

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
    this.tonlibClient = new TonlibClient();
  }

  init() {
    this.tonlibClient.send({
      '@type': 'init',
      options: {
        config: {
          config: CONFIG,
          blockchain_name: 'mainnet'
        },
        keystore_type: {
          '@type': 'keyStoreTypeInMemory'
        }
      }
    })
  }

  getMasterchainInfo() {
    this.tonlibClient.send({
      '@type': 'liteServer.getMasterchainInfo'
    })
  }

  poll(timeout) {
    console.log(this.tonlibClient.receive(timeout));
  }
}

const client = new TestClient();
client.init();
client.getMasterchainInfo();

while (true) {
  console.log(client.poll(300));
}
