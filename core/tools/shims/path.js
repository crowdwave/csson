export const resolve = (...a) => a.filter(Boolean).join("/");
export const dirname = (p) => String(p).replace(/\/[^/]*$/, "") || ".";
export const join = (...a) => a.join("/");
export const isAbsolute = (p) => String(p).startsWith("/");
export const sep = "/"; export const relative = (_a, b) => b;
export const parse = (p) => ({ dir: dirname(p), base: p });
export const basename = (p) => String(p).split("/").pop();
export default { resolve, dirname, join, isAbsolute, sep, relative, parse, basename };
