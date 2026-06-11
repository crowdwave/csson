import parse from 'css-tree/parser';
const OPTS = { parseRulePrelude: false, parseAtrulePrelude: false, parseValue: false, parseCustomProperty: false };
const seltype = (p) => ((p && p.type === 'Raw') ? p.value : '').replace(/^[&\s]+/, '').trim();
const rawValue = (d) => (d.value && d.value.type === 'Raw') ? d.value.value : '';
function coerce(raw){const s=raw.trim();
  if(/^(?:0|-?[1-9][0-9]*)$/.test(s)){const n=Number(s);return Number.isSafeInteger(n)?n:s;}
  if(s.length>=2&&s[0]==='"'&&s[s.length-1]==='"')return s.slice(1,-1); return s;}
function handle(child,o){
  if(child.type==='Declaration'&&child.property.startsWith('--')) o[child.property.slice(2)]=coerce(rawValue(child));
  else if(child.type==='Rule'){const ty=seltype(child.prelude);(o[ty]=o[ty]||[]).push(nodeOf(child));}
  else if(child.type==='Raw'){ parse(child.value,OPTS).children.forEach(c=>handle(c,o)); }
}
function nodeOf(rule){const o={};rule.block.children.forEach(c=>handle(c,o));return o;}
function canon(v){if(Array.isArray(v))return v.map(canon);
  if(v&&typeof v==='object'){const out={};for(const k of Object.keys(v).sort())out[k]=canon(v[k]);return out;}return v;}
export function cssonCanon(text){const ast=parse(text,OPTS);let r=null;
  ast.children.forEach(n=>{if(n.type==='Rule'&&seltype(n.prelude)==='cssonv1')r=nodeOf(n);});
  if(r===null)throw new Error('no root cssonv1 rule');return JSON.stringify(canon(r));}
