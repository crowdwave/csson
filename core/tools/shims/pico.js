const id = (s) => s; const c = new Proxy({}, { get: () => id });
export default c; export const createColors = () => c;
