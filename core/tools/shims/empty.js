export default new Proxy({}, { get: () => () => undefined });
