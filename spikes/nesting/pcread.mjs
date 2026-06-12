import postcss from 'postcss';
const seltype=(s)=>s.replace(/^[&\s]+/,'').trim();
function coerce(raw){const s=raw.trim();
  if(/^(?:0|-?[1-9][0-9]*)$/.test(s)){const n=Number(s);return Number.isSafeInteger(n)?n:s;}
  if(s.length>=2&&s[0]==='"'&&s[s.length-1]==='"')return s.slice(1,-1); return s;}
function nodeOf(rule){const o={};
  rule.nodes.forEach(n=>{
    if(n.type==='decl'&&n.prop.startsWith('--')) o[n.prop.slice(2)]=coerce(n.value);
    else if(n.type==='rule'){const ty=seltype(n.selector);(o[ty]=o[ty]||[]).push(nodeOf(n));}
  });return o;}
function canon(v){if(Array.isArray(v))return v.map(canon);
  if(v&&typeof v==='object'){const out={};for(const k of Object.keys(v).sort())out[k]=canon(v[k]);return out;}return v;}
export function cssonCanon(text){const root=postcss.parse(text);let r=null;
  root.nodes.forEach(n=>{if(n.type==='rule'&&seltype(n.selector)==='cssonv1')r=nodeOf(n);});
  if(r===null)throw new Error('no root');return JSON.stringify(canon(r));}
