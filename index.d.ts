declare class TonlibClient {
    constructor();

    send(request: object): void;

    receive(timeout: number): object;

    execute(request: object): object;
}
